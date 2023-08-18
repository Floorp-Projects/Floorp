/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextPaintStyle_h__
#define nsTextPaintStyle_h__

#include "mozilla/Attributes.h"
#include "mozilla/ComputedStyle.h"
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
  void GetIMESelectionColors(int32_t aIndex, nscolor* aForeColor,
                             nscolor* aBackColor);
  // if this returns false, we don't need to draw underline.
  bool GetSelectionUnderlineForPaint(int32_t aIndex, nscolor* aLineColor,
                                     float* aRelativeSize,
                                     StyleTextDecorationStyle* aStyle);

  // if this returns false, we don't need to draw underline.
  static bool GetSelectionUnderline(nsIFrame*, int32_t aIndex,
                                    nscolor* aLineColor, float* aRelativeSize,
                                    StyleTextDecorationStyle* aStyle);

  // if this returns false, no text-shadow was specified for the selection
  // and the *aShadow parameter was not modified.
  bool GetSelectionShadow(mozilla::Span<const StyleSimpleShadow>* aShadows);

  nsPresContext* PresContext() const { return mPresContext; }

  enum {
    eIndexRawInput = 0,
    eIndexSelRawText,
    eIndexConvText,
    eIndexSelConvText,
    eIndexSpellChecker
  };

  static int32_t GetUnderlineStyleIndexForSelectionType(
      SelectionType aSelectionType) {
    switch (aSelectionType) {
      case SelectionType::eIMERawClause:
        return eIndexRawInput;
      case SelectionType::eIMESelectedRawClause:
        return eIndexSelRawText;
      case SelectionType::eIMEConvertedClause:
        return eIndexConvText;
      case SelectionType::eIMESelectedClause:
        return eIndexSelConvText;
      case SelectionType::eSpellCheck:
        return eIndexSpellChecker;
      default:
        NS_WARNING("non-IME selection type");
        return eIndexRawInput;
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
    bool mInit;
    nscolor mTextColor;
    nscolor mBGColor;
    nscolor mUnderlineColor;
    StyleTextDecorationStyle mUnderlineStyle;
    float mUnderlineRelativeSize;
  };
  nsSelectionStyle mSelectionStyle[5];

  // Color initializations
  void InitCommonColors();
  bool InitSelectionColorsAndShadow();

  nsSelectionStyle* GetSelectionStyle(int32_t aIndex);
  void InitSelectionStyle(int32_t aIndex);

  // Ensures sufficient contrast between the frame background color and the
  // selection background color, and swaps the selection text and background
  // colors accordingly.
  bool EnsureSufficientContrast(nscolor* aForeColor, nscolor* aBackColor);

  nscolor GetResolvedForeColor(nscolor aColor, nscolor aDefaultForeColor,
                               nscolor aBackColor);
};

#endif  // nsTextPaintStyle_h__
