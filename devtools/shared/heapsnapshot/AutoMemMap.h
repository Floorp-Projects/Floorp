/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_AutoMemMap_h
#define mozilla_devtools_AutoMemMap_h

#include <prio.h>
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "ErrorList.h"

namespace mozilla {
namespace devtools {

// # AutoMemMap
//
// AutoMemMap is an RAII class to manage mapping a file to memory. It is a
// wrapper aorund managing opening and closing a file and calling PR_MemMap and
// PR_MemUnmap.
//
// Example usage:
//
//     {
//       AutoMemMap mm;
//       if (NS_FAILED(mm.init("/path/to/desired/file"))) {
//          // Handle the error however you see fit.
//         return false;
//       }
//
//       doStuffWithMappedMemory(mm.address());
//     }
//     // The memory is automatically unmapped when the AutoMemMap leaves scope.
class MOZ_RAII AutoMemMap {
  PRFileInfo64 fileInfo;
  PRFileDesc* fd;
  PRFileMap* fileMap;
  void* addr;

  AutoMemMap(const AutoMemMap& aOther) = delete;
  void operator=(const AutoMemMap& aOther) = delete;

 public:
  explicit AutoMemMap()
      : fileInfo(), fd(nullptr), fileMap(nullptr), addr(nullptr){};
  ~AutoMemMap();

  // Initialize this AutoMemMap.
  nsresult init(const char* filePath, int flags = PR_RDONLY, int mode = 0,
                PRFileMapProtect prot = PR_PROT_READONLY);

  // Get the size of the memory mapped file.
  uint32_t size() const {
    MOZ_ASSERT(fileInfo.size <= UINT32_MAX,
               "Should only call size() if init() succeeded.");
    return uint32_t(fileInfo.size);
  }

  // Get the mapped memory.
  void* address() {
    MOZ_ASSERT(addr);
    return addr;
  }
  const void* address() const {
    MOZ_ASSERT(addr);
    return addr;
  }
};

}  // namespace devtools
}  // namespace mozilla

#endif  // mozilla_devtools_AutoMemMap_h
