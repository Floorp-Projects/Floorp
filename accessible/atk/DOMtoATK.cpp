/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMtoATK.h"
#include "nsUTF8Utils.h"

namespace mozilla {
namespace a11y {

namespace DOMtoATK {

void AddBOMs(nsACString& aDest, const nsACString& aSource) {
  uint32_t destlength = 0;

  // First compute how much room we will need.
  for (uint32_t srci = 0; srci < aSource.Length();) {
    int bytes = UTF8traits::bytes(aSource[srci]);
    if (bytes >= 4) {
      // Non-BMP character, will add a BOM after it.
      destlength += 3;
    }
    // Skip whole character encoding.
    srci += bytes;
    destlength += bytes;
  }

  uint32_t desti = 0;  // Index within aDest.

  // Add BOMs after non-BMP characters.
  aDest.SetLength(destlength);
  for (uint32_t srci = 0; srci < aSource.Length();) {
    uint32_t bytes = UTF8traits::bytes(aSource[srci]);

    MOZ_ASSERT(bytes <= aSource.Length() - srci,
               "We should have the whole sequence");

    // Copy whole sequence.
    aDest.Replace(desti, bytes, Substring(aSource, srci, bytes));
    desti += bytes;
    srci += bytes;

    if (bytes >= 4) {
      // More than 4 bytes in UTF-8 encoding exactly means more than 16 encoded
      // bits.  This is thus a non-BMP character which needed a surrogate
      // pair to get encoded in UTF-16, add a BOM after it.

      // And add a BOM after it.
      aDest.Replace(desti, 3, "\xEF\xBB\xBF");
      desti += 3;
    }
  }
  MOZ_ASSERT(desti == destlength,
             "Incoherency between computed length"
             "and actually translated length");
}

void ATKStringConverterHelper::AdjustOffsets(gint* aStartOffset,
                                             gint* aEndOffset, gint count) {
  MOZ_ASSERT(!mAdjusted,
             "DOMtoATK::ATKStringConverterHelper::AdjustOffsets needs to be "
             "called only once");

  if (*aStartOffset > 0) {
    (*aStartOffset)--;
    mStartShifted = true;
  }

  if (*aEndOffset >= 0 && *aEndOffset < count) {
    (*aEndOffset)++;
    mEndShifted = true;
  }

#ifdef DEBUG
  mAdjusted = true;
#endif
}

gchar* ATKStringConverterHelper::FinishUTF16toUTF8(nsCString& aStr) {
  int skip = 0;

  if (mStartShifted) {
    // AdjustOffsets added a leading character.

    MOZ_ASSERT(aStr.Length() > 0, "There should be a leading character");
    MOZ_ASSERT(
        static_cast<int>(aStr.Length()) >= UTF8traits::bytes(aStr.CharAt(0)),
        "The leading character should be complete");

    // drop first character
    skip = UTF8traits::bytes(aStr.CharAt(0));
  }

  if (mEndShifted) {
    // AdjustOffsets added a trailing character.

    MOZ_ASSERT(aStr.Length() > 0, "There should be a trailing character");

    int trail = -1;
    // Find beginning of last character.
    for (trail = aStr.Length() - 1; trail >= 0; trail--) {
      if (!UTF8traits::isInSeq(aStr.CharAt(trail))) {
        break;
      }
    }
    MOZ_ASSERT(trail >= 0,
               "There should be at least a whole trailing character");
    MOZ_ASSERT(trail + UTF8traits::bytes(aStr.CharAt(trail)) ==
                   static_cast<int>(aStr.Length()),
               "The trailing character should be complete");

    // Drop the last character.
    aStr.Truncate(trail);
  }

  // copy and return, libspi will free it
  return g_strdup(aStr.get() + skip);
}

gchar* ATKStringConverterHelper::ConvertAdjusted(const nsAString& aStr) {
  MOZ_ASSERT(mAdjusted,
             "DOMtoATK::ATKStringConverterHelper::AdjustOffsets needs to be "
             "called before ATKStringConverterHelper::ConvertAdjusted");

  NS_ConvertUTF16toUTF8 cautoStr(aStr);
  if (!cautoStr.get()) {
    return nullptr;
  }

  nsAutoCString cautoStrBOMs;
  AddBOMs(cautoStrBOMs, cautoStr);
  return FinishUTF16toUTF8(cautoStrBOMs);
}

gchar* Convert(const nsAString& aStr) {
  NS_ConvertUTF16toUTF8 cautoStr(aStr);
  if (!cautoStr.get()) {
    return nullptr;
  }

  nsAutoCString cautoStrBOMs;
  AddBOMs(cautoStrBOMs, cautoStr);
  return g_strdup(cautoStrBOMs.get());
}

}  // namespace DOMtoATK

}  // namespace a11y
}  // namespace mozilla
