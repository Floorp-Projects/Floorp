/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file forward-defines Irregexp classes that need to be visible
// to the rest of Spidermonkey and re-exports them into js::irregexp.

#ifndef regexp_RegExpTypes_h
#define regexp_RegExpTypes_h

namespace js {
class MatchPairs;
}

namespace v8 {
namespace internal {

struct InputOutputData {
  const void* inputStart;
  const void* inputEnd;

  // Index into inputStart (in chars) at which to begin matching.
  size_t startIndex;

  js::MatchPairs* matches;

  template <typename CharT>
  InputOutputData(const CharT* inputStart, const CharT* inputEnd,
                  size_t startIndex, js::MatchPairs* matches)
    : inputStart(inputStart),
      inputEnd(inputEnd),
      startIndex(startIndex),
      matches(matches)
  {}
};

} // namespace internal
} // namespace v8


namespace js {
namespace irregexp {

using InputOutputData = v8::internal::InputOutputData;

} // namespace irregexp
} // namespace js

#endif // regexp_RegExpTypes_h
