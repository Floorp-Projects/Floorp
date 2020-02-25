/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLValidateStrings.h"

#include "WebGLContext.h"

#include <regex>

namespace mozilla {

/* GLSL ES 3.00 p17:
  - Comments are delimited by / * and * /, or by // and a newline.

  - '//' style comments include the initial '//' marker and continue up to, but
not including, the terminating newline.

  - '/ * ... * /' comments include both the start and end marker.

  - The begin comment delimiters (/ * or //) are not recognized as comment
delimiters inside of a comment, hence comments cannot be nested.

  - Comments are treated syntactically as a single space.

However, since we want to keep line/character numbers the same, replace
char-for-char with spaces and newlines.
*/

std::string CommentsToSpaces(const std::string& src) {
// `.` is any-excluding-newline
// `[^]` is any-including-newline
// `*?` is non-greedy `*`, matching the fewest characters, instead of the most.
// `??` is non-greedy `?`, preferring to match zero times, instead of once.
// Non-continuing line comment is: `//.*\n`
// But we need to support hitting EOF instead of just newline: `//.*(?:\n|$)`
// But line-continuation is `\\\n`
// So we need to match `//(?:\\\n|.)*(?:\n|$)`
#define LINE_COMMENT R"(//(?:\\\n|.)*(?:\n|$))"

// The fewest characters that match /*...*/
#define BLOCK_COMMENT R"(/\*[^]*?\*/)"

  constexpr auto flags = std::regex::ECMAScript | std::regex::nosubs | std::regex::optimize;

  static const std::regex COMMENT_RE(
      "(?:" LINE_COMMENT ")|(?:" BLOCK_COMMENT ")",
      flags);

#undef LINE_COMMENT
#undef BLOCK_COMMENT

  std::string ret;
  ret.reserve(src.size());

  // Replace all comments with block comments with the right number of newlines.
  // Line positions may be off, but line numbers will be accurate, which is more important.

  auto itr = src.begin();
  const auto end = src.end();
  std::smatch match;
  while (std::regex_search(itr, end, match, COMMENT_RE)) {
    MOZ_ASSERT(match.length() >= 2);
    const auto matchBegin = itr + match.position();
    const auto matchEnd = matchBegin + match.length();
    const auto matchLast = matchEnd - 1;
    ret.append(itr, matchBegin);
    ret += "/*";
    for (itr = matchBegin; itr != matchLast; ++itr) {
      const auto cur = *itr;
      if (cur == '\n') {
        ret += cur;
      }
    }
    const auto cur = *itr;
    ret += "*/";
    if (cur == '\n') { // Must be a line-comment, so do the '\n' after the '*/'
      ret += cur;
    }
    ++itr;
    MOZ_ASSERT(itr == matchEnd);
  }

  ret.append(itr, end);
  return ret;
}

////////////////////////////////////////////////////////////////////////////////

static bool IsValidGLSLChar(const char c) {
  if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
      ('0' <= c && c <= '9')) {
    return true;
  }

  switch (c) {
    case ' ':
    case '\t':
    case '\v':
    case '\f':
    case '\r':
    case '\n':
    case '_':
    case '.':
    case '+':
    case '-':
    case '/':
    case '*':
    case '%':
    case '<':
    case '>':
    case '[':
    case ']':
    case '(':
    case ')':
    case '{':
    case '}':
    case '^':
    case '|':
    case '&':
    case '~':
    case '=':
    case '!':
    case ':':
    case ';':
    case ',':
    case '?':
      return true;

    default:
      return false;
  }
}

////

Maybe<char> CheckGLSLPreprocString(const bool webgl2,
                                   const std::string& string) {
  for (const auto c : string) {
    if (IsValidGLSLChar(c)) continue;
    if (c == '#') continue;
    if (c == '\\' && webgl2) continue;

    return Some(c);
  }
  return {};
}

Maybe<webgl::ErrorInfo> CheckGLSLVariableName(const bool webgl2,
                                              const std::string& name) {
  if (name.empty()) return {};

  const uint32_t maxSize = webgl2 ? 1024 : 256;
  if (name.size() > maxSize) {
    const auto info = nsPrintfCString(
        "Identifier is %zu characters long, exceeds the"
        " maximum allowed length of %u characters.",
        name.size(), maxSize);
    return Some(webgl::ErrorInfo{LOCAL_GL_INVALID_VALUE, info.BeginReading()});
  }

  for (const auto cur : name) {
    if (!IsValidGLSLChar(cur)) {
      const auto info =
          nsPrintfCString("String contains the illegal character 0x%x'.", cur);
      return Some(
          webgl::ErrorInfo{LOCAL_GL_INVALID_VALUE, info.BeginReading()});
    }
  }

  if (name.find("webgl_") == 0 || name.find("_webgl_") == 0) {
    return Some(webgl::ErrorInfo{
        LOCAL_GL_INVALID_OPERATION,
        "String matches reserved GLSL prefix pattern /_?webgl_/."});
  }

  return {};
}

}  // namespace mozilla
