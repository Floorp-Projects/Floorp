/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextPaintStyle_h__
#define nsTextPaintStyle_h__

#include "mozilla/Attributes.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/Span.h"

#include "nsAtomHashKeys.h"
#include "nsISelectionController.h"
#include "nsTHashMap.h"

class nsTextFrame;
class nsPresContext;

namespace mozilla {
enum class StyleTextDecorationStyle : uint8_t;
}

/**
 * This helper object computes colors used for painting, and also IME
 * underline information. The data is computed lazily and cached as necessary.
 * These live for just the duration of one paint operation.
 */
class MOZ_STACK_CLASS nsTextPaintStyle {
  using ComputedStyle = mozilla::ComputedStyle;
  using SelectionType = mozilla::SelectionType;
  using StyleTextDecorationStyle = mozilla::StyleTextDecorationStyle;
  using StyleSimpleShadow = mozilla::StyleSimpleShadow;

 public:
  explicit nsTextPaintStyle(nsTextFrame* aFrame);

  void SetResolveColors(bool aResolveColors) {
    mResolveColors = aResolveColors;
  }

  nscolor GetTextColor();

  // SVG text has its own painting process, so we should never get its stroke
  // property from here.
  nscolor GetWebkitTextStrokeColor();
  float GetWebkitTextStrokeWidth();

  // Index used to look up styles for different types of selection.
  enum class SelectionStyleIndex : uint8_t {
    RawInput = 0,
    SelRawText,
    ConvText,
    SelConvText,
    SpellChecker,
    // Not an actual enum value; used to size the array of styles.
    Count,
  };

  /**
   * Compute the colors for normally-selected text. Returns false if
   * the normal selection is not being displayed.
   */
  bool GetSelectionColors(nscolor* aForeColor, nscolor* aBackColor);
  void GetHighlightColors(nscolor* aForeColor, nscolor* aBackColor);
  // Computes colors for custom highlights.
  // Returns false if there are no rules associated with `aHighlightName`.
  bool GetCustomHighlightTextColor(nsAtom* aHighlightName, nscolor* aForeColor);
  bool GetCustomHighlightBackgroundColor(nsAtom* aHighlightName,
                                         nscolor* aBackColor);
  void GetURLSecondaryColor(nscolor* aForeColor);
  void GetIMESelectionColors(SelectionStyleIndex aIndex, nscolor* aForeColor,
                             nscolor* aBackColor);
  // if this returns false, we don't need to draw underline.
  bool GetSelectionUnderlineForPaint(SelectionStyleIndex aIndex,
                                     nscolor* aLineColor, float* aRelativeSize,
                                     StyleTextDecorationStyle* aStyle);

  // if this returns false, we don't need to draw underline.
  static bool GetSelectionUnderline(nsIFrame*, SelectionStyleIndex aIndex,
                                    nscolor* aLineColor, float* aRelativeSize,
                                    StyleTextDecorationStyle* aStyle);

  // if this returns false, no text-shadow was specified for the selection
  // and the *aShadow parameter was not modified.
  bool GetSelectionShadow(mozilla::Span<const StyleSimpleShadow>* aShadows);

  nsPresContext* PresContext() const { return mPresContext; }

  static SelectionStyleIndex GetUnderlineStyleIndexForSelectionType(
      SelectionType aSelectionType) {
    switch (aSelectionType) {
      case SelectionType::eIMERawClause:
        return SelectionStyleIndex::RawInput;
      case SelectionType::eIMESelectedRawClause:
        return SelectionStyleIndex::SelRawText;
      case SelectionType::eIMEConvertedClause:
        return SelectionStyleIndex::ConvText;
      case SelectionType::eIMESelectedClause:
        return SelectionStyleIndex::SelConvText;
      case SelectionType::eSpellCheck:
        return SelectionStyleIndex::SpellChecker;
      default:
        NS_WARNING("non-IME selection type");
        return SelectionStyleIndex::RawInput;
    }
  }

  nscolor GetSystemFieldForegroundColor();
  nscolor GetSystemFieldBackgroundColor();

 protected:
  nsTextFrame* mFrame;
  nsPresContext* mPresContext;
  bool mInitCommonColors;
  bool mInitSelectionColorsAndShadow;
  bool mResolveColors;

  // Selection data

  nscolor mSelectionTextColor;
  nscolor mSelectionBGColor;
  RefPtr<ComputedStyle> mSelectionPseudoStyle;
  nsTHashMap<RefPtr<nsAtom>, RefPtr<ComputedStyle>>
      mCustomHighlightPseudoStyles;

  // Common data

  int32_t mSufficientContrast;
  nscolor mFrameBackgroundColor;
  nscolor mSystemFieldForegroundColor;
  nscolor mSystemFieldBackgroundColor;

  // selection colors and underline info, the colors are resolved colors if
  // mResolveColors is true (which is the default), i.e., the foreground color
  // and background color are swapped if it's needed. And also line color will
  // be resolved from them.
  struct nsSelectionStyle {
    nscolor mTextColor;
    nscolor mBGColor;
    nscolor mUnderlineColor;
    StyleTextDecorationStyle mUnderlineStyle;
    float mUnderlineRelativeSize;
  };
  mozilla::EnumeratedArray<SelectionStyleIndex, SelectionStyleIndex::Count,
                           mozilla::Maybe<nsSelectionStyle>>
      mSelectionStyle;

  // Color initializations
  void InitCommonColors();
  bool InitSelectionColorsAndShadow();

  nsSelectionStyle* SelectionStyle(SelectionStyleIndex aIndex);
  nsSelectionStyle InitSelectionStyle(SelectionStyleIndex aIndex);

  // Ensures sufficient contrast between the frame background color and the
  // selection background color, and swaps the selection text and background
  // colors accordingly.
  bool EnsureSufficientContrast(nscolor* aForeColor, nscolor* aBackColor);

  nscolor GetResolvedForeColor(nscolor aColor, nscolor aDefaultForeColor,
                               nscolor aBackColor);
};

#endif  // nsTextPaintStyle_h__
