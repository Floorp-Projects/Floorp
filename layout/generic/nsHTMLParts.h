/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* factory functions for rendering object classes */

#ifndef nsHTMLParts_h___
#define nsHTMLParts_h___

#include "nscore.h"
#include "nsFrameState.h"
#include "nsISupports.h"

class nsAtom;
class nsCheckboxRadioFrame;
class nsComboboxControlFrame;
class nsContainerFrame;
class nsIChannel;
class nsIContent;
class nsIFragmentContentSink;
class nsIFrame;
class nsIHTMLContentSink;
class nsIURI;
class nsListControlFrame;
class nsNodeInfoManager;
class nsTableColFrame;
namespace mozilla {
class ComputedStyle;
class PresShell;
class PrintedSheetFrame;
class ViewportFrame;

namespace dom {
class Document;
}
}  // namespace mozilla

// Factory methods for creating html layout objects

// Create a frame that supports "display: block" layout behavior
class nsBlockFrame;
nsBlockFrame* NS_NewBlockFrame(mozilla::PresShell* aPresShell,
                               mozilla::ComputedStyle* aStyle);

// Special Generated Content Node. It contains text taken from an
// attribute of its *grandparent* content node.
nsresult NS_NewAttributeContent(nsNodeInfoManager* aNodeInfoManager,
                                int32_t aNameSpaceID, nsAtom* aAttrName,
                                nsAtom* aFallback, nsIContent** aResult);

// Create a basic area frame but the GetFrameForPoint is overridden to always
// return the option frame
// By default, area frames will extend
// their height to cover any children that "stick out".
nsContainerFrame* NS_NewSelectsAreaFrame(mozilla::PresShell* aPresShell,
                                         mozilla::ComputedStyle* aStyle,
                                         nsFrameState aFlags);

// Create a block formatting context blockframe
nsBlockFrame* NS_NewBlockFormattingContext(mozilla::PresShell* aPresShell,
                                           mozilla::ComputedStyle* aStyle);

nsIFrame* NS_NewBRFrame(mozilla::PresShell* aPresShell,
                        mozilla::ComputedStyle* aStyle);

nsIFrame* NS_NewCommentFrame(mozilla::PresShell* aPresShell,
                             mozilla::ComputedStyle* aStyle);

// <frame> and <iframe>
nsIFrame* NS_NewSubDocumentFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);
// <frameset>
nsIFrame* NS_NewHTMLFramesetFrame(mozilla::PresShell* aPresShell,
                                  mozilla::ComputedStyle* aStyle);

mozilla::ViewportFrame* NS_NewViewportFrame(mozilla::PresShell* aPresShell,
                                            mozilla::ComputedStyle* aStyle);
class nsCanvasFrame;
nsCanvasFrame* NS_NewCanvasFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewImageFrame(mozilla::PresShell* aPresShell,
                           mozilla::ComputedStyle* aStyle);
class nsInlineFrame;
nsInlineFrame* NS_NewInlineFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewTextFrame(mozilla::PresShell* aPresShell,
                          mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewContinuingTextFrame(mozilla::PresShell* aPresShell,
                                    mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewEmptyFrame(mozilla::PresShell* aPresShell,
                           mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewWBRFrame(mozilla::PresShell* aPresShell,
                         mozilla::ComputedStyle* aStyle);

nsBlockFrame* NS_NewColumnSetWrapperFrame(mozilla::PresShell* aPresShell,
                                          mozilla::ComputedStyle* aStyle,
                                          nsFrameState aStateFlags);
nsContainerFrame* NS_NewColumnSetFrame(mozilla::PresShell* aPresShell,
                                       mozilla::ComputedStyle* aStyle,
                                       nsFrameState aStateFlags);

class nsPageSequenceFrame;
nsPageSequenceFrame* NS_NewPageSequenceFrame(mozilla::PresShell* aPresShell,
                                             mozilla::ComputedStyle* aStyle);

mozilla::PrintedSheetFrame* NS_NewPrintedSheetFrame(
    mozilla::PresShell* aPresShell, mozilla::ComputedStyle* aStyle);

class nsPageFrame;
nsPageFrame* NS_NewPageFrame(mozilla::PresShell* aPresShell,
                             mozilla::ComputedStyle* aStyle);
class nsPageContentFrame;
nsPageContentFrame* NS_NewPageContentFrame(
    mozilla::PresShell* aPresShell, mozilla::ComputedStyle* aStyle,
    already_AddRefed<const nsAtom> aPageName);
nsIFrame* NS_NewPageBreakFrame(mozilla::PresShell* aPresShell,
                               mozilla::ComputedStyle* aStyle);
class nsFirstLetterFrame;
nsFirstLetterFrame* NS_NewFirstLetterFrame(mozilla::PresShell* aPresShell,
                                           mozilla::ComputedStyle* aStyle);
class nsFirstLineFrame;
nsFirstLineFrame* NS_NewFirstLineFrame(mozilla::PresShell* aPresShell,
                                       mozilla::ComputedStyle* aStyle);

// forms
nsContainerFrame* NS_NewGfxButtonControlFrame(mozilla::PresShell* aPresShell,
                                              mozilla::ComputedStyle* aStyle);
nsCheckboxRadioFrame* NS_NewCheckboxRadioFrame(mozilla::PresShell* aPresShell,
                                               mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewImageControlFrame(mozilla::PresShell* aPresShell,
                                  mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewHTMLButtonControlFrame(mozilla::PresShell* aPresShell,
                                               mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewFieldSetFrame(mozilla::PresShell* aPresShell,
                                      mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewFileControlFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewColorControlFrame(mozilla::PresShell* aPresShell,
                                  mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewTextControlFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);
nsListControlFrame* NS_NewListControlFrame(mozilla::PresShell* aPresShell,
                                           mozilla::ComputedStyle* aStyle);
nsComboboxControlFrame* NS_NewComboboxControlFrame(
    mozilla::PresShell* aPresShell, mozilla::ComputedStyle* aStyle,
    nsFrameState aFlags);
nsIFrame* NS_NewProgressFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMeterFrame(mozilla::PresShell* aPresShell,
                           mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewRangeFrame(mozilla::PresShell* aPresShell,
                           mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewNumberControlFrame(mozilla::PresShell* aPresShell,
                                   mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewDateTimeControlFrame(mozilla::PresShell* aPresShell,
                                     mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewSearchControlFrame(mozilla::PresShell* aPresShell,
                                   mozilla::ComputedStyle* aStyle);

// Table frame factories
class nsTableWrapperFrame;
nsTableWrapperFrame* NS_NewTableWrapperFrame(mozilla::PresShell* aPresShell,
                                             mozilla::ComputedStyle* aStyle);
class nsTableFrame;
nsTableFrame* NS_NewTableFrame(mozilla::PresShell* aPresShell,
                               mozilla::ComputedStyle* aStyle);
nsTableColFrame* NS_NewTableColFrame(mozilla::PresShell* aPresShell,
                                     mozilla::ComputedStyle* aStyle);
class nsTableColGroupFrame;
nsTableColGroupFrame* NS_NewTableColGroupFrame(mozilla::PresShell* aPresShell,
                                               mozilla::ComputedStyle* aStyle);
class nsTableRowFrame;
nsTableRowFrame* NS_NewTableRowFrame(mozilla::PresShell* aPresShell,
                                     mozilla::ComputedStyle* aStyle);
class nsTableRowGroupFrame;
nsTableRowGroupFrame* NS_NewTableRowGroupFrame(mozilla::PresShell* aPresShell,
                                               mozilla::ComputedStyle* aStyle);
class nsTableCellFrame;
nsTableCellFrame* NS_NewTableCellFrame(mozilla::PresShell* aPresShell,
                                       mozilla::ComputedStyle* aStyle,
                                       nsTableFrame* aTableFrame);

nsresult NS_NewHTMLContentSink(nsIHTMLContentSink** aInstancePtrResult,
                               mozilla::dom::Document* aDoc, nsIURI* aURL,
                               nsISupports* aContainer,  // e.g. docshell
                               nsIChannel* aChannel);
nsresult NS_NewHTMLFragmentContentSink(
    nsIFragmentContentSink** aInstancePtrResult);
nsresult NS_NewHTMLFragmentContentSink2(
    nsIFragmentContentSink** aInstancePtrResult);

#endif /* nsHTMLParts_h___ */
