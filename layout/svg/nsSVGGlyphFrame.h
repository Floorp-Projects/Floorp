/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGGLYPHFRAME_H__
#define __NS_SVGGLYPHFRAME_H__

#include "mozilla/Attributes.h"
#include "gfxFont.h"
#include "nsISVGGlyphFragmentNode.h"
#include "nsISVGChildFrame.h"
#include "nsSVGGeometryFrame.h"
#include "nsSVGUtils.h"
#include "nsTextFragment.h"
#include "gfxSVGGlyphs.h"

class CharacterIterator;
class gfxContext;
class nsDisplaySVGGlyphs;
class nsIDOMSVGRect;
class nsRenderingContext;
class nsSVGGlyphFrame;
class nsSVGTextFrame;
class nsSVGTextPathFrame;
class gfxTextObjectPaint;

struct CharacterPosition;

typedef gfxFont::DrawMode DrawMode;

typedef nsSVGGeometryFrame nsSVGGlyphFrameBase;

class nsSVGGlyphFrame : public nsSVGGlyphFrameBase,
                        public nsISVGGlyphFragmentNode,
                        public nsISVGChildFrame
{
  class AutoCanvasTMForMarker;
  friend class AutoCanvasTMForMarker;
  friend class CharacterIterator;
  friend nsIFrame*
  NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGGlyphFrame(nsStyleContext* aContext)
    : nsSVGGlyphFrameBase(aContext),
      mTextRun(nullptr),
      mStartIndex(0),
      mGetCanvasTMForFlag(nsISVGChildFrame::FOR_OUTERSVG_TM),
      mCompressWhitespace(true),
      mTrimLeadingWhitespace(false),
      mTrimTrailingWhitespace(false)
      {}
  ~nsSVGGlyphFrame()
  {
    ClearTextRun();
  }

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // These do not use the global transform if NS_STATE_NONDISPLAY_CHILD
  nsresult GetStartPositionOfChar(uint32_t charnum, nsISupports **_retval);
  nsresult GetEndPositionOfChar(uint32_t charnum, nsISupports **_retval);
  nsresult GetExtentOfChar(uint32_t charnum, nsIDOMSVGRect **_retval);
  nsresult GetRotationOfChar(uint32_t charnum, float *_retval);
  /**
   * @param aForceGlobalTransform controls whether to use the
   * global transform even when NS_STATE_NONDISPLAY_CHILD
   */
  float GetAdvance(bool aForceGlobalTransform);

  void SetGlyphPosition(gfxPoint *aPosition, bool aForceGlobalTransform);
  nsSVGTextPathFrame* FindTextPathParent();
  bool IsStartOfChunk(); // == is new absolutely positioned chunk.

  void GetXY(mozilla::SVGUserUnitList *aX, mozilla::SVGUserUnitList *aY);
  void SetStartIndex(uint32_t aStartIndex);
  /*
   * Returns inherited x and y values instead of parent element's attribute
   * values.
   */
  void GetEffectiveXY(int32_t strLength,
                      nsTArray<float> &aX, nsTArray<float> &aY);
  /*
   * Returns inherited dx and dy values instead of parent element's attribute
   * values.
   */
  void GetEffectiveDxDy(int32_t strLength, 
                        nsTArray<float> &aDx,
                        nsTArray<float> &aDy);
  /*
   * Returns inherited rotate values instead of parent element's attribute
   * values.
   */
  void GetEffectiveRotate(int32_t strLength,
                          nsTArray<float> &aRotate);
  uint16_t GetTextAnchor();
  bool IsAbsolutelyPositioned();
  bool IsTextEmpty() const {
    return mContent->GetText()->GetLength() == 0;
  }
  void SetTrimLeadingWhitespace(bool aTrimLeadingWhitespace) {
    if (mTrimLeadingWhitespace != aTrimLeadingWhitespace) {
      mTrimLeadingWhitespace = aTrimLeadingWhitespace;
      ClearTextRun();
    }
  }
  void SetTrimTrailingWhitespace(bool aTrimTrailingWhitespace) {
    if (mTrimTrailingWhitespace != aTrimTrailingWhitespace) {
      mTrimTrailingWhitespace = aTrimTrailingWhitespace;
      ClearTextRun();
    }
  }
  bool EndsWithWhitespace() const;
  bool IsAllWhitespace() const;

  // nsIFrame interface:
  NS_IMETHOD  CharacterDataChanged(CharacterDataChangeInfo* aInfo);

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  NS_IMETHOD  IsSelectable(bool* aIsSelectable, uint8_t* aSelectStyle) const;

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgGlyphFrame
   */
  virtual nsIAtom* GetType() const;

  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    // Set the frame state bit for text frames to mark them as replaced.
    // XXX kipp: temporary

    return nsSVGGlyphFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGlyph"), aResult);
  }
#endif

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  // nsISVGChildFrame interface:
  // These four always use the global transform, even if NS_STATE_NONDISPLAY_CHILD
  NS_IMETHOD PaintSVG(nsRenderingContext *aContext,
                      const nsIntRect *aDirtyRect);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint &aPoint) MOZ_OVERRIDE;
  virtual SVGBBox GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      uint32_t aFlags) MOZ_OVERRIDE;

  NS_IMETHOD_(nsRect) GetCoveredRegion() MOZ_OVERRIDE;
  virtual void ReflowSVG() MOZ_OVERRIDE;
  virtual void NotifySVGChanged(uint32_t aFlags) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsDisplayContainer() MOZ_OVERRIDE { return false; }

  // nsSVGGeometryFrame methods
  gfxMatrix GetCanvasTM(uint32_t aFor);

  // nsISVGGlyphFragmentNode interface:
  // These do not use the global transform if NS_STATE_NONDISPLAY_CHILD
  virtual uint32_t GetNumberOfChars();
  virtual float GetComputedTextLength() MOZ_OVERRIDE;
  virtual float GetSubStringLength(uint32_t charnum, uint32_t fragmentChars) MOZ_OVERRIDE;
  virtual int32_t GetCharNumAtPosition(mozilla::DOMSVGPoint *point) MOZ_OVERRIDE;
  NS_IMETHOD_(nsSVGGlyphFrame *) GetFirstGlyphFrame() MOZ_OVERRIDE;
  NS_IMETHOD_(nsSVGGlyphFrame *) GetNextGlyphFrame() MOZ_OVERRIDE;
  NS_IMETHOD_(void) SetWhitespaceCompression(bool aCompressWhitespace) MOZ_OVERRIDE {
    if (mCompressWhitespace != aCompressWhitespace) {
      mCompressWhitespace = aCompressWhitespace;
      ClearTextRun();
    }
  }

private:

  /**
   * This class exists purely because it would be too messy to pass the "for"
   * flag for GetCanvasTM through the call chains to the GetCanvasTM() call in
   * EnsureTextRun.
   */
  class AutoCanvasTMForMarker {
  public:
    AutoCanvasTMForMarker(nsSVGGlyphFrame *aFrame, uint32_t aFor)
      : mFrame(aFrame)
    {
      mOldFor = mFrame->mGetCanvasTMForFlag;
      mFrame->mGetCanvasTMForFlag = aFor;
    }
    ~AutoCanvasTMForMarker()
    {
      // Default
      mFrame->mGetCanvasTMForFlag = mOldFor;
    }
  private:
    nsSVGGlyphFrame *mFrame;
    uint32_t mOldFor;
  };

  // Use a power of 2 here. It's not so important to match
  // nsDeviceContext::AppUnitsPerDevPixel, but since we do a lot of
  // multiplying by 1/GetTextRunUnitsFactor, it's good for it to be a
  // power of 2 to avoid accuracy loss.
  static uint32_t GetTextRunUnitsFactor() { return 64; }
  
  /**
   * @aParam aDrawScale font drawing must be scaled into user units
   * by this factor
   * @param aMetricsScale font metrics must be scaled into user units
   * by this factor
   * @param aForceGlobalTransform set to true if we should force use of
   * the global transform; otherwise we won't use the global transform
   * if we're a NONDISPLAY_CHILD
   */
  bool EnsureTextRun(float *aDrawScale, float *aMetricsScale,
                       bool aForceGlobalTransform);
  void ClearTextRun();

  bool GetCharacterData(nsAString & aCharacterData);
  bool GetCharacterPositions(nsTArray<CharacterPosition>* aCharacterPositions,
                               float aMetricsScale);
  uint32_t GetTextRunFlags(uint32_t strLength);

  void AddCharactersToPath(CharacterIterator *aIter,
                           gfxContext *aContext);
  void AddBoundingBoxesToPath(CharacterIterator *aIter,
                              gfxContext *aContext);
  void DrawCharacters(CharacterIterator *aIter,
                      gfxContext *aContext,
                      DrawMode aDrawMode,
                      gfxTextObjectPaint *aObjectPaint = nullptr);

  void NotifyGlyphMetricsChange();
  void SetupGlobalTransform(gfxContext *aContext, uint32_t aFor);
  nsresult GetHighlight(uint32_t *charnum, uint32_t *nchars,
                        nscolor *foreground, nscolor *background);
  float GetSubStringAdvance(uint32_t charnum, uint32_t fragmentChars,
                            float aMetricsScale);
  gfxFloat GetBaselineOffset(float aMetricsScale);

  virtual void GetDxDy(SVGUserUnitList *aDx, SVGUserUnitList *aDy);
  virtual const SVGNumberList *GetRotate();

  // Used to support GetBBoxContribution by making GetConvasTM use this as the
  // parent transform instead of the real CanvasTM.
  nsAutoPtr<gfxMatrix> mOverrideCanvasTM;

  // Owning pointer, must call gfxTextRunWordCache::RemoveTextRun before deleting
  gfxTextRun *mTextRun;
  gfxPoint mPosition;
  // The start index into the position and rotation data
  uint32_t mStartIndex;
  uint32_t mGetCanvasTMForFlag;
  bool mCompressWhitespace;
  bool mTrimLeadingWhitespace;
  bool mTrimTrailingWhitespace;

private:
  DrawMode SetupCairoState(gfxContext *aContext,
                           gfxTextObjectPaint *aOuterObjectPaint,
                           gfxTextObjectPaint **aThisObjectPaint);

  // Slightly horrible callback for deferring application of opacity
  struct SVGTextObjectPaint : public gfxTextObjectPaint {
    already_AddRefed<gfxPattern> GetFillPattern(float aOpacity,
                                                const gfxMatrix& aCTM);
    already_AddRefed<gfxPattern> GetStrokePattern(float aOpacity,
                                                  const gfxMatrix& aCTM);

    void SetFillOpacity(float aOpacity) { mFillOpacity = aOpacity; }
    float GetFillOpacity() { return mFillOpacity; }

    void SetStrokeOpacity(float aOpacity) { mStrokeOpacity = aOpacity; }
    float GetStrokeOpacity() { return mStrokeOpacity; }

    struct Paint {
      Paint() {
        mPatternCache.Init();
      }

      void SetPaintServer(nsIFrame *aFrame, const gfxMatrix& aContextMatrix,
                          nsSVGPaintServerFrame *aPaintServerFrame) {
        mPaintType = eStyleSVGPaintType_Server;
        mPaintDefinition.mPaintServerFrame = aPaintServerFrame;
        mFrame = aFrame;
        mContextMatrix = aContextMatrix;
      }

      void SetColor(const nscolor &aColor) {
        mPaintType = eStyleSVGPaintType_Color;
        mPaintDefinition.mColor = aColor;
      }

      void SetObjectPaint(gfxTextObjectPaint *aObjectPaint,
                          nsStyleSVGPaintType aPaintType) {
        NS_ASSERTION(aPaintType == eStyleSVGPaintType_ObjectFill ||
                     aPaintType == eStyleSVGPaintType_ObjectStroke,
                     "Invalid object paint type");
        mPaintType = aPaintType;
        mPaintDefinition.mObjectPaint = aObjectPaint;
      }

      union {
        nsSVGPaintServerFrame *mPaintServerFrame;
        gfxTextObjectPaint *mObjectPaint;
        nscolor mColor;
      } mPaintDefinition;

      nsIFrame *mFrame;
      // CTM defining the user space for the pattern we will use.
      gfxMatrix mContextMatrix;
      nsStyleSVGPaintType mPaintType;

      // Device-space-to-pattern-space
      gfxMatrix mPatternMatrix;
      nsRefPtrHashtable<nsFloatHashKey, gfxPattern> mPatternCache;

      already_AddRefed<gfxPattern> GetPattern(float aOpacity,
                                              nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                              const gfxMatrix& aCTM);
    };

    Paint mFillPaint;
    Paint mStrokePaint;

    float mFillOpacity;
    float mStrokeOpacity;
  };

  /**
   * Sets up the stroke style in |aContext| and stores stroke pattern
   * information in |aThisObjectPaint|.
   */
  bool SetupCairoStroke(gfxContext *aContext,
                        gfxTextObjectPaint *aOuterObjectPaint,
                        SVGTextObjectPaint *aThisObjectPaint);

  /**
   * Sets up the fill style in |aContext| and stores fill pattern information
   * in |aThisObjectPaint|.
   */
  bool SetupCairoFill(gfxContext *aContext,
                      gfxTextObjectPaint *aOuterObjectPaint,
                      SVGTextObjectPaint *aThisObjectPaint);

  /**
   * Sets the current pattern to the fill or stroke style of the outer text
   * object. Will also set the paint opacity to transparent if the paint is set
   * to "none".
   */
  bool SetupObjectPaint(gfxContext *aContext,
                        nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                        float& aOpacity,
                        gfxTextObjectPaint *aObjectPaint);

  /**
   * Stores in |aTargetPaint| information on how to reconstruct the current
   * fill or stroke pattern. Will also set the paint opacity to transparent if
   * the paint is set to "none".
   * @param aOuterObjectPaint pattern information from the outer text object
   * @param aTargetPaint where to store the current pattern information
   * @param aFillOrStroke member pointer to the paint we are setting up
   * @param aProperty the frame property descriptor of the fill or stroke paint
   *   server frame
   */
  void SetupInheritablePaint(gfxContext *aContext,
                             float& aOpacity,
                             gfxTextObjectPaint *aOuterObjectPaint,
                             SVGTextObjectPaint::Paint& aTargetPaint,
                             nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                             const FramePropertyDescriptor *aProperty);

};

#endif
