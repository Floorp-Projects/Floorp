/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NtUndoc_h
#define mozilla_NtUndoc_h

#include <winternl.h>

#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#endif

enum UndocSystemInformationClass
{
  SystemExtendedHandleInformation = 64
};

struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
{
  PVOID       mObject;
  ULONG_PTR   mPid;
  ULONG_PTR   mHandle;
  ACCESS_MASK mGrantedAccess;
  USHORT      mCreatorBackTraceIndex;
  USHORT      mObjectTypeIndex;
  ULONG       mAttributes;
  ULONG       mReserved;
};

struct SYSTEM_HANDLE_INFORMATION_EX
{
  ULONG_PTR                         mHandleCount;
  ULONG_PTR                         mReserved;
  SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX mHandles[1];
};

enum UndocObjectInformationClass
{
  ObjectNameInformation = 1
};

struct OBJECT_NAME_INFORMATION
{
  UNICODE_STRING  mName;
};

#endif // mozilla_NtUndoc_h
