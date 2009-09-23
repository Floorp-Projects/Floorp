// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for escaping strings.

#ifndef BASE_STRING_ESCAPE_H__
#define BASE_STRING_ESCAPE_H__

#include "base/string16.h"

namespace string_escape {

// Escape |str| appropriately for a javascript string litereal, _appending_ the
// result to |dst|. This will create standard escape sequences (\b, \n),
// hex escape sequences (\x00), and unicode escape sequences (\uXXXX).
// If |put_in_quotes| is true, the result will be surrounded in double quotes.
// The outputted literal, when interpreted by the browser, should result in a
// javascript string that is identical and the same length as the input |str|.
void JavascriptDoubleQuote(const string16& str,
                           bool put_in_quotes,
                           std::string* dst);

// Similar to the wide version, but for narrow strings.  It will not use
// \uXXXX unicode escape sequences.  It will pass non-7bit characters directly
// into the string unencoded, allowing the browser to interpret the encoding.
// The outputted literal, when interpreted by the browser, could result in a
// javascript string of a different length than the input |str|.
void JavascriptDoubleQuote(const std::string& str,
                           bool put_in_quotes,
                           std::string* dst);

}  // namespace string_escape

#endif  // BASE_STRING_ESCAPE_H__
