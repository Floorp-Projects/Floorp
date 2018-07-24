/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef loader_AutoMemMap_h
#define loader_AutoMemMap_h

#include "mozilla/FileUtils.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/Result.h"
#include "nsIMemoryReporter.h"

#include <prio.h>

class nsIFile;

namespace mozilla {
namespace ipc {
  class FileDescriptor;
}

namespace loader {

using mozilla::ipc::FileDescriptor;

class AutoMemMap
{
    public:
        AutoMemMap() = default;

        ~AutoMemMap();

        Result<Ok, nsresult>
        init(nsIFile* file, int flags = PR_RDONLY, int mode = 0,
             PRFileMapProtect prot = PR_PROT_READONLY);

        Result<Ok, nsresult>
        init(const ipc::FileDescriptor& file,
             PRFileMapProtect prot = PR_PROT_READONLY,
             size_t expectedSize = 0);

        // Initializes the mapped memory with a shared memory handle. On
        // Unix-like systems, this is identical to the above init() method. On
        // Windows, the FileDescriptor must be a handle for a file mapping,
        // rather than a file descriptor.
        Result<Ok, nsresult>
        initWithHandle(const ipc::FileDescriptor& file, size_t size,
                       PRFileMapProtect prot = PR_PROT_READONLY);

        void reset();

        bool initialized() const { return addr; }

        uint32_t size() const { return size_; }

        template<typename T = void>
        RangedPtr<T> get()
        {
            MOZ_ASSERT(addr);
            return { static_cast<T*>(addr), size_ };
        }

        template<typename T = void>
        const RangedPtr<T> get() const
        {
            MOZ_ASSERT(addr);
            return { static_cast<T*>(addr), size_ };
        }

        size_t nonHeapSizeOfExcludingThis() { return size_; }

        FileDescriptor cloneFileDescriptor() const;
        FileDescriptor cloneHandle() const;

        // Makes this mapping persistent. After calling this, the mapped memory
        // will remained mapped, even after this instance is destroyed.
        void setPersistent() { persistent_ = true; }

    private:
        Result<Ok, nsresult> initInternal(PRFileMapProtect prot = PR_PROT_READONLY,
                                          size_t expectedSize = 0);

        AutoFDClose fd;
        PRFileMap* fileMap = nullptr;

#ifdef XP_WIN
        // We can't include windows.h in this header, since it gets included
        // by some binding headers (which are explicitly incompatible with
        // windows.h). So we can't use the HANDLE type here.
        void* handle_ = nullptr;
#endif

        uint32_t size_ = 0;
        void* addr = nullptr;

        bool persistent_ = 0;

        AutoMemMap(const AutoMemMap&) = delete;
        void operator=(const AutoMemMap&) = delete;
};

} // namespace loader
} // namespace mozilla

#endif // loader_AutoMemMap_h
