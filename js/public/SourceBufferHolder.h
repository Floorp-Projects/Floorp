/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * SourceBufferHolder groups buffer and length values and provides a way to
 * optionally pass ownership of the buffer to the JS engine without copying.
 *
 * Rules for use:
 *
 *  1) The data array must be allocated with js_malloc() or js_realloc() if
 *     ownership is being granted to the SourceBufferHolder.
 *  2) If ownership is not given to the SourceBufferHolder, then the memory
 *     must be kept alive until the JS compilation is complete.
 *  3) Any code calling SourceBufferHolder::take() must guarantee to keep the
 *     memory alive until JS compilation completes.  Normally only the JS
 *     engine should be calling take().
 *
 * Example use:
 *
 *    size_t length = 512;
 *    char16_t* chars = js_pod_malloc<char16_t>(length);
 *    JS::SourceBufferHolder srcBuf(chars, length, JS::SourceBufferHolder::GiveOwnership);
 *    JS::Compile(cx, options, srcBuf);
 */

#ifndef js_SourceBufferHolder_h
#define js_SourceBufferHolder_h

#include "mozilla/Assertions.h" // MOZ_ASSERT

#include <stddef.h> // size_t

#include "js/Utility.h" // JS::UniqueTwoByteChars

namespace JS {

class SourceBufferHolder final
{
  private:
    const char16_t* data_;
    size_t length_;
    bool ownsChars_;

  private:
    void fixEmptyBuffer() {
        // Ensure that null buffers properly return an unowned, empty,
        // null-terminated string.
        static const char16_t NullChar_ = 0;
        if (!data_) {
            data_ = &NullChar_;
            length_ = 0;
            ownsChars_ = false;
        }
    }

  public:
    enum Ownership {
      NoOwnership,
      GiveOwnership
    };

    SourceBufferHolder(const char16_t* data, size_t dataLength, Ownership ownership)
      : data_(data),
        length_(dataLength),
        ownsChars_(ownership == GiveOwnership)
    {
        fixEmptyBuffer();
    }

    SourceBufferHolder(UniqueTwoByteChars&& data, size_t dataLength)
      : data_(data.release()),
        length_(dataLength),
        ownsChars_(true)
    {
        fixEmptyBuffer();
    }

    SourceBufferHolder(SourceBufferHolder&& other)
      : data_(other.data_),
        length_(other.length_),
        ownsChars_(other.ownsChars_)
    {
        other.data_ = nullptr;
        other.length_ = 0;
        other.ownsChars_ = false;
    }

    ~SourceBufferHolder() {
        if (ownsChars_)
            js_free(const_cast<char16_t*>(data_));
    }

    /** Access the underlying source buffer without affecting ownership. */
    const char16_t* get() const {
        return data_;
    }

    /** Length of the source buffer in char16_t code units (not bytes). */
    size_t length() const {
        return length_;
    }

    /**
     * Returns true if the SourceBufferHolder owns the buffer and will free it
     * upon destruction.  If true, it is legal to call take().
     */
    bool ownsChars() const {
        return ownsChars_;
    }

    /**
     * Retrieve and take ownership of the underlying data buffer.  The caller
     * is now responsible for calling js_free() on the returned value, *but
     * only after JS script compilation has completed*.
     *
     * After the buffer has been taken the SourceBufferHolder functions as if
     * it had been constructed on an unowned buffer;  get() and length() still
     * work.  In order for this to be safe the taken buffer must be kept alive
     * until after JS script compilation completes as noted above.
     *
     * It's the caller's responsibility to check ownsChars() before taking the
     * buffer.  Taking and then free'ing an unowned buffer will have dire
     * consequences.
     */
    char16_t* take() {
        MOZ_ASSERT(ownsChars_);
        ownsChars_ = false;
        return const_cast<char16_t*>(data_);
    }

  private:
    SourceBufferHolder(SourceBufferHolder&) = delete;
    SourceBufferHolder& operator=(SourceBufferHolder&) = delete;
};

} // namespace JS

#endif /* js_SourceBufferHolder_h */
