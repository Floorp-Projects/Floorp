/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowParts.h"
#include "nsContentUtils.h"
#include "nsString.h"

namespace mozilla {

static bool IsSpace(char16_t aChar) {
  return nsContentUtils::IsHTMLWhitespace(aChar);
};

using SingleMapping = std::pair<RefPtr<nsAtom>, RefPtr<nsAtom>>;

// https://drafts.csswg.org/css-shadow-parts/#parsing-mapping
//
// Returns null on both tokens to signal an error.
static SingleMapping ParseSingleMapping(const nsAString& aString) {
  const char16_t* c = aString.BeginReading();
  const char16_t* end = aString.EndReading();

  const auto CollectASequenceOfSpaces = [&c, end]() {
    while (c != end && IsSpace(*c)) {
      ++c;
    }
  };

  const auto CollectToken = [&c, end]() -> RefPtr<nsAtom> {
    const char16_t* t = c;
    while (c != end && !IsSpace(*c) && *c != ':') {
      ++c;
    }
    if (c == t) {
      return nullptr;
    }
    return NS_AtomizeMainThread(Substring(t, c));
  };

  // Steps 1 and 2 are variable declarations.
  //
  // 3. Collect a sequence of code points that are space characters.
  CollectASequenceOfSpaces();

  // 4. Collect a sequence of code points that are not space characters or
  // U+003A COLON characters, and call the result first token.
  RefPtr<nsAtom> firstToken = CollectToken();

  // 5. If first token is empty then return error.
  if (!firstToken) {
    return {nullptr, nullptr};
  }

  // 6. Collect a sequence of code points that are space characters.
  CollectASequenceOfSpaces();

  // 7. If the end of the input has been reached, return the pair first
  // token/first token.
  if (c == end) {
    return {firstToken, firstToken};
  }

  // 8. If character at position is not a U+003A COLON character, return error.
  if (*c != ':') {
    return {nullptr, nullptr};
  }

  // 9. Consume the U+003A COLON character.
  ++c;

  // 10. Collect a sequence of code points that are space characters.
  CollectASequenceOfSpaces();

  // 11. Collect a sequence of code points that are not space characters or
  // U+003A COLON characters. and let second token be the result.
  RefPtr<nsAtom> secondToken = CollectToken();

  // 12. If second token is empty then return error.
  if (!secondToken) {
    return {nullptr, nullptr};
  }

  // 13. Collect a sequence of code points that are space characters.
  CollectASequenceOfSpaces();

  // 14. If position is not past the end of input then return error.
  if (c != end) {
    return {nullptr, nullptr};
  }

  // 15. Return the pair first token/second token.
  return {std::move(firstToken), std::move(secondToken)};
}

// https://drafts.csswg.org/css-shadow-parts/#parsing-mapping-list
ShadowParts ShadowParts::Parse(const nsAString& aString) {
  ShadowParts parts;

  for (const auto& substring : aString.Split(',')) {
    auto mapping = ParseSingleMapping(substring);
    if (!mapping.first) {
      MOZ_ASSERT(!mapping.second);
      continue;
    }
    nsAtom* second = mapping.second.get();
    parts.mMappings.GetOrInsertNew(mapping.first)
        ->AppendElement(std::move(mapping.second));
    parts.mReverseMappings.InsertOrUpdate(second, std::move(mapping.first));
  }

  return parts;
}

#ifdef DEBUG
void ShadowParts::Dump() const {
  if (mMappings.IsEmpty()) {
    printf("  (empty)\n");
    return;
  }
  for (auto& entry : mMappings) {
    nsAutoCString key;
    entry.GetKey()->ToUTF8String(key);
    printf("  %s: ", key.get());

    bool first = true;
    for (nsAtom* part : *entry.GetData()) {
      if (!first) {
        printf(", ");
      }
      first = false;
      nsAutoCString value;
      part->ToUTF8String(value);
      printf("%s", value.get());
    }
    printf("\n");
  }
}
#endif
}  // namespace mozilla
