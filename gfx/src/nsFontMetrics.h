/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSFONTMETRICS__H__
#define NSFONTMETRICS__H__

#include <stdint.h>              // for uint32_t
#include <sys/types.h>           // for int32_t
#include "mozilla/Assertions.h"  // for MOZ_ASSERT_HELPER2
#include "mozilla/RefPtr.h"      // for RefPtr
#include "nsCOMPtr.h"            // for nsCOMPtr
#include "nsCoord.h"             // for nscoord
#include "nsError.h"             // for nsresult
#include "nsFont.h"              // for nsFont
#include "nsISupports.h"         // for NS_INLINE_DECL_REFCOUNTING
#include "nsStyleConsts.h"
#include "nscore.h"  // for char16_t

class gfxContext;
class gfxFontGroup;
class gfxUserFontSet;
class gfxTextPerfMetrics;
class nsPresContext;
class nsAtom;
struct nsBoundingMetrics;

namespace mozilla {
namespace gfx {
class DrawTarget;
}  // namespace gfx
}  // namespace mozilla

/**
 * Font metrics
 *
 * This class may be somewhat misnamed. A better name might be
 * nsFontList. The style system uses the nsFont struct for various
 * font properties, one of which is font-family, which can contain a
 * *list* of font names. The nsFont struct is "realized" by asking the
 * pres context to cough up an nsFontMetrics object, which contains
 * a list of real font handles, one for each font mentioned in
 * font-family (and for each fallback when we fall off the end of that
 * list).
 *
 * The style system needs to have access to certain metrics, such as
 * the em height (for the CSS "em" unit), and we use the first Western
 * font's metrics for that purpose. The platform-specific
 * implementations are expected to select non-Western fonts that "fit"
 * reasonably well with the Western font that is loaded at Init time.
 */
class nsFontMetrics final {
 public:
  typedef mozilla::gfx::DrawTarget DrawTarget;

  enum FontOrientation { eHorizontal, eVertical };

  struct MOZ_STACK_CLASS Params {
    nsAtom* language = nullptr;
    bool explicitLanguage = false;
    FontOrientation orientation = eHorizontal;
    gfxUserFontSet* userFontSet = nullptr;
    gfxTextPerfMetrics* textPerf = nullptr;
    gfxFontFeatureValueSet* featureValueLookup = nullptr;
  };

  nsFontMetrics(const nsFont& aFont, const Params& aParams,
                nsPresContext* aContext);

  // Used by stylo
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsFontMetrics)

  /**
   * Destroy this font metrics. This breaks the association between
   * the font metrics and the pres context.
   */
  void Destroy();

  /**
   * Return the font's x-height.
   */
  nscoord XHeight() const;

  /**
   * Return the font's cap-height.
   */
  nscoord CapHeight() const;

  /**
   * Return the font's superscript offset (the distance from the
   * baseline to where a superscript's baseline should be placed).
   * The value returned will be positive.
   */
  nscoord SuperscriptOffset() const;

  /**
   * Return the font's subscript offset (the distance from the
   * baseline to where a subscript's baseline should be placed).
   * The value returned will be positive.
   */
  nscoord SubscriptOffset() const;

  /**
   * Return the font's strikeout offset (the distance from the
   * baseline to where a strikeout should be placed) and size.
   * Positive values are above the baseline, negative below.
   */
  void GetStrikeout(nscoord& aOffset, nscoord& aSize) const;

  /**
   * Return the font's underline offset (the distance from the
   * baseline to where a underline should be placed) and size.
   * Positive values are above the baseline, negative below.
   */
  void GetUnderline(nscoord& aOffset, nscoord& aSize) const;

  /**
   * Returns the amount of internal leading for the font.
   * This is normally the difference between the max ascent
   * and the em ascent.
   */
  nscoord InternalLeading() const;

  /**
   * Returns the amount of external leading for the font.
   * em ascent(?) plus external leading is the font designer's
   * recommended line-height for this font.
   */
  nscoord ExternalLeading() const;

  /**
   * Returns the height of the em square.
   * This is em ascent plus em descent.
   */
  nscoord EmHeight() const;

  /**
   * Returns the ascent part of the em square.
   */
  nscoord EmAscent() const;

  /**
   * Returns the descent part of the em square.
   */
  nscoord EmDescent() const;

  /**
   * Returns the height of the bounding box.
   * This is max ascent plus max descent.
   */
  nscoord MaxHeight() const;

  /**
   * Returns the maximum distance characters in this font extend
   * above the base line.
   */
  nscoord MaxAscent() const;

  /**
   * Returns the maximum distance characters in this font extend
   * below the base line.
   */
  nscoord MaxDescent() const;

  /**
   * Returns the maximum character advance for the font.
   */
  nscoord MaxAdvance() const;

  /**
   * Returns the average character width
   */
  nscoord AveCharWidth() const;

  /**
   * Returns width of the zero character, or AveCharWidth if no zero present.
   */
  nscoord ZeroOrAveCharWidth() const;

  /**
   * Returns the often needed width of the space character
   */
  nscoord SpaceWidth() const;

  /**
   * Returns the font associated with these metrics. The return value
   * is only defined after Init() has been called.
   */
  const nsFont& Font() const { return mFont; }

  /**
   * Returns the language associated with these metrics
   */
  nsAtom* Language() const { return mLanguage; }

  /**
   * Returns the orientation (horizontal/vertical) of these metrics.
   */
  FontOrientation Orientation() const { return mOrientation; }

  int32_t GetMaxStringLength() const;

  // Get the width for this string.  aWidth will be updated with the
  // width in points, not twips.  Callers must convert it if they
  // want it in another format.
  nscoord GetWidth(const char* aString, uint32_t aLength,
                   DrawTarget* aDrawTarget) const;
  nscoord GetWidth(const char16_t* aString, uint32_t aLength,
                   DrawTarget* aDrawTarget) const;

  // Draw a string using this font handle on the surface passed in.
  void DrawString(const char* aString, uint32_t aLength, nscoord aX, nscoord aY,
                  gfxContext* aContext) const;
  void DrawString(const char16_t* aString, uint32_t aLength, nscoord aX,
                  nscoord aY, gfxContext* aContext,
                  DrawTarget* aTextRunConstructionDrawTarget) const;

  nsBoundingMetrics GetBoundingMetrics(const char16_t* aString,
                                       uint32_t aLength,
                                       DrawTarget* aDrawTarget) const;

  // Returns the LOOSE_INK_EXTENTS bounds of the text for determing the
  // overflow area of the string.
  nsBoundingMetrics GetInkBoundsForInkOverflow(const char16_t* aString,
                                               uint32_t aLength,
                                               DrawTarget* aDrawTarget) const;

  void SetTextRunRTL(bool aIsRTL) { mTextRunRTL = aIsRTL; }
  bool GetTextRunRTL() const { return mTextRunRTL; }

  void SetVertical(bool aVertical) { mVertical = aVertical; }
  bool GetVertical() const { return mVertical; }

  void SetTextOrientation(mozilla::StyleTextOrientation aTextOrientation) {
    mTextOrientation = aTextOrientation;
  }
  mozilla::StyleTextOrientation GetTextOrientation() const {
    return mTextOrientation;
  }

  bool ExplicitLanguage() const { return mExplicitLanguage; }

  gfxFontGroup* GetThebesFontGroup() const { return mFontGroup; }
  gfxUserFontSet* GetUserFontSet() const;

  int32_t AppUnitsPerDevPixel() const { return mP2A; }

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsFontMetrics();

  const nsFont mFont;
  RefPtr<gfxFontGroup> mFontGroup;
  RefPtr<nsAtom> const mLanguage;
  // Pointer to the pres context for which this fontMetrics object was
  // created.
  nsPresContext* MOZ_NON_OWNING_REF mPresContext;
  const int32_t mP2A;

  // The font orientation (horizontal or vertical) for which these metrics
  // have been initialized. This determines which line metrics (ascent and
  // descent) they will return.
  const FontOrientation mOrientation;

  // Whether mLanguage comes from explicit markup (in which case it should be
  // used to tailor effects like case-conversion) or is an inferred/default
  // value.
  const bool mExplicitLanguage;

  // These fields may be set by clients to control the behavior of methods
  // like GetWidth and DrawString according to the writing mode, direction
  // and text-orientation desired.
  bool mTextRunRTL;
  bool mVertical;
  mozilla::StyleTextOrientation mTextOrientation;
};

#endif /* NSFONTMETRICS__H__ */
