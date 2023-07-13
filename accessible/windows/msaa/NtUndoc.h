/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NtUndoc_h
#define mozilla_NtUndoc_h

#include <winternl.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef STATUS_INFO_LENGTH_MISMATCH
#  define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#endif

#ifndef STATUS_BUFFER_TOO_SMALL
#  define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#endif

#ifndef STATUS_MORE_ENTRIES
#  define STATUS_MORE_ENTRIES ((NTSTATUS)0x00000105L)
#endif

enum UndocSystemInformationClass { SystemExtendedHandleInformation = 64 };

struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
  PVOID mObject;
  ULONG_PTR mPid;
  ULONG_PTR mHandle;
  ACCESS_MASK mGrantedAccess;
  USHORT mCreatorBackTraceIndex;
  USHORT mObjectTypeIndex;
  ULONG mAttributes;
  ULONG mReserved;
};

struct SYSTEM_HANDLE_INFORMATION_EX {
  ULONG_PTR mHandleCount;
  ULONG_PTR mReserved;
  SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX mHandles[1];
};

#ifndef __MINGW32__
enum UndocObjectInformationClass { ObjectNameInformation = 1 };

struct OBJECT_NAME_INFORMATION {
  UNICODE_STRING Name;
};
#endif

// The following declarations are documented on MSDN but are not included in
// public user-mode headers.

enum DirectoryObjectAccessFlags {
  DIRECTORY_QUERY = 0x0001,
  DIRECTORY_TRAVERSE = 0x0002,
  DIRECTORY_CREATE_OBJECT = 0x0004,
  DIRECTORY_CREATE_SUBDIRECTORY = 0x0008,
  DIRECTORY_ALL_ACCESS = STANDARD_RIGHTS_REQUIRED | 0x000F
};

NTSTATUS WINAPI NtOpenDirectoryObject(PHANDLE aDirectoryHandle,
                                      ACCESS_MASK aDesiredAccess,
                                      POBJECT_ATTRIBUTES aObjectAttributes);

struct OBJECT_DIRECTORY_INFORMATION {
  UNICODE_STRING mName;
  UNICODE_STRING mTypeName;
};

NTSTATUS WINAPI NtQueryDirectoryObject(HANDLE aDirectoryHandle,
                                       PVOID aOutBuffer, ULONG aBufferLength,
                                       BOOLEAN aReturnSingleEntry,
                                       BOOLEAN aRestartScan, PULONG aContext,
                                       PULONG aOutReturnLength);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // mozilla_NtUndoc_h
