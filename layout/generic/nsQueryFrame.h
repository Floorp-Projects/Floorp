/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsQueryFrame_h
#define nsQueryFrame_h

#include "nscore.h"

#define NS_DECL_QUERYFRAME_TARGET(classname)                    \
  static const nsQueryFrame::FrameIID kFrameIID = nsQueryFrame::classname##_id;

#define NS_DECL_QUERYFRAME                                      \
  virtual void* QueryFrame(FrameIID id);

#define NS_QUERYFRAME_HEAD(class)                               \
  void* class::QueryFrame(FrameIID id) { switch (id) {

#define NS_QUERYFRAME_ENTRY(class)                              \
  case class::kFrameIID: return static_cast<class*>(this);

#define NS_QUERYFRAME_ENTRY_CONDITIONAL(class, condition)       \
  case class::kFrameIID:                                        \
  if (condition) return static_cast<class*>(this);              \
  break;

#define NS_QUERYFRAME_TAIL_INHERITING(class)                    \
  default: break;                                               \
  }                                                             \
  return class::QueryFrame(id);                                 \
}

#define NS_QUERYFRAME_TAIL_INHERITANCE_ROOT                     \
  default: break;                                               \
  }                                                             \
  return nsnull;                                                \
}

class nsQueryFrame
{
public:
  enum FrameIID {
    BRFrame_id,
    nsAutoRepeatBoxFrame_id,
    nsBCTableCellFrame_id,
    nsBlockFrame_id,
    nsBox_id,
    nsBoxFrame_id,
    nsBulletFrame_id,
    nsButtonBoxFrame_id,
    nsCanvasFrame_id,
    nsColumnSetFrame_id,
    nsComboboxControlFrame_id,
    nsComboboxDisplayFrame_id,
    nsContainerFrame_id,
    nsContinuingTextFrame_id,
    nsDeckFrame_id,
    nsDocElementBoxFrame_id,
    nsFieldSetFrame_id,
    nsFileControlFrame_id,
    nsFirstLetterFrame_id,
    nsFirstLineFrame_id,
    nsFormControlFrame_id,
    nsFrame_id,
    nsGfxButtonControlFrame_id,
    nsGfxCheckboxControlFrame_id,
    nsGfxRadioControlFrame_id,
    nsGridRowGroupFrame_id,
    nsGridRowLeafFrame_id,
    nsGroupBoxFrame_id,
    nsHTMLButtonControlFrame_id,
    nsHTMLCanvasFrame_id,
    nsHTMLFramesetBlankFrame_id,
    nsHTMLFramesetBorderFrame_id,
    nsHTMLFramesetFrame_id,
    nsHTMLScrollFrame_id,
    nsIAnonymousContentCreator_id,
    nsIComboboxControlFrame_id,
    nsIFormControlFrame_id,
    nsIFrame_id,
    nsIFrameFrame_id,
    nsIListControlFrame_id,
    nsIMathMLFrame_id,
    nsIMenuFrame_id,
    nsIObjectFrame_id,
    nsIPageSequenceFrame_id,
    nsIPercentHeightObserver_id,
    nsIRootBox_id,
    nsISVGChildFrame_id,
    nsISVGGlyphFragmentLeaf_id,
    nsISVGGlyphFragmentNode_id,
    nsISVGSVGFrame_id,
    nsIScrollableFrame_id,
    nsIScrollbarMediator_id,
    nsISelectControlFrame_id,
    nsIStatefulFrame_id,
    nsITableCellLayout_id,
    nsITableLayout_id,
    nsITextControlFrame_id,
    nsITreeBoxObject_id,
    nsImageBoxFrame_id,
    nsImageControlFrame_id,
    nsImageFrame_id,
    nsInlineFrame_id,
    nsLeafBoxFrame_id,
    nsLeafFrame_id,
    nsLegendFrame_id,
    nsListBoxBodyFrame_id,
    nsListControlFrame_id,
    nsListItemFrame_id,
    nsMathMLContainerFrame_id,
    nsMathMLFrame_id,
    nsMathMLmactionFrame_id,
    nsMathMLmathBlockFrame_id,
    nsMathMLmathInlineFrame_id,
    nsMathMLmencloseFrame_id,
    nsMathMLmfencedFrame_id,
    nsMathMLmfracFrame_id,
    nsMathMLmmultiscriptsFrame_id,
    nsMathMLmoFrame_id,
    nsMathMLmoverFrame_id,
    nsMathMLmpaddedFrame_id,
    nsMathMLmphantomFrame_id,
    nsMathMLmrootFrame_id,
    nsMathMLmrowFrame_id,
    nsMathMLmspaceFrame_id,
    nsMathMLmsqrtFrame_id,
    nsMathMLmstyleFrame_id,
    nsMathMLmsubFrame_id,
    nsMathMLmsubsupFrame_id,
    nsMathMLmsupFrame_id,
    nsMathMLmtableFrame_id,
    nsMathMLmtableOuterFrame_id,
    nsMathMLmtdFrame_id,
    nsMathMLmtdInnerFrame_id,
    nsMathMLmtrFrame_id,
    nsMathMLmunderFrame_id,
    nsMathMLmunderoverFrame_id,
    nsMathMLsemanticsFrame_id,
    nsMathMLTokenFrame_id,
    nsMenuBarFrame_id,
    nsMenuFrame_id,
    nsMenuPopupFrame_id,
    nsObjectFrame_id,
    nsPageBreakFrame_id,
    nsPageContentFrame_id,
    nsPageFrame_id,
    nsPlaceholderFrame_id,
    nsPopupSetFrame_id,
    nsProgressFrame_id,
    nsProgressMeterFrame_id,
    nsResizerFrame_id,
    nsRootBoxFrame_id,
    nsScrollbarButtonFrame_id,
    nsScrollbarFrame_id,
    nsSelectsAreaFrame_id,
    nsSimplePageSequenceFrame_id,
    nsSliderFrame_id,
    nsSplittableFrame_id,
    nsSplitterFrame_id,
    nsStackFrame_id,
    nsSubDocumentFrame_id,
    nsSVGAFrame_id,
    nsSVGClipPathFrame_id,
    nsSVGContainerFrame_id,
    nsSVGDisplayContainerFrame_id,
    SVGFEContainerFrame_id,
    SVGFEImageFrame_id,
    SVGFELeafFrame_id,
    SVGFEUnstyledLeafFrame_id,
    nsSVGFilterFrame_id,
    nsSVGForeignObjectFrame_id,
    nsSVGGenericContainerFrame_id,
    nsSVGGeometryFrame_id,
    nsSVGGFrame_id,
    nsSVGGlyphFrame_id,
    nsSVGGradientFrame_id,
    nsSVGImageFrame_id,
    nsSVGInnerSVGFrame_id,
    nsSVGLinearGradientFrame_id,
    nsSVGMarkerFrame_id,
    nsSVGMaskFrame_id,
    nsSVGOuterSVGFrame_id,
    nsSVGPaintServerFrame_id,
    nsSVGPathGeometryFrame_id,
    nsSVGPatternFrame_id,
    nsSVGRadialGradientFrame_id,
    nsSVGStopFrame_id,
    nsSVGSwitchFrame_id,
    nsSVGTextContainerFrame_id,
    nsSVGTextFrame_id,
    nsSVGTextPathFrame_id,
    nsSVGTSpanFrame_id,
    nsSVGUseFrame_id,
    nsTableCaptionFrame_id,
    nsTableCellFrame_id,
    nsTableColFrame_id,
    nsTableColGroupFrame_id,
    nsTableFrame_id,
    nsTableOuterFrame_id,
    nsTableRowFrame_id,
    nsTableRowGroupFrame_id,
    nsTextBoxFrame_id,
    nsTextControlFrame_id,
    nsTextFrame_id,
    nsTitleBarFrame_id,
    nsTreeBodyFrame_id,
    nsTreeColFrame_id,
    nsVideoFrame_id,
    nsXULLabelFrame_id,
    nsXULScrollFrame_id,
    ViewportFrame_id,

    // The PresArena implementation uses this bit to distinguish objects
    // allocated by size from objects allocated by type ID (that is, frames
    // using AllocateByFrameID, and other objects using AllocateByObjectID).
    // It should not collide with any frame ID (above) or Object ID (in
    // nsPresArena.h).  It is not 0x80000000 to avoid the question of
    // whether enumeration constants are signed.
    NON_FRAME_MARKER = 0x20000000
  };

  virtual void* QueryFrame(FrameIID id) = 0;
};

class do_QueryFrame
{
public:
  do_QueryFrame(nsQueryFrame *s) : mRawPtr(s) { }

  template<class Dest>
  operator Dest*() {
    if (!mRawPtr)
      return nsnull;

    return reinterpret_cast<Dest*>(mRawPtr->QueryFrame(Dest::kFrameIID));
  }

private:
  nsQueryFrame *mRawPtr;
};

#endif // nsQueryFrame_h
