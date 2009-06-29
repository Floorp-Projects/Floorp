// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// All Rights Reserved.

#ifndef BASE_REGISTRY_H__
#define BASE_REGISTRY_H__

#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
#include <string>

// The shared file uses a bunch of header files that define types that we don't.
// To avoid changing much code from the standard version, and also to avoid
// polluting our namespace with extra types we don't want, we define these types
// here with the preprocessor and undefine them at the end of the file.
#define tchar TCHAR
#define CTP const tchar*
#define tstr std::basic_string<tchar>

// RegKey
// Utility class to read from and manipulate the registry.
// Registry vocabulary primer: a "key" is like a folder, in which there
// are "values", which are <name,data> pairs, with an associated data type.

class RegKey {
 public:
  RegKey(HKEY rootkey = NULL, CTP subkey = NULL, REGSAM access = KEY_READ);
    // start there

  ~RegKey() { this->Close(); }

  bool Create(HKEY rootkey, CTP subkey, REGSAM access = KEY_READ);

  bool CreateWithDisposition(HKEY rootkey, CTP subkey, DWORD* disposition,
                             REGSAM access = KEY_READ);

  bool Open(HKEY rootkey, CTP subkey, REGSAM access = KEY_READ);

  // Create a subkey (or open if exists)
  bool CreateKey(CTP name, REGSAM access);

  // Open a subkey
  bool OpenKey(CTP name, REGSAM access);

  // all done, eh?
  void Close();

  DWORD ValueCount();  // Count of the number of value extant

  bool ReadName(int index, tstr* name);  // Determine the Nth value's name

  // True while the key is valid
  bool Valid() const { return NULL != key_; }

  // Kill key and everything that liveth below it; please be careful out there
  bool DeleteKey(CTP name);

  // Delete a single value within the key
  bool DeleteValue(CTP name);

  bool ValueExists(CTP name);
  bool ReadValue(CTP name, void * data, DWORD * dsize, DWORD * dtype = NULL);
  bool ReadValue(CTP name, tstr * value);
  bool ReadValueDW(CTP name, DWORD * value);  // Named to differ from tstr*

  bool WriteValue(CTP name, const void * data, DWORD dsize,
                  DWORD dtype = REG_BINARY);
  bool WriteValue(CTP name, CTP value);
  bool WriteValue(CTP name, DWORD value);

  // StartWatching()
  //   Start watching the key to see if any of its values have changed.
  //   The key must have been opened with the KEY_NOTIFY access
  //   privelege.
  bool StartWatching();

  // HasChanged()
  //   If StartWatching hasn't been called, always returns false.
  //   Otherwise, returns true if anything under the key has changed.
  //   This can't be const because the watch_event_ may be refreshed.
  bool HasChanged();

  // StopWatching()
  //   Will automatically be called by destructor if not manually called
  //   beforehand.  Returns true if it was watching, false otherwise.
  bool StopWatching();

  inline bool IsWatching() const { return watch_event_ != 0; }
  HANDLE watch_event() const { return watch_event_; }
  HKEY Handle() const { return key_; }

 private:
  HKEY  key_;    // the registry key being iterated
  HANDLE watch_event_;
};


// Standalone registry functions -- sorta deprecated, they now map to
// using RegKey


// Add a raw data to the registry -- you can pass NULL for the data if
// you just want to create a key
inline bool AddToRegistry(HKEY root_key, CTP key, CTP value_name,
                          void const * data, DWORD dsize,
                          DWORD dtype = REG_BINARY) {
  return RegKey(root_key, key, KEY_WRITE).WriteValue(value_name, data, dsize,
                                                     dtype);
}

// Convenience routine to add a string value to the registry
inline bool AddToRegistry(HKEY root_key, CTP key, CTP value_name, CTP value) {
  return AddToRegistry(root_key, key, value_name, value,
                       sizeof(*value) * (lstrlen(value) + 1), REG_SZ);
}

// Read raw data from the registry -- pass something as the dtype
// parameter if you care to learn what type the value was stored as
inline bool ReadFromRegistry(HKEY root_key, CTP key, CTP value_name,
                             void* data, DWORD* dsize, DWORD* dtype = NULL) {
  return RegKey(root_key, key).ReadValue(value_name, data, dsize, dtype);
}


// Delete a value or a key from the registry
inline bool DeleteFromRegistry(HKEY root_key, CTP subkey, CTP value_name) {
  if (value_name)
    return ERROR_SUCCESS == ::SHDeleteValue(root_key, subkey, value_name);
  else
    return ERROR_SUCCESS == ::SHDeleteKey(root_key, subkey);
}



// delete a key and all subkeys from the registry
inline bool DeleteKeyFromRegistry(HKEY root_key, CTP key_path, CTP key_name) {
  RegKey key;
  return key.Open(root_key, key_path, KEY_WRITE)
      && key.DeleteKey(key_name);
}


// Iterates the entries found in a particular folder on the registry.
// For this application I happen to know I wont need data size larger
// than MAX_PATH, but in real life this wouldn't neccessarily be
// adequate.
class RegistryValueIterator {
 public:
  // Specify a key in construction
  RegistryValueIterator(HKEY root_key, LPCTSTR folder_key);

  ~RegistryValueIterator();

  DWORD ValueCount() const;  // count of the number of subkeys extant

  bool Valid() const;  // true while the iterator is valid

  void operator++();  // advance to the next entry in the folder

  // The pointers returned by these functions are statics owned by the
  // Name and Value functions
  CTP Name() const { return name_; }
  CTP Value() const { return value_; }
  DWORD ValueSize() const { return value_size_; }
  DWORD Type() const { return type_; }

  int Index() const { return index_; }

 private:
  bool Read();   // read in the current values

  HKEY  key_;    // the registry key being iterated
  int   index_;  // current index of the iteration

  // Current values
  TCHAR name_[MAX_PATH];
  TCHAR value_[MAX_PATH];
  DWORD value_size_;
  DWORD type_;
};


class RegistryKeyIterator {
 public:
  // Specify a parent key in construction
  RegistryKeyIterator(HKEY root_key, LPCTSTR folder_key);

  ~RegistryKeyIterator();

  DWORD SubkeyCount() const;  // count of the number of subkeys extant

  bool Valid() const;  // true while the iterator is valid

  void operator++();  // advance to the next entry in the folder

  // The pointer returned by Name() is a static owned by the function
  CTP Name() const { return name_; }

  int Index() const { return index_; }

 private:
  bool Read();   // read in the current values

  HKEY  key_;    // the registry key being iterated
  int   index_;  // current index of the iteration

  // Current values
  TCHAR name_[MAX_PATH];
};


// Register a COM object with the most usual properties.
bool RegisterCOMServer(const tchar* guid, const tchar* name,
                       const tchar* modulepath);
bool RegisterCOMServer(const tchar* guid, const tchar* name, HINSTANCE module);
bool UnregisterCOMServer(const tchar* guid);

// undo the local types defined above
#undef tchar
#undef CTP
#undef tstr

#endif  // BASE_REGISTRY_H__
