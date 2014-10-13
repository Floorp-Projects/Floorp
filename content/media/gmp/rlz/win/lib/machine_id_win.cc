// Copyright 2011 Google Inc. All Rights Reserved.
// Use of this source code is governed by an Apache-style license that can be
// found in the COPYING file.

#include <windows.h>
#include <Sddl.h>  // For ConvertSidToStringSidW.
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "rlz/lib/assert.h"

namespace rlz_lib {

namespace {

bool GetSystemVolumeSerialNumber(int* number) {
  if (!number)
    return false;

  *number = 0;

  // Find the system root path (e.g: C:\).
  wchar_t system_path[MAX_PATH + 1];
  if (!GetSystemDirectoryW(system_path, MAX_PATH))
    return false;

  wchar_t* first_slash = wcspbrk(system_path, L"\\/");
  if (first_slash != NULL)
    *(first_slash + 1) = 0;

  DWORD number_local = 0;
  if (!GetVolumeInformationW(system_path, NULL, 0, &number_local, NULL, NULL,
                             NULL, 0))
    return false;

  *number = (int)number_local;
  return true;
}

bool GetComputerSid(const wchar_t* account_name, SID* sid, DWORD sid_size) {
  static const DWORD kStartDomainLength = 128;  // reasonable to start with

  scoped_array<wchar_t> domain_buffer(new wchar_t[kStartDomainLength]);
  DWORD domain_size = kStartDomainLength;
  DWORD sid_dword_size = sid_size;
  SID_NAME_USE sid_name_use;

  BOOL success = ::LookupAccountNameW(NULL, account_name, sid,
                                      &sid_dword_size, domain_buffer.get(),
                                      &domain_size, &sid_name_use);
  if (!success && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    // We could have gotten the insufficient buffer error because
    // one or both of sid and szDomain was too small. Check for that
    // here.
    if (sid_dword_size > sid_size)
      return false;

    if (domain_size > kStartDomainLength)
      domain_buffer.reset(new wchar_t[domain_size]);

    success = ::LookupAccountNameW(NULL, account_name, sid, &sid_dword_size,
                                   domain_buffer.get(), &domain_size,
                                   &sid_name_use);
  }

  return success != FALSE;
}

std::wstring ConvertSidToString(SID* sid) {
  std::wstring sid_string;
#if _WIN32_WINNT >= 0x500
  wchar_t* sid_buffer = NULL;
  if (ConvertSidToStringSidW(sid, &sid_buffer)) {
    sid_string = sid_buffer;
    LocalFree(sid_buffer);
  }
#else
  SID_IDENTIFIER_AUTHORITY* sia = ::GetSidIdentifierAuthority(sid);

  if(sia->Value[0] || sia->Value[1]) {
    base::SStringPrintf(
        &sid_string, L"S-%d-0x%02hx%02hx%02hx%02hx%02hx%02hx",
        SID_REVISION, (USHORT)sia->Value[0], (USHORT)sia->Value[1],
        (USHORT)sia->Value[2], (USHORT)sia->Value[3], (USHORT)sia->Value[4],
        (USHORT)sia->Value[5]);
  } else {
    ULONG authority = 0;
    for (int i = 2; i < 6; ++i) {
      authority <<= 8;
      authority |= sia->Value[i];
    }
    base::SStringPrintf(&sid_string, L"S-%d-%lu", SID_REVISION, authority);
  }

  int sub_auth_count = *::GetSidSubAuthorityCount(sid);
  for(int i = 0; i < sub_auth_count; ++i)
    base::StringAppendF(&sid_string, L"-%lu", *::GetSidSubAuthority(sid, i));
#endif

  return sid_string;
}

}  // namespace

bool GetRawMachineId(string16* sid_string, int* volume_id) {
  // Calculate the Windows SID.

  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1] = {0};
  DWORD size = arraysize(computer_name);

  if (!GetComputerNameW(computer_name, &size)) {
    return false;
  }
  char sid_buffer[SECURITY_MAX_SID_SIZE];
  SID* sid = reinterpret_cast<SID*>(sid_buffer);
  if (GetComputerSid(computer_name, sid, SECURITY_MAX_SID_SIZE)) {
    *sid_string = ConvertSidToString(sid);
  }

  // Get the system drive volume serial number.
  *volume_id = 0;
  if (!GetSystemVolumeSerialNumber(volume_id)) {
    ASSERT_STRING("GetMachineId: Failed to retrieve volume serial number");
    *volume_id = 0;
  }

  return true;
}

}  // namespace rlz_lib
