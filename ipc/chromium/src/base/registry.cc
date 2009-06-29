// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// All Rights Reserved.

#include "base/registry.h"

#include <assert.h>
#include <shlwapi.h>
#include <windows.h>

#pragma comment(lib, "shlwapi.lib")  // for SHDeleteKey

// local types (see the same declarations in the header file)
#define tchar TCHAR
#define CTP const tchar*
#define tstr std::basic_string<tchar>

//
// RegistryValueIterator
//

RegistryValueIterator::RegistryValueIterator(HKEY root_key,
                                             LPCTSTR folder_key) {
  LONG result = RegOpenKeyEx(root_key, folder_key, 0, KEY_READ, &key_);
  if (result != ERROR_SUCCESS) {
    key_ = NULL;
  } else {
    DWORD count = 0;
    result = ::RegQueryInfoKey(key_, NULL, 0, NULL, NULL, NULL, NULL, &count,
                               NULL, NULL, NULL, NULL);

    if (result != ERROR_SUCCESS) {
      ::RegCloseKey(key_);
      key_ = NULL;
    } else {
      index_ = count - 1;
    }
  }

  Read();
}

RegistryValueIterator::~RegistryValueIterator() {
  if (key_)
    ::RegCloseKey(key_);
}

bool RegistryValueIterator::Valid() const {
  // true while the iterator is valid
  return key_ != NULL && index_ >= 0;
}

void RegistryValueIterator::operator ++ () {
  // advance to the next entry in the folder
  --index_;
  Read();
}

bool RegistryValueIterator::Read() {
  if (Valid()) {
    DWORD ncount = sizeof(name_)/sizeof(*name_);
    value_size_ = sizeof(value_);
    LRESULT r = ::RegEnumValue(key_, index_, name_, &ncount, NULL, &type_,
                               reinterpret_cast<BYTE*>(value_), &value_size_);
    if (ERROR_SUCCESS == r)
      return true;
  }

  name_[0] = '\0';
  value_[0] = '\0';
  value_size_ = 0;
  return false;
}

DWORD RegistryValueIterator::ValueCount() const {

  DWORD count = 0;
  HRESULT result = ::RegQueryInfoKey(key_, NULL, 0, NULL, NULL, NULL, NULL,
                                     &count, NULL, NULL, NULL, NULL);

  if (result != ERROR_SUCCESS)
    return 0;

  return count;
}

//
// RegistryKeyIterator
//

RegistryKeyIterator::RegistryKeyIterator(HKEY root_key,
                                         LPCTSTR folder_key) {
  LONG result = RegOpenKeyEx(root_key, folder_key, 0, KEY_READ, &key_);
  if (result != ERROR_SUCCESS) {
    key_ = NULL;
  } else {
    DWORD count = 0;
    HRESULT result = ::RegQueryInfoKey(key_, NULL, 0, NULL, &count, NULL, NULL,
                                       NULL, NULL, NULL, NULL, NULL);

    if (result != ERROR_SUCCESS) {
      ::RegCloseKey(key_);
      key_ = NULL;
    } else {
      index_ = count - 1;
    }
  }

  Read();
}

RegistryKeyIterator::~RegistryKeyIterator() {
  if (key_)
    ::RegCloseKey(key_);
}

bool RegistryKeyIterator::Valid() const {
  // true while the iterator is valid
  return key_ != NULL && index_ >= 0;
}

void RegistryKeyIterator::operator ++ () {
  // advance to the next entry in the folder
  --index_;
  Read();
}

bool RegistryKeyIterator::Read() {
  if (Valid()) {
    DWORD ncount = sizeof(name_)/sizeof(*name_);
    FILETIME written;
    LRESULT r = ::RegEnumKeyEx(key_, index_, name_, &ncount, NULL, NULL,
                               NULL, &written);
    if (ERROR_SUCCESS == r)
      return true;
  }

  name_[0] = '\0';
  return false;
}

DWORD RegistryKeyIterator::SubkeyCount() const {

  DWORD count = 0;
  HRESULT result = ::RegQueryInfoKey(key_, NULL, 0, NULL, &count, NULL, NULL,
                                     NULL, NULL, NULL, NULL, NULL);

  if (result != ERROR_SUCCESS)
    return 0;

  return count;
}

//
// RegKey
//

RegKey::RegKey(HKEY rootkey, const tchar* subkey, REGSAM access)
  : key_(NULL), watch_event_(0) {
  if (rootkey) {
    if (access & (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK))
      this->Create(rootkey, subkey, access);
    else
      this->Open(rootkey, subkey, access);
  }
  else assert(!subkey);
}

void RegKey::Close() {
  StopWatching();
  if (key_) {
    ::RegCloseKey(key_);
    key_ = NULL;
  }
}

bool RegKey::Create(HKEY rootkey, const tchar* subkey, REGSAM access) {
  DWORD disposition_value;
  return CreateWithDisposition(rootkey, subkey, &disposition_value, access);
}

bool RegKey::CreateWithDisposition(HKEY rootkey, const tchar* subkey,
                                   DWORD* disposition, REGSAM access) {
  assert(rootkey && subkey && access && disposition);
  this->Close();

  LONG const result = RegCreateKeyEx(rootkey,
                                     subkey,
                                     0,
                                     NULL,
                                     REG_OPTION_NON_VOLATILE,
                                     access,
                                     NULL,
                                     &key_,
                                     disposition );
  if (result != ERROR_SUCCESS) {
    key_ = NULL;
    return false;
  }
  else return true;
}

bool RegKey::Open(HKEY rootkey, const tchar* subkey, REGSAM access) {
  assert(rootkey && subkey && access);
  this->Close();

  LONG const result = RegOpenKeyEx(rootkey, subkey, 0,
                                   access, &key_ );
  if (result != ERROR_SUCCESS) {
    key_ = NULL;
    return false;
  }
  else return true;
}

bool RegKey::CreateKey(const tchar* name, REGSAM access) {
  assert(name && access);

  HKEY subkey = NULL;
  LONG const result = RegCreateKeyEx(key_, name, 0, NULL,
                                     REG_OPTION_NON_VOLATILE,
                                     access, NULL, &subkey, NULL);
  this->Close();

  key_ = subkey;
  return (result == ERROR_SUCCESS);
}

bool RegKey::OpenKey(const tchar* name, REGSAM access) {
  assert(name && access);

  HKEY subkey = NULL;
  LONG const result = RegOpenKeyEx(key_, name, 0, access, &subkey);

  this->Close();

  key_ = subkey;
  return (result == ERROR_SUCCESS);
}

DWORD RegKey::ValueCount() {
  DWORD count = 0;
  HRESULT const result = ::RegQueryInfoKey(key_, NULL, 0, NULL, NULL, NULL,
                                     NULL, &count, NULL, NULL, NULL, NULL);
  return (result != ERROR_SUCCESS) ? 0 : count;
}

bool RegKey::ReadName(int index, tstr* name) {
  tchar buf[256];
  DWORD bufsize = sizeof(buf)/sizeof(*buf);
  LRESULT r = ::RegEnumValue(key_, index, buf, &bufsize, NULL, NULL,
                             NULL, NULL);
  if (r != ERROR_SUCCESS)
    return false;
  if (name)
    *name = buf;
  return true;
}

bool RegKey::ValueExists(const tchar* name) {
  if (!key_) return false;
  const HRESULT result = RegQueryValueEx(key_, name, 0, NULL, NULL, NULL);
  return (result == ERROR_SUCCESS);
}

bool RegKey::ReadValue(const tchar* name, void* data,
                       DWORD* dsize, DWORD* dtype) {
  if (!key_) return false;
  HRESULT const result = RegQueryValueEx(key_, name, 0, dtype,
                                         reinterpret_cast<LPBYTE>(data),
                                         dsize);
  return (result == ERROR_SUCCESS);
}

bool RegKey::ReadValue(const tchar* name, tstr * value) {
  assert(value);
  static const size_t kMaxStringLength = 1024;  // This is after expansion.
  // Use the one of the other forms of ReadValue if 1024 is too small for you.
  TCHAR raw_value[kMaxStringLength];
  DWORD type = REG_SZ, size = sizeof(raw_value);
  if (this->ReadValue(name, raw_value, &size, &type)) {
    if (type == REG_SZ) {
      *value = raw_value;
    } else if (type == REG_EXPAND_SZ) {
      TCHAR expanded[kMaxStringLength];
      size = ExpandEnvironmentStrings(raw_value, expanded, kMaxStringLength);
      // Success: returns the number of TCHARs copied
      // Fail: buffer too small, returns the size required
      // Fail: other, returns 0
      if (size == 0 || size > kMaxStringLength)
        return false;
      *value = expanded;
    } else {
      // Not a string. Oops.
      return false;
    }
    return true;
  }
  else return false;
}

bool RegKey::ReadValueDW(const tchar* name, DWORD * value) {
  assert(value);
  DWORD type = REG_DWORD, size = sizeof(DWORD), result = 0;
  if (this->ReadValue(name, &result, &size, &type)
     && (type == REG_DWORD || type == REG_BINARY)
     && size == sizeof(DWORD)) {
    *value = result;
    return true;
  }
  else return false;
}

bool RegKey::WriteValue(const tchar* name,
                        const void * data,
                        DWORD dsize,
                        DWORD dtype) {
  assert(data);
  if (!key_) return false;
  HRESULT const result = RegSetValueEx(
      key_,
      name,
      0,
      dtype,
      reinterpret_cast<LPBYTE>(const_cast<void*>(data)),
      dsize);
  return (result == ERROR_SUCCESS);
}

bool RegKey::WriteValue(const tchar * name, const tchar * value) {
  return this->WriteValue(name, value,
    static_cast<DWORD>(sizeof(*value) * (_tcslen(value) + 1)), REG_SZ);
}

bool RegKey::WriteValue(const tchar * name, DWORD value) {
  return this->WriteValue(name, &value,
    static_cast<DWORD>(sizeof(value)), REG_DWORD);
}

bool RegKey::DeleteKey(const tchar * name) {
  if (!key_) return false;
  return (ERROR_SUCCESS == SHDeleteKey(key_, name));
}


bool RegKey::DeleteValue(const tchar * value_name) {
  assert(value_name);
  HRESULT const result = RegDeleteValue(key_, value_name);
  return (result == ERROR_SUCCESS);
}

bool RegKey::StartWatching() {
  if (!watch_event_)
    watch_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);

  DWORD filter = REG_NOTIFY_CHANGE_NAME |
                 REG_NOTIFY_CHANGE_ATTRIBUTES |
                 REG_NOTIFY_CHANGE_LAST_SET |
                 REG_NOTIFY_CHANGE_SECURITY;

  // Watch the registry key for a change of value.
  HRESULT result = RegNotifyChangeKeyValue(key_, TRUE, filter,
                                           watch_event_, TRUE);
  if (SUCCEEDED(result)) {
    return true;
  } else {
    CloseHandle(watch_event_);
    watch_event_ = 0;
    return false;
  }
}

bool RegKey::StopWatching() {
  if (watch_event_) {
    CloseHandle(watch_event_);
    watch_event_ = 0;
    return true;
  }
  return false;
}

bool RegKey::HasChanged() {
  if (watch_event_) {
    if (WaitForSingleObject(watch_event_, 0) == WAIT_OBJECT_0) {
      StartWatching();
      return true;
    }
  }
  return false;
}

// Register a COM object with the most usual properties.
bool RegisterCOMServer(const tchar* guid,
                       const tchar* name,
                       const tchar* path) {
  RegKey key(HKEY_CLASSES_ROOT, _T("CLSID"), KEY_WRITE);
  key.CreateKey(guid, KEY_WRITE);
  key.WriteValue(NULL, name);
  key.CreateKey(_T("InprocServer32"), KEY_WRITE);
  key.WriteValue(NULL, path);
  key.WriteValue(_T("ThreadingModel"), _T("Apartment"));
  return true;
}

bool RegisterCOMServer(const tchar* guid, const tchar* name, HINSTANCE module) {
  tchar module_path[MAX_PATH];
  ::GetModuleFileName(module, module_path, MAX_PATH);
  _tcslwr_s(module_path, MAX_PATH);
  return RegisterCOMServer(guid, name, module_path);
}

bool UnregisterCOMServer(const tchar* guid) {
  RegKey key(HKEY_CLASSES_ROOT, _T("CLSID"), KEY_WRITE);
  key.DeleteKey(guid);
  return true;
}
