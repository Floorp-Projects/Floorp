/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JavaScript string operations. */

#ifndef js_String_h
#define js_String_h

#include "js/shadow/String.h"  // JS::shadow::String

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_ALWAYS_INLINE
#include "mozilla/Likely.h"      // MOZ_LIKELY

#include <algorithm>  // std::copy_n
#include <stddef.h>   // size_t
#include <stdint.h>   // uint32_t, uint64_t, INT32_MAX

#include "jstypes.h"  // JS_PUBLIC_API

#include "js/TypeDecls.h"  // JS::Latin1Char

class JS_PUBLIC_API JSAtom;
class JSLinearString;
class JS_PUBLIC_API JSString;

namespace JS {

class JS_PUBLIC_API AutoRequireNoGC;

/**
 * Maximum length of a JS string. This is chosen so that the number of bytes
 * allocated for a null-terminated TwoByte string still fits in int32_t.
 */
static constexpr uint32_t MaxStringLength = (1 << 30) - 2;

static_assert((uint64_t(MaxStringLength) + 1) * sizeof(char16_t) <= INT32_MAX,
              "size of null-terminated JSString char buffer must fit in "
              "INT32_MAX");

/** Compute the length of a string. */
MOZ_ALWAYS_INLINE size_t GetStringLength(JSString* s) {
  return shadow::AsShadowString(s)->length();
}

/** Compute the length of a linear string. */
MOZ_ALWAYS_INLINE size_t GetLinearStringLength(JSLinearString* s) {
  return shadow::AsShadowString(s)->length();
}

/** Return true iff the given linear string uses Latin-1 storage. */
MOZ_ALWAYS_INLINE bool LinearStringHasLatin1Chars(JSLinearString* s) {
  return shadow::AsShadowString(s)->hasLatin1Chars();
}

/** Return true iff the given string uses Latin-1 storage. */
MOZ_ALWAYS_INLINE bool StringHasLatin1Chars(JSString* s) {
  return shadow::AsShadowString(s)->hasLatin1Chars();
}

/**
 * Given a linear string known to use Latin-1 storage, return a pointer to that
 * storage.  This pointer remains valid only as long as no GC occurs.
 */
MOZ_ALWAYS_INLINE const Latin1Char* GetLatin1LinearStringChars(
    const AutoRequireNoGC& nogc, JSLinearString* linear) {
  return shadow::AsShadowString(linear)->latin1LinearChars();
}

/**
 * Given a linear string known to use two-byte storage, return a pointer to that
 * storage.  This pointer remains valid only as long as no GC occurs.
 */
MOZ_ALWAYS_INLINE const char16_t* GetTwoByteLinearStringChars(
    const AutoRequireNoGC& nogc, JSLinearString* linear) {
  return shadow::AsShadowString(linear)->twoByteLinearChars();
}

/**
 * Given an in-range index into the provided string, return the character at
 * that index.
 */
MOZ_ALWAYS_INLINE char16_t GetLinearStringCharAt(JSLinearString* linear,
                                                 size_t index) {
  shadow::String* s = shadow::AsShadowString(linear);
  MOZ_ASSERT(index < s->length());

  return s->hasLatin1Chars() ? s->latin1LinearChars()[index]
                             : s->twoByteLinearChars()[index];
}

/**
 * Convert an atom to a linear string.  All atoms are linear, so this
 * operation is infallible.
 */
MOZ_ALWAYS_INLINE JSLinearString* AtomToLinearString(JSAtom* atom) {
  return reinterpret_cast<JSLinearString*>(atom);
}

/**
 * If the provided string uses externally-managed storage, return true and set
 * |*callbacks| to the external-string callbacks used to create it and |*chars|
 * to a pointer to its two-byte storage.  (These pointers remain valid as long
 * as the provided string is kept alive.)
 */
MOZ_ALWAYS_INLINE bool IsExternalString(
    JSString* str, const JSExternalStringCallbacks** callbacks,
    const char16_t** chars) {
  shadow::String* s = shadow::AsShadowString(str);

  if (!s->isExternal()) {
    return false;
  }

  *callbacks = s->externalCallbacks;
  *chars = s->nonInlineCharsTwoByte;
  return true;
}

namespace detail {

extern JS_FRIEND_API JSLinearString* StringToLinearStringSlow(JSContext* cx,
                                                              JSString* str);

}  // namespace detail

/** Convert a string to a linear string. */
MOZ_ALWAYS_INLINE JSLinearString* StringToLinearString(JSContext* cx,
                                                       JSString* str) {
  if (MOZ_LIKELY(shadow::AsShadowString(str)->isLinear())) {
    return reinterpret_cast<JSLinearString*>(str);
  }

  return detail::StringToLinearStringSlow(cx, str);
}

/** Copy characters in |s[start..start + len]| to |dest[0..len]|. */
MOZ_ALWAYS_INLINE void CopyLinearStringChars(char16_t* dest, JSLinearString* s,
                                             size_t len, size_t start = 0) {
#ifdef DEBUG
  size_t stringLen = GetLinearStringLength(s);
  MOZ_ASSERT(start <= stringLen);
  MOZ_ASSERT(len <= stringLen - start);
#endif

  shadow::String* str = shadow::AsShadowString(s);

  if (str->hasLatin1Chars()) {
    const Latin1Char* src = str->latin1LinearChars();
    for (size_t i = 0; i < len; i++) {
      dest[i] = src[start + i];
    }
  } else {
    const char16_t* src = str->twoByteLinearChars();
    std::copy_n(src + start, len, dest);
  }
}

/**
 * Copy characters in |s[start..start + len]| to |dest[0..len]|, lossily
 * truncating 16-bit values to |char| if necessary.
 */
MOZ_ALWAYS_INLINE void LossyCopyLinearStringChars(char* dest, JSLinearString* s,
                                                  size_t len,
                                                  size_t start = 0) {
#ifdef DEBUG
  size_t stringLen = GetLinearStringLength(s);
  MOZ_ASSERT(start <= stringLen);
  MOZ_ASSERT(len <= stringLen - start);
#endif

  shadow::String* str = shadow::AsShadowString(s);

  if (LinearStringHasLatin1Chars(s)) {
    const Latin1Char* src = str->latin1LinearChars();
    for (size_t i = 0; i < len; i++) {
      dest[i] = char(src[start + i]);
    }
  } else {
    const char16_t* src = str->twoByteLinearChars();
    for (size_t i = 0; i < len; i++) {
      dest[i] = char(src[start + i]);
    }
  }
}

/**
 * Copy characters in |s[start..start + len]| to |dest[0..len]|.
 *
 * This function is fallible.  If you already have a linear string, use the
 * infallible |JS::CopyLinearStringChars| above instead.
 */
[[nodiscard]] inline bool CopyStringChars(JSContext* cx, char16_t* dest,
                                          JSString* s, size_t len,
                                          size_t start = 0) {
  JSLinearString* linear = StringToLinearString(cx, s);
  if (!linear) {
    return false;
  }

  CopyLinearStringChars(dest, linear, len, start);
  return true;
}

/**
 * Copy characters in |s[start..start + len]| to |dest[0..len]|, lossily
 * truncating 16-bit values to |char| if necessary.
 *
 * This function is fallible.  If you already have a linear string, use the
 * infallible |JS::LossyCopyLinearStringChars| above instead.
 */
[[nodiscard]] inline bool LossyCopyStringChars(JSContext* cx, char* dest,
                                               JSString* s, size_t len,
                                               size_t start = 0) {
  JSLinearString* linear = StringToLinearString(cx, s);
  if (!linear) {
    return false;
  }

  LossyCopyLinearStringChars(dest, linear, len, start);
  return true;
}

}  // namespace JS

/** DO NOT USE, only present for Rust bindings as a temporary hack */
[[deprecated]] extern JS_PUBLIC_API bool JS_DeprecatedStringHasLatin1Chars(
    JSString* str);

#endif  // js_String_h
