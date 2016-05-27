/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ds_PageProtectingVector_h
#define ds_PageProtectingVector_h

#include "mozilla/Vector.h"

#include "gc/Memory.h"

namespace js {

/*
 * PageProtectingVector is a vector that can only grow or be cleared, and marks
 * all of its fully used memory pages as read-only. It can be used to detect
 * heap corruption in important buffers, since anything that tries to write
 * into its protected pages will crash.
 *
 * PageProtectingVector's protection is limited to full pages. If the front
 * of its buffer is not aligned on a page boundary, bytes preceding the first
 * page boundary will not be protected. Similarly, the end of the buffer will
 * not be fully protected unless it is aligned on a page boundary. Altogether,
 * up to two pages of memory may not be protected.
 */
template<typename T,
         size_t MinInlineCapacity = 0,
         class AllocPolicy = mozilla::MallocAllocPolicy>
class PageProtectingVector final
{
    mozilla::Vector<T, MinInlineCapacity, AllocPolicy> vector;

    size_t pageSize;
    size_t pageMask;

    /*
     * The number of bytes between the start of the buffer being used by
     * |vector| and the first page we can protect. With jemalloc, this number
     * should always be 0 for vectors with a buffer larger than |pageSize / 2|
     * bytes, but with other allocators large buffers may not be page-aligned.
     */
    size_t offsetToPage;

    /* The number of currently protected bytes (a multiple of pageSize). */
    size_t protectedBytes;

    /*
     * The number of bytes that are currently unprotected, but could be.
     * This number starts at |-offsetToPage|, since any bytes before
     * |vector.begin() + offsetToPage| can never be protected (as we do not own
     * the whole page). As a result, if |unprotectedBytes >= pageSize|, we know
     * we can protect at least one more page, and |unprotectedBytes & ~pageMask|
     * is always the number of additional bytes we can protect. Put another way,
     * |offsetToPage + protectedBytes + unprotectedBytes == vector.length()|
     * always holds, and if |protectedBytes != 0| then |unprotectedBytes >= 0|.
     */
    intptr_t unprotectedBytes;

    bool protectionEnabled;
    bool regionUnprotected;

    void updateOffsetToPage() {
        unprotectedBytes += offsetToPage;
        offsetToPage = (pageSize - (uintptr_t(vector.begin()) & pageMask)) & pageMask;
        unprotectedBytes -= offsetToPage;
    }

    void protect() {
        MOZ_ASSERT(!regionUnprotected);
        if (protectionEnabled && unprotectedBytes >= intptr_t(pageSize)) {
            size_t toProtect = size_t(unprotectedBytes) & ~pageMask;
            uintptr_t addr = uintptr_t(vector.begin()) + offsetToPage + protectedBytes;
            gc::MakePagesReadOnly(reinterpret_cast<void*>(addr), toProtect);
            unprotectedBytes -= toProtect;
            protectedBytes += toProtect;
        }
    }

    void unprotect() {
        MOZ_ASSERT(!regionUnprotected);
        MOZ_ASSERT_IF(!protectionEnabled, !protectedBytes);
        if (protectedBytes) {
            uintptr_t addr = uintptr_t(vector.begin()) + offsetToPage;
            gc::UnprotectPages(reinterpret_cast<void*>(addr), protectedBytes);
            unprotectedBytes += protectedBytes;
            protectedBytes = 0;
        }
    }

    void protectNewBuffer() {
        updateOffsetToPage();
        protect();
    }

    bool anyProtected(size_t first, size_t last) {
        return last >= offsetToPage && first < offsetToPage + protectedBytes;
    }

    void setContainingRegion(size_t first, size_t last, uintptr_t* addr, size_t* size) {
        if (first < offsetToPage)
            first = offsetToPage;
        if (last > offsetToPage + protectedBytes - 1)
            last = offsetToPage + protectedBytes - 1;
        uintptr_t firstAddr = uintptr_t(vector.begin());
        uintptr_t firstPage = (firstAddr + first) & ~pageMask;
        uintptr_t lastPage = (firstAddr + last) & ~pageMask;
        *size = pageSize + (lastPage - firstPage);
        *addr = firstPage;
    }

    void increaseElemsUsed(size_t used) {
        unprotectedBytes += used * sizeof(T);
        protect();
    }

    /* A helper class to simplify unprotecting and reprotecting when needed. */
    class AutoUnprotect
    {
        PageProtectingVector* vector;

      public:
        AutoUnprotect() : vector(nullptr) {};

        void emplace(PageProtectingVector* holder) {
            vector = holder;
            vector->unprotect();
        }

        explicit AutoUnprotect(PageProtectingVector* holder) {
            emplace(holder);
        }

        ~AutoUnprotect() {
            if (vector)
                vector->protectNewBuffer();
        }
    };

  public:
    explicit PageProtectingVector(AllocPolicy policy = AllocPolicy())
      : vector(policy),
        pageSize(gc::SystemPageSize()),
        pageMask(pageSize - 1),
        offsetToPage(0),
        protectedBytes(0),
        unprotectedBytes(0),
        protectionEnabled(false),
        regionUnprotected(false) { updateOffsetToPage(); }

    ~PageProtectingVector() { unprotect(); }

    /* Enable protection for the entire buffer. */
    void enableProtection() {
        MOZ_ASSERT(!protectionEnabled);
        protectionEnabled = true;
        protectNewBuffer();
    }

    /* Disable protection for the entire buffer. */
    void disableProtection() {
        MOZ_ASSERT(protectionEnabled);
        unprotect();
        protectionEnabled = false;
    }

    /*
     * Disable protection on the smallest number of pages containing
     * both |firstByteOffset| and |lastByteOffset|.
     */
    void unprotectRegion(size_t firstByteOffset, size_t lastByteOffset) {
        MOZ_ASSERT(!regionUnprotected);
        regionUnprotected = true;
        if (!protectedBytes || !anyProtected(firstByteOffset, lastByteOffset))
            return;
        size_t size;
        uintptr_t addr;
        setContainingRegion(firstByteOffset, lastByteOffset, &addr, &size);
        gc::UnprotectPages(reinterpret_cast<void*>(addr), size);
    }

    /*
     * Re-enable protection on the region containing
     * |firstByteOffset| and |lastByteOffset|.
     */
    void reprotectRegion(size_t firstByteOffset, size_t lastByteOffset) {
        MOZ_ASSERT(regionUnprotected);
        regionUnprotected = false;
        if (!protectedBytes || !anyProtected(firstByteOffset, lastByteOffset))
            return;
        size_t size;
        uintptr_t addr;
        setContainingRegion(firstByteOffset, lastByteOffset, &addr, &size);
        gc::MakePagesReadOnly(reinterpret_cast<void*>(addr), size);
    }

    size_t length() const { return vector.length(); }

    T* begin() { return vector.begin(); }
    const T* begin() const { return vector.begin(); }

    void clear() {
        AutoUnprotect guard(this);
        vector.clear();
        offsetToPage = 0;
        unprotectedBytes = 0;
    }

    MOZ_MUST_USE bool reserve(size_t size) {
        AutoUnprotect guard;
        if (size > vector.capacity())
            guard.emplace(this);
        return vector.reserve(size);
    }

    template<typename U>
    MOZ_ALWAYS_INLINE void infallibleAppend(const U* values, size_t size) {
        vector.infallibleAppend(values, size);
        increaseElemsUsed(size);
    }

    template<typename U>
    MOZ_MUST_USE bool append(const U* values, size_t size) {
        bool ret;
        {
            AutoUnprotect guard;
            if (MOZ_UNLIKELY(vector.length() + size > vector.capacity()))
                guard.emplace(this);
            ret = vector.append(values, size);
        }
        if (ret)
            increaseElemsUsed(size);
        return ret;
    }
};

} /* namespace js */

#endif /* ds_PageProtectingVector_h */
