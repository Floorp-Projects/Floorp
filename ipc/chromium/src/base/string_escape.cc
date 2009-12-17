// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_escape.h"

#include <string>

#include "base/string_util.h"

namespace string_escape {

// Try to escape |c| as a "SingleEscapeCharacter" (\n, etc).  If successful,
// returns true and appends the escape sequence to |dst|.
template<typename CHAR>
static bool JavascriptSingleEscapeChar(const CHAR c, std::string* dst) {
  // WARNING: if you add a new case here, you need to update the reader as well.
  switch (c) {
    case '\b':
      dst->append("\\b");
      break;
    case '\f':
      dst->append("\\f");
      break;
    case '\n':
      dst->append("\\n");
      break;
    case '\r':
      dst->append("\\r");
      break;
    case '\t':
      dst->append("\\t");
      break;
    case '\v':
      dst->append("\\v");
      break;
    case '\\':
      dst->append("\\\\");
      break;
    case '"':
      dst->append("\\\"");
      break;
    default:
      return false;
  }
  return true;
}

void JavascriptDoubleQuote(const string16& str,
                           bool put_in_quotes,
                           std::string* dst) {
  if (put_in_quotes)
    dst->push_back('"');

  for (string16::const_iterator it = str.begin(); it != str.end(); ++it) {
    char16 c = *it;
    if (!JavascriptSingleEscapeChar(c, dst)) {
      if (c > 255) {
        // Non-ascii values need to be unicode dst->
        // TODO(tc): Some unicode values are handled specially. See
        // spidermonkey code.
        StringAppendF(dst, "\\u%04X", c);
      } else if (c < 32 || c > 126) {
        // Spidermonkey hex escapes these values.
        StringAppendF(dst, "\\x%02X", c);
      } else {
        dst->push_back(static_cast<char>(c));
      }
    }
  }

  if (put_in_quotes)
    dst->push_back('"');
}

void JavascriptDoubleQuote(const std::string& str,
                           bool put_in_quotes,
                           std::string* dst) {
  if (put_in_quotes)
    dst->push_back('"');

  for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
    unsigned char c = *it;
    if (!JavascriptSingleEscapeChar(c, dst)) {
      // Hex encode if the character is non-printable 7bit ascii
      if (c < 32 || c == 127) {
        StringAppendF(dst, "\\x%02X", c);
      } else {
        dst->push_back(static_cast<char>(c));
      }
    }
  }

  if (put_in_quotes)
    dst->push_back('"');
}

}  // namespace string_escape
