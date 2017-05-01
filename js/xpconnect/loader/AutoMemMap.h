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
#include "mozilla/ipc/FileDescriptor.h"
#include "nsIMemoryReporter.h"

#include <prio.h>

class nsIFile;

namespace mozilla {
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
        init(const ipc::FileDescriptor& file);

        bool initialized() { return addr; }

        uint32_t size() const { MOZ_ASSERT(fd); return size_; }

        template<typename T = void>
        const RangedPtr<T> get()
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

        FileDescriptor cloneFileDescriptor();

    private:
        Result<Ok, nsresult> initInternal(PRFileMapProtect prot = PR_PROT_READONLY);

        AutoFDClose fd;
        PRFileMap* fileMap = nullptr;

        uint32_t size_ = 0;
        void* addr = nullptr;

        AutoMemMap(const AutoMemMap&) = delete;
        void operator=(const AutoMemMap&) = delete;
};

} // namespace loader
} // namespace mozilla

#endif // loader_AutoMemMap_h
