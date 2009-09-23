// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares a helpers to intercept functions from a DLL.
//
// This set of functions are designed to intercept functions for a
// specific DLL imported from another DLL. This is the case when,
// for example, we want to intercept CertDuplicateCertificateContext
// function (exported from crypt32.dll) called by wininet.dll.

#ifndef BASE_IAT_PATCH_H__
#define BASE_IAT_PATCH_H__

#include <windows.h>
#include "base/basictypes.h"
#include "base/pe_image.h"

namespace iat_patch {

// Helper to intercept a function in an import table of a specific
// module.
//
// Arguments:
// module_handle          Module to be intercepted
// imported_from_module   Module that exports the symbol
// function_name          Name of the API to be intercepted
// new_function           Interceptor function
// old_function           Receives the original function pointer
// iat_thunk              Receives pointer to IAT_THUNK_DATA
//                        for the API from the import table.
//
// Returns: Returns NO_ERROR on success or Windows error code
//          as defined in winerror.h
//
DWORD InterceptImportedFunction(HMODULE module_handle,
                                const char* imported_from_module,
                                const char* function_name,
                                void* new_function,
                                void** old_function,
                                IMAGE_THUNK_DATA** iat_thunk);

// Restore intercepted IAT entry with the original function.
//
// Arguments:
// intercept_function     Interceptor function
// original_function      Receives the original function pointer
//
// Returns: Returns NO_ERROR on success or Windows error code
//          as defined in winerror.h
//
DWORD RestoreImportedFunction(void* intercept_function,
                              void* original_function,
                              IMAGE_THUNK_DATA* iat_thunk);

// Change the page protection (of code pages) to writable and copy
// the data at the specified location
//
// Arguments:
// old_code               Target location to copy
// new_code               Source
// length                 Number of bytes to copy
//
// Returns: Windows error code (winerror.h). NO_ERROR if successful
//
DWORD ModifyCode(void* old_code,
                 void* new_code,
                 int length);

// A class that encapsulates IAT patching helpers and restores
// the original function in the destructor.
class IATPatchFunction {
 public:
  IATPatchFunction();
  ~IATPatchFunction();

  // Intercept a function in an import table of a specific
  // module. Save the original function and the import
  // table address. These values will be used later
  // during Unpatch
  //
  // Arguments:
  // module                 Module to be intercepted
  // imported_from_module   Module that exports the 'function_name'
  // function_name          Name of the API to be intercepted
  //
  // Returns: Windows error code (winerror.h). NO_ERROR if successful
  //
  // Note: Patching a function will make the IAT patch take some "ownership" on
  // |module|.  It will LoadLibrary(module) to keep the DLL alive until a call
  // to Unpatch(), which will call FreeLibrary() and allow the module to be
  // unloaded.  The idea is to help prevent the DLL from going away while a
  // patch is still active.
  //
  DWORD Patch(const wchar_t* module,
              const char* imported_from_module,
              const char* function_name,
              void* new_function);

  // Unpatch the IAT entry using internally saved original
  // function.
  //
  // Returns: Windows error code (winerror.h). NO_ERROR if successful
  //
  DWORD Unpatch();

  bool is_patched() const {
    return (NULL != intercept_function_);
  }

 private:
  HMODULE module_handle_;
  void* intercept_function_;
  void* original_function_;
  IMAGE_THUNK_DATA* iat_thunk_;

  DISALLOW_EVIL_CONSTRUCTORS(IATPatchFunction);
};

}  // namespace iat_patch

#endif  // BASE_IAT_PATCH_H__
