/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * DEPRECATED functions and classes for heap-allocating copies of a JSString's
 * data.
 */

#ifndef js_AutoByteString_h
#define js_AutoByteString_h

#include "mozilla/Assertions.h" // MOZ_ASSERT
#include "mozilla/Attributes.h" // MOZ_RAII, MOZ_GUARD*

#include <string.h> // strlen

#include "jstypes.h" // JS_PUBLIC_API

#include "js/MemoryFunctions.h" // JS_free
#include "js/RootingAPI.h" // JS::Handle
#include "js/TypeDecls.h" // JSContext, JSString
#include "js/Utility.h" // js_free, JS::UniqueChars

/**
 * DEPRECATED
 *
 * Allocate memory sufficient to contain the characters of |str| truncated to
 * Latin-1 and a trailing null terminator, fill the memory with the characters
 * interpreted in that manner plus the null terminator, and return a pointer to
 * the memory.  The memory must be freed using JS_free to avoid leaking.
 *
 * This function *loses information* when it copies the characters of |str| if
 * |str| contains code units greater than 0xFF.  Additionally, users that
 * depend on null-termination will misinterpret the copied characters if |str|
 * contains any nulls.  Avoid using this function if possible, because it will
 * eventually be removed.
 */
extern JS_PUBLIC_API(char*)
JS_EncodeString(JSContext* cx, JSString* str);

/**
 * DEPRECATED
 *
 * Same behavior as JS_EncodeString(), but encode into a UTF-8 string.
 *
 * This function *loses information* when it copies the characters of |str| if
 * |str| contains invalid UTF-16: U+FFFD REPLACEMENT CHARACTER will be copied
 * instead.
 *
 * The returned string is also subject to misinterpretation if |str| contains
 * any nulls (which are faithfully transcribed into the returned string, but
 * which will implicitly truncate the string if it's passed to functions that
 * expect null-terminated strings).
 *
 * Avoid using this function if possible, because we'll remove it once we can
 * devise a better API for the task.
 */
extern JS_PUBLIC_API(char*)
JS_EncodeStringToUTF8(JSContext* cx, JS::Handle<JSString*> str);

/**
 * DEPRECATED
 *
 * A lightweight RAII helper class around the various JS_Encode* functions
 * above, subject to the same pitfalls noted above.  Avoid using this class if
 * possible, because as with the functions above, it too needs to be replaced
 * with a better, safer API.
 */
class MOZ_RAII JSAutoByteString final
{
  private:
    char* mBytes;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  private:
    JSAutoByteString(const JSAutoByteString& another) = delete;
    void operator=(const JSAutoByteString& another) = delete;

  public:
    JSAutoByteString(JSContext* cx, JSString* str
                     MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mBytes(JS_EncodeString(cx, str))
    {
        MOZ_ASSERT(cx);
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    explicit JSAutoByteString(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
      : mBytes(nullptr)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~JSAutoByteString() {
        JS_free(nullptr, mBytes);
    }

    /* Take ownership of the given byte array. */
    void initBytes(JS::UniqueChars&& bytes) {
        MOZ_ASSERT(!mBytes);
        mBytes = bytes.release();
    }

    char* encodeLatin1(JSContext* cx, JSString* str) {
        MOZ_ASSERT(!mBytes);
        MOZ_ASSERT(cx);
        mBytes = JS_EncodeString(cx, str);
        return mBytes;
    }

    char* encodeUtf8(JSContext* cx, JS::Handle<JSString*> str) {
        MOZ_ASSERT(!mBytes);
        MOZ_ASSERT(cx);
        mBytes = JS_EncodeStringToUTF8(cx, str);
        return mBytes;
    }

    void clear() {
        js_free(mBytes);
        mBytes = nullptr;
    }

    char* ptr() const {
        return mBytes;
    }

    bool operator!() const {
        return !mBytes;
    }

    size_t length() const {
        if (!mBytes)
            return 0;
        return strlen(mBytes);
    }
};

#endif /* js_AutoByteString_h */
