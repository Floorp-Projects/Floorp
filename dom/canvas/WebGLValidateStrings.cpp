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
// `[^]` is any-including-newline
// `*?` is non-greedy `*`, matching the fewest characters, instead of the most.
// `??` is non-greedy `?`, preferring to match zero times, instead of once.
// Non-continuing line comment is: `//[^]*?\n`
// But line-continuation is `\\\n`
// So we need to match `//[^]*?[^\\]\n`
// But we need to recognize "//\n", thus: `//([^]*?[^\\])??\n`
#define LINE_COMMENT "//(?:[^]*?[^\\\\])??\n"

// The fewest characters that match /*...*/
#define BLOCK_COMMENT "/[*][^]*?[*]/"

  static const std::regex COMMENT_RE(
      "(?:" LINE_COMMENT ")|(?:" BLOCK_COMMENT ")",
      std::regex::ECMAScript | std::regex::nosubs | std::regex::optimize);

#undef LINE_COMMENT
#undef BLOCK_COMMENT

  static const std::regex TRAILING_RE("/[*/]", std::regex::ECMAScript |
                                                   std::regex::nosubs |
                                                   std::regex::optimize);

  std::string ret;
  ret.reserve(src.size());

  auto itr = src.begin();
  auto end = src.end();
  std::smatch match;
  while (std::regex_search(itr, end, match, COMMENT_RE)) {
    const auto matchBegin = itr + match.position();
    const auto matchEnd = matchBegin + match.length();
    ret.append(itr, matchBegin);
    for (itr = matchBegin; itr != matchEnd; ++itr) {
      auto cur = *itr;
      switch (cur) {
        case '/':
        case '*':
        case '\n':
        case '\\':
          break;
        default:
          cur = ' ';
          break;
      }
      ret += cur;
    }
  }

  // Check for a trailing comment that hits EOF instead of the end of the
  // comment.
  if (std::regex_search(itr, end, match, TRAILING_RE)) {
    end = itr + match.position();
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
