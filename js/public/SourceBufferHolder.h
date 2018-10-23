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
 *  2) If ownership is not given to SourceBufferHolder::init, then the memory
 *     must be kept alive until the JS compilation is complete.  If ownership
 *     is given to SourceBufferHolder::init, it is given *even if that function
 *     fails*.
 *  3) Any code calling SourceBufferHolder::take() must guarantee to keep the
 *     memory alive until JS compilation completes.  Normally only the JS
 *     engine should be calling take().
 *
 * Example use:
 *
 *    size_t length = 512;
 *    char16_t* chars = js_pod_malloc<char16_t>(length);
 *    if (!chars) {
 *        JS_ReportOutOfMemory(cx);
 *        return false;
 *    }
 *    JS::SourceBufferHolder srcBuf;
 *    if (!srcBuf.init(cx, chars, length, JS::SourceBufferHolder::GiveOwnership) ||
 *        !JS::Compile(cx, options, srcBuf))
 *    {
 *        return false;
 *    }
 */

#ifndef js_SourceBufferHolder_h
#define js_SourceBufferHolder_h

#include "mozilla/Assertions.h" // MOZ_ASSERT
#include "mozilla/Attributes.h" // MOZ_IS_CLASS_INIT, MOZ_MUST_USE
#include "mozilla/Likely.h" // MOZ_UNLIKELY

#include <stddef.h> // size_t
#include <stdint.h> // UINT32_MAX

#include "js/Utility.h" // JS::UniqueTwoByteChars

namespace JS {

namespace detail {

MOZ_COLD extern JS_PUBLIC_API(void)
ReportSourceTooLong(JSContext* cx);

}

class SourceBufferHolder final
{
  private:
    const char16_t* data_ = nullptr;
    size_t length_ = 0;
    bool ownsChars_ = false;

  public:
    /**
     * Construct a SourceBufferHolder.  It must be initialized using |init()|
     * before it can be used as compilation source text.
     */
    SourceBufferHolder() = default;

    /**
     * Construct a SourceBufferHolder from contents extracted from |other|.
     * This SourceBufferHolder will then act exactly as |other| would have
     * acted, had it not been passed to this function.  |other| will return to
     * its default-constructed state and must have |init()| called on it to use
     * it.
     */
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
        if (ownsChars_) {
            js_free(const_cast<char16_t*>(data_));
        }
    }

    enum Ownership {
        NoOwnership,
        GiveOwnership
    };

    /**
     * Initialize this with data.
     *
     * If |ownership == GiveOwnership|, *this function* takes ownership of
     * |data|, *even if* this function fails.  You MUST NOT free |data| after
     * this function takes ownership of it, REGARDLESS whether this function
     * succeeds or fails.  This single-owner-friendly approach reduces risk of
     * leaks on failure.
     *
     * |data| may be null if |dataLength == 0|; if so, this will silently be
     * initialized using non-null, unowned data.
     */
    MOZ_IS_CLASS_INIT MOZ_MUST_USE bool
    init(JSContext* cx, const char16_t* data, size_t dataLength, Ownership ownership) {
        MOZ_ASSERT_IF(data == nullptr, dataLength == 0);

        static const char16_t nonNullData[] = u"";

        // Initialize all fields *before* checking length.  This ensures that
        // if data ownership is being passed to SourceBufferHolder, a failure
        // of length-checking will result in this class freeing the data on
        // destruction.
        if (data) {
            data_ = data;
            length_ = static_cast<uint32_t>(dataLength);
            ownsChars_ = ownership == GiveOwnership;
        } else {
            data_ = nonNullData;
            length_ = 0;
            ownsChars_ = false;
        }

        // IMPLEMENTATION DETAIL, DO NOT RELY ON: This limit is used so we can
        // store offsets in JSScripts as uint32_t.  It could be lifted fairly
        // easily if desired as the compiler uses size_t internally.
        if (MOZ_UNLIKELY(dataLength > UINT32_MAX)) {
            detail::ReportSourceTooLong(cx);
            return false;
        }

        return true;
    }

    /**
     * Initialize this from data with ownership transferred from the caller.
     */
    MOZ_IS_CLASS_INIT MOZ_MUST_USE
    bool init(JSContext* cx, UniqueTwoByteChars data, size_t dataLength) {
        return init(cx, data.release(), dataLength, GiveOwnership);
    }

    /** Access the underlying source buffer without affecting ownership. */
    const char16_t* get() const {
        return data_;
    }

    /** Length of the source buffer in char16_t code units (not bytes). */
    uint32_t length() const {
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
     * After the buffer has been taken, this will continue to refer to the same
     * data -- it just won't own the data.  get() and length() will return the
     * same values.  The taken buffer must be kept alive until after JS script
     * compilation completes, as noted above, for this to be safe.
     *
     * The caller must check ownsChars() before attempting to take the buffer.
     * Taking and then free'ing an unowned buffer will have dire consequences.
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
