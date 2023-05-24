/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <glib.h>
#include <cstdint>
#include "mozilla/a11y/HyperTextAccessibleBase.h"
#include "nsCharTraits.h"
#include "nsString.h"

/**
 * ATK offsets are counted in unicode codepoints, while DOM offsets are counted
 * in UTF-16 code units.  That makes a difference for non-BMP characters,
 * which need two UTF-16 code units to be represented (a pair of surrogates),
 * while they are just one unicode character.
 *
 * To keep synchronization between ATK offsets (unicode codepoints) and DOM
 * offsets (UTF-16 code units), after translation from UTF-16 to UTF-8 we add a
 * BOM after each non-BMP character (which would otherwise use 2 UTF-16
 * code units for only 1 unicode codepoint).
 *
 * BOMs (Byte Order Marks, U+FEFF, also known as ZERO WIDTH NO-BREAK SPACE, but
 * that usage is deprecated) normally only appear at the beginning of unicode
 * files, but their occurrence within text (notably after cut&paste) is not
 * uncommon, and are thus considered as non-text.
 *
 * Since the selection requested through ATK may not contain both surrogates
 * at the ends of the selection, we need to fetch one UTF-16 code point more
 * on both side, and get rid of it before returning the string to ATK. The
 * ATKStringConverterHelper class maintains this, NewATKString should be used
 * to call it properly.
 *
 * In the end,
 * - if the start is between the high and low surrogates, the UTF-8 result
 * includes a BOM from it but not the character
 * - if the end is between the high and low surrogates, the UTF-8 result
 * includes the character but *not* the BOM
 * - all non-BMP characters that are fully in the string are in the UTF-8 result
 * as character followed by BOM
 */
namespace mozilla {
namespace a11y {

namespace DOMtoATK {

/**
 * Converts a string of accessible text into ATK gchar* string (by adding
 * BOMs). This can be used when offsets do not need to be adjusted because
 * ends of the string can not fall between surrogates.
 */
gchar* Convert(const nsAString& aStr);

/**
 * Add a BOM after each non-BMP character.
 */
void AddBOMs(nsACString& aDest, const nsACString& aSource);

class ATKStringConverterHelper {
 public:
  ATKStringConverterHelper(void)
      :
#ifdef DEBUG
        mAdjusted(false),
#endif
        mStartShifted(false),
        mEndShifted(false) {
  }

  /**
   * In order to properly get non-BMP values, offsets need to be changed
   * to get one character more on each end, so that ConvertUTF16toUTF8 can
   * convert surrogates even if the originally requested offsets fall between
   * them.
   */
  void AdjustOffsets(gint* aStartOffset, gint* aEndOffset, gint count);

  /**
   * Converts a string of accessible text with adjusted offsets into ATK
   * gchar* string (by adding BOMs).  Note, AdjustOffsets has to be called
   * before getting the text passed to this.
   */
  gchar* ConvertAdjusted(const nsAString& aStr);

 private:
  /**
   * Remove the additional characters requested by PrepareUTF16toUTF8.
   */
  gchar* FinishUTF16toUTF8(nsCString& aStr);

#ifdef DEBUG
  bool mAdjusted;
#endif
  bool mStartShifted;
  bool mEndShifted;
};

/**
 * Get text from aAccessible, using ATKStringConverterHelper to properly
 * introduce appropriate BOMs.
 */
inline gchar* NewATKString(HyperTextAccessibleBase* aAccessible,
                           gint aStartOffset, gint aEndOffset) {
  gint startOffset = aStartOffset, endOffset = aEndOffset;
  ATKStringConverterHelper converter;
  converter.AdjustOffsets(&startOffset, &endOffset,
                          gint(aAccessible->CharacterCount()));
  nsAutoString str;
  aAccessible->TextSubstring(startOffset, endOffset, str);

  if (str.Length() == 0) {
    // Bogus offsets, or empty string, either way we do not need conversion.
    return g_strdup("");
  }

  return converter.ConvertAdjusted(str);
}

/**
 * Get a character from aAccessible, fetching more data as appropriate to
 * properly get non-BMP characters or a BOM as appropriate.
 */
inline gunichar ATKCharacter(HyperTextAccessibleBase* aAccessible,
                             gint aOffset) {
  // char16_t is unsigned short in Mozilla, gnuichar is guint32 in glib.
  gunichar character = static_cast<gunichar>(aAccessible->CharAt(aOffset));

  if (NS_IS_LOW_SURROGATE(character)) {
    // Trailing surrogate, return BOM instead.
    return 0xFEFF;
  }

  if (NS_IS_HIGH_SURROGATE(character)) {
    // Heading surrogate, get the trailing surrogate and combine them.
    gunichar characterLow =
        static_cast<gunichar>(aAccessible->CharAt(aOffset + 1));

    if (!NS_IS_LOW_SURROGATE(characterLow)) {
      // It should have been a trailing surrogate... Flag the error.
      return 0xFFFD;
    }
    return SURROGATE_TO_UCS4(character, characterLow);
  }

  return character;
}

}  // namespace DOMtoATK

}  // namespace a11y
}  // namespace mozilla
