/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* String utility functions used by the module loader. */

#ifndef shell_StringUtils_h
#define shell_StringUtils_h

#include "js/StableStringChars.h"
#include "js/String.h"

namespace js {
namespace shell {

inline char16_t CharAt(JSLinearString* str, size_t index) {
  return str->latin1OrTwoByteChar(index);
}

inline JSLinearString* SubString(JSContext* cx, JSLinearString* str,
                                 size_t start, size_t end) {
  MOZ_ASSERT(start <= str->length());
  MOZ_ASSERT(end >= start && end <= str->length());
  return NewDependentString(cx, str, start, end - start);
}

inline JSLinearString* SubString(JSContext* cx, JSLinearString* str,
                                 size_t start) {
  return SubString(cx, str, start, str->length());
}

template <size_t NullTerminatedLength>
bool StringStartsWith(JSLinearString* str,
                      const char16_t (&chars)[NullTerminatedLength]) {
  MOZ_ASSERT(NullTerminatedLength > 0);
  const size_t length = NullTerminatedLength - 1;
  MOZ_ASSERT(chars[length] == '\0');

  if (str->length() < length) {
    return false;
  }

  for (size_t i = 0; i < length; i++) {
    if (CharAt(str, i) != chars[i]) {
      return false;
    }
  }

  return true;
}

template <size_t NullTerminatedLength>
bool StringEquals(JSLinearString* str,
                  const char16_t (&chars)[NullTerminatedLength]) {
  MOZ_ASSERT(NullTerminatedLength > 0);
  const size_t length = NullTerminatedLength - 1;
  MOZ_ASSERT(chars[length] == '\0');

  return str->length() == length && StringStartsWith(str, chars);
}

inline int32_t IndexOf(Handle<JSLinearString*> str, char16_t target,
                       size_t start = 0) {
  int32_t length = str->length();
  for (int32_t i = start; i < length; i++) {
    if (CharAt(str, i) == target) {
      return i;
    }
  }

  return -1;
}

inline int32_t LastIndexOf(Handle<JSLinearString*> str, char16_t target) {
  int32_t length = str->length();
  for (int32_t i = length - 1; i >= 0; i--) {
    if (CharAt(str, i) == target) {
      return i;
    }
  }

  return -1;
}

inline JSLinearString* ReplaceCharGlobally(JSContext* cx,
                                           Handle<JSLinearString*> str,
                                           char16_t target,
                                           char16_t replacement) {
  int32_t i = IndexOf(str, target);
  if (i == -1) {
    return str;
  }

  JS::AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx, str)) {
    return nullptr;
  }

  Vector<char16_t> buf(cx);
  if (!buf.append(chars.twoByteChars(), str->length())) {
    return nullptr;
  }

  for (; i < int32_t(buf.length()); i++) {
    if (buf[i] == target) {
      buf[i] = replacement;
    }
  }

  RootedString result(cx, JS_NewUCStringCopyN(cx, buf.begin(), buf.length()));
  if (!result) {
    return nullptr;
  }

  return JS_EnsureLinearString(cx, result);
}

inline JSString* JoinStrings(JSContext* cx,
                             Handle<GCVector<JSLinearString*>> strings,
                             Handle<JSLinearString*> separator) {
  RootedString result(cx, JS_GetEmptyString(cx));

  for (size_t i = 0; i < strings.length(); i++) {
    HandleString str = strings[i];
    if (i != 0) {
      result = JS_ConcatStrings(cx, result, separator);
      if (!result) {
        return nullptr;
      }
    }

    result = JS_ConcatStrings(cx, result, str);
    if (!result) {
      return nullptr;
    }
  }

  return result;
}

}  // namespace shell
}  // namespace js

#endif  // shell_StringUtils_h
