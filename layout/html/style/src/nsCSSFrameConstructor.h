/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsCSSFrameConstructor_h___
#define nsCSSFrameConstructor_h___

#include "nsIStyleFrameConstruction.h"
#include "nslayout.h"

class nsIDocument;
struct nsFrameItems;
struct nsAbsoluteItems;
struct nsTableCreator;
class nsIStyleContext;
struct nsTableList;
struct nsStyleContent;
struct nsStyleDisplay;
class nsIPresShell;
class nsVoidArray;

class nsCSSFrameConstructor : public nsIStyleFrameConstruction {
public:
  nsCSSFrameConstructor(void);
  virtual ~nsCSSFrameConstructor(void);

private: 
  // These are not supported and are not implemented! 
  nsCSSFrameConstructor(const nsCSSFrameConstructor& aCopy); 
  nsCSSFrameConstructor& operator=(const nsCSSFrameConstructor& aCopy); 

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIDocument* aDocument);

  // nsIStyleFrameConstruction API
  NS_IMETHOD ConstructRootFrame(nsIPresContext* aPresContext,
                                nsIContent*     aDocElement,
                                nsIFrame*&      aNewFrame);

  NS_IMETHOD  ReconstructDocElementHierarchy(nsIPresContext* aPresContext);

  NS_IMETHOD ContentAppended(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             PRInt32         aNewIndexInContainer);

  NS_IMETHOD ContentInserted(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInContainer);

  NS_IMETHOD ContentReplaced(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aOldChild,
                             nsIContent*     aNewChild,
                             PRInt32         aIndexInContainer);

  NS_IMETHOD ContentRemoved(nsIPresContext* aPresContext,
                            nsIContent*     aContainer,
                            nsIContent*     aChild,
                            PRInt32         aIndexInContainer);

  NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentStateChanged(nsIPresContext* aPresContext, 
                                 nsIContent* aContent);
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aContent,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

  // Style change notifications
  NS_IMETHOD StyleRuleChanged(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint); // See nsStyleConsts fot hint values
  NS_IMETHOD StyleRuleAdded(nsIPresContext* aPresContext,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule);
  NS_IMETHOD StyleRuleRemoved(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);

  NS_IMETHOD ProcessRestyledFrames(nsStyleChangeList& aRestyleArray, 
                                   nsIPresContext* aPresContext);

  // Notification that we were unable to render a replaced element.
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame);

  // Request to create a continuing frame
  NS_IMETHOD CreateContinuingFrame(nsIPresContext* aPresContext,
                                   nsIFrame*       aFrame,
                                   nsIFrame*       aParentFrame,
                                   nsIFrame**      aContinuingFrame);

protected:

  nsresult ResolveStyleContext(nsIPresContext*   aPresContext,
                               nsIFrame*         aParentFrame,
                               nsIContent*       aContent,
                               nsIAtom*          aTag,
                               nsIStyleContext** aStyleContext);

  nsresult ConstructFrame(nsIPresContext*  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParentFrame,
                          nsAbsoluteItems& aAbsoluteItems,
                          nsFrameItems&    aFrameItems,
                          nsAbsoluteItems& aFixedItems,
                          nsAbsoluteItems& aFloatingItems);

  nsresult ConstructDocElementFrame(nsIPresContext*  aPresContext,
                                    nsIContent*      aDocElement,
                                    nsIFrame*        aParentFrame,
                                    nsIStyleContext* aParentStyleContext,
                                    nsIFrame*&       aNewFrame,
                                    nsAbsoluteItems& aFixedItems);

  nsresult CreateGeneratedFrameFor(nsIPresContext*       aPresContext,
                                   nsIFrame*             aParentFrame,
                                   nsIContent*           aContent,
                                   nsIStyleContext*      aStyleContext,
                                   const nsStyleContent* aStyleContent,
                                   PRUint32              aContentIndex,
                                   nsIFrame**            aFrame);

  PRBool CreateGeneratedContentFrame(nsIPresContext*  aPresContext,
                                     nsIFrame*        aFrame,
                                     nsIContent*      aContent,
                                     nsIStyleContext* aStyleContext,
                                     nsIAtom*         aPseudoElement,
                                     nsIFrame**       aResult);

  // BEGIN TABLE SECTION
  nsresult ConstructTableFrame(nsIPresContext*  aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParent,
                               nsIStyleContext* aStyleContext,
                               nsAbsoluteItems& aAboluteItems,
                               nsIFrame*&       aNewFrame,
                               nsAbsoluteItems& aFixedItems,
                               nsTableCreator&  aTableCreator);

  nsresult ConstructAnonymousTableFrame(nsIPresContext*  aPresContext, 
                                        nsIContent*      aContent, 
                                        nsIFrame*        aParentFrame,
                                        nsIFrame*&       aNewTopFrame,
                                        nsIFrame*&       aOuterFrame, 
                                        nsIFrame*&       aInnerFrame,
                                        nsAbsoluteItems& aAbsoluteItems,
                                        nsAbsoluteItems& aFixedItems,
                                        nsTableCreator&  aTableCreator);

  nsresult ConstructTableCaptionFrame(nsIPresContext*  aPresContext,
                                      nsIContent*      aContent,
                                      nsIFrame*        aParent,
                                      nsIStyleContext* aStyleContext,
                                      nsAbsoluteItems& aAbsoluteItems,
                                      nsIFrame*&       aNewTopMostFrame,
                                      nsIFrame*&       aNewCaptionFrame,
                                      nsAbsoluteItems& aFixedItems,
                                      nsTableCreator&  aTableCreator);

  nsresult ConstructTableGroupFrame(nsIPresContext*  aPresContext,
                                    nsIContent*      aContent,
                                    nsIFrame*        aParent,
                                    nsIStyleContext* aStyleContext,
                                    nsAbsoluteItems& aAbsoluteItems,
                                    PRBool           aIsRowGroup,
                                    nsIFrame*&       aNewTopFrame,
                                    nsIFrame*&       aNewGroupFrame,
                                    nsAbsoluteItems& aFixedItems,
                                    nsTableCreator&  aTableCreator,
                                    nsTableList*     aToDo = nsnull);

  nsresult ConstructTableGroupFrameOnly(nsIPresContext*  aPresContext,
                                        nsIContent*      aContent,
                                        nsIFrame*        aParent,
                                        nsIStyleContext* aStyleContext,
                                        nsAbsoluteItems& aAbsoluteItems,
                                        PRBool           aIsRowGroup,
                                        nsIFrame*&       aNewTopMostFrame,
                                        nsIFrame*&       aNewGroupFrame,
                                        nsAbsoluteItems& aFixedItems,
                                        nsTableCreator&  aTableCreator,
                                        PRBool           aProcessChildren = PR_TRUE);

  nsresult ConstructTableRowFrame(nsIPresContext*  aPresContext,
                                  nsIContent*      aContent,
                                  nsIFrame*        aParent,
                                  nsIStyleContext* aStyleContext,
                                  nsAbsoluteItems& aAbsoluteItems,
                                  nsIFrame*&       aNewTopMostFrame,
                                  nsIFrame*&       aNewRowFrame,
                                  nsAbsoluteItems& aFixedItems,
                                  nsTableCreator&  aTableCreator,
                                  nsTableList*     aToDo = nsnull);

  nsresult ConstructTableRowFrameOnly(nsIPresContext*  aPresContext,
                                      nsIContent*      aContent,
                                      nsIFrame*        aParentFrame,
                                      nsIStyleContext* aStyleContext,
                                      nsAbsoluteItems& aAbsoluteItems,
                                      PRBool           aProcessChildren,
                                      nsIFrame*&       aNewRowFrame,
                                      nsAbsoluteItems& aFixedItems,
                                      nsTableCreator&  aTableCreator);

  nsresult ConstructTableColFrame(nsIPresContext*  aPresContext,
                                  nsIContent*      aContent,
                                  nsIFrame*        aParent,
                                  nsIStyleContext* aStyleContext,
                                  nsAbsoluteItems& aAbsoluteItems,
                                  nsIFrame*&       aNewTopMostFrame,
                                  nsIFrame*&       aNewColFrame,
                                  nsAbsoluteItems& aFixedItems,
                                  nsTableCreator&  aTableCreator);

  nsresult ConstructTableColFrameOnly(nsIPresContext*  aPresContext,
                                      nsIContent*      aContent,
                                      nsIFrame*        aParentFrame,
                                      nsIStyleContext* aStyleContext,
                                      nsAbsoluteItems& aAbsoluteItems,
                                      nsIFrame*&       aNewColFrame,
                                      nsAbsoluteItems& aFixedItems,
                                      nsTableCreator&  aTableCreator);

  nsresult ConstructTableCellFrame(nsIPresContext*  aPresContext,
                                   nsIContent*      aContent,
                                   nsIFrame*        aParentFrame,
                                   nsIStyleContext* aStyleContext,
                                   nsAbsoluteItems& aAbsoluteItems,
                                   nsIFrame*&       aNewTopMostFrame,
                                   nsIFrame*&       aNewCellFrame,
                                   nsIFrame*&       aNewCellBodyFrame,
                                   nsAbsoluteItems& aFixedItems,
                                   nsTableCreator&  aTableCreator,
                                   PRBool           aProcessChildren = PR_TRUE);

  nsresult ConstructTableCellFrameOnly(nsIPresContext*  aPresContext,
                                       nsIContent*      aContent,
                                       nsIFrame*        aParentFrame,
                                       nsIStyleContext* aStyleContext,
                                       nsAbsoluteItems& aAbsoluteItems,
                                       nsIFrame*&       aNewCellFrame,
                                       nsIFrame*&       aNewCellBodyFrame,
                                       nsAbsoluteItems& aFixedItems,
                                       nsTableCreator&  aTableCreator,
                                       PRBool           aProcessChildren);

  nsresult TableProcessChildren(nsIPresContext*  aPresContext,
                                nsIContent*      aContent,
                                nsIFrame*        aParentFrame,
                                nsAbsoluteItems& aAbsoluteItems,
                                nsFrameItems&    aChildList,
                                nsAbsoluteItems& aFixedItems,
                                nsTableCreator&  aTableCreator);

  nsresult TableProcessChild(nsIPresContext*  aPresContext,
                             nsIContent*      aChildContent,
                             nsIFrame*        aParentFrame,
                             nsIStyleContext* aParentStyleContext,
                             nsAbsoluteItems& aAbsoluteItems,
                             nsFrameItems&    aChildItems,
                             nsAbsoluteItems& aFixedItems,
                             nsTableCreator&  aTableCreator);

  nsresult TableProcessTableList(nsIPresContext* aPresContext,
                                 nsTableList& aTableList);

  nsIFrame* TableGetAsNonScrollFrame(nsIPresContext*       aPresContext,
                                     nsIFrame*             aFrame, 
                                     const nsStyleDisplay* aDisplayType);

  PRBool TableIsValidCellContent(nsIPresContext* aPresContext,
                                 nsIFrame*       aParentFrame,
                                 nsIContent*     aContent);

  const nsStyleDisplay* GetDisplay(nsIFrame* aFrame);
 
  // END TABLE SECTION

  nsresult CreatePlaceholderFrameFor(nsIPresContext*  aPresContext,
                                     nsIContent*      aContent,
                                     nsIFrame*        aFrame,
                                     nsIStyleContext* aStyleContext,
                                     nsIFrame*        aParentFrame,
                                     nsIFrame*&       aPlaceholderFrame);

  nsresult CreateFloaterPlaceholderFrameFor(nsIPresContext*  aPresContext,
                                            nsIContent*      aContent,
                                            nsIFrame*        aFrame,
                                            nsIStyleContext* aStyleContext,
                                            nsIFrame*        aParentFrame,
                                            nsIFrame**      aPlaceholderFrame);

  nsresult ConstructAlternateImageFrame(nsIPresContext* aPresContext,
                                        nsIContent*     aContent,
                                        nsIFrame*       aParentFrame,
                                        nsIFrame*&      aFrame);

  nsresult ConstructSelectFrame(nsIPresContext*  aPresContext,
                                nsIContent*      aContent,
                                nsIFrame*        aParentFrame,
                                nsIAtom*         aTag,
                                nsIStyleContext* aStyleContext,
                                nsAbsoluteItems& aAbsoluteItems,
                                nsIFrame*&       aNewFrame,
                                PRBool &         aProcessChildren,
                                PRBool           aIsAbsolutelyPositioned,
                                PRBool &         aFrameHasBeenInitialized,
                                PRBool           aIsFixedPositioned,
                                nsAbsoluteItems& aFixedItems);

  nsresult ConstructFrameByTag(nsIPresContext*  aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParentFrame,
                               nsIAtom*         aTag,
                               nsIStyleContext* aStyleContext,
                               nsAbsoluteItems& aAbsoluteItems,
                               nsFrameItems&    aFrameItems,
                               nsAbsoluteItems& aFixedItems,
                               nsAbsoluteItems& aFloatingItems);

#ifdef INCLUDE_XUL
  nsresult ConstructXULFrame(nsIPresContext*  aPresContext,
                             nsIContent*      aContent,
                             nsIFrame*        aParentFrame,
                             nsIAtom*         aTag,
                             nsIStyleContext* aStyleContext,
                             nsAbsoluteItems& aAbsoluteItems,
                             nsFrameItems&    aFrameItems,
                             nsAbsoluteItems& aFixedItems,
                             nsAbsoluteItems& aFloatingItems,
                             PRBool&          aHaltProcessing);
#endif

  nsresult ConstructFrameByDisplayType(nsIPresContext*       aPresContext,
                                       const nsStyleDisplay* aDisplay,
                                       nsIContent*           aContent,
                                       nsIFrame*             aParentFrame,
                                       nsIStyleContext*      aStyleContext,
                                       nsAbsoluteItems&      aAbsoluteItems,
                                       nsFrameItems&         aFrameItems,
                                       nsAbsoluteItems&      aFixedItems,
                                       nsAbsoluteItems&      aFloatingItems);

  nsresult GetAdjustedParentFrame(nsIFrame*  aCurrentParentFrame, 
                                  PRUint8    aChildDisplayType,
                                  nsIFrame*& aNewParentFrame);


  nsresult ProcessChildren(nsIPresContext*  aPresContext,
                           nsIContent*      aContent,
                           nsIFrame*        aFrame,
                           nsAbsoluteItems& aAbsoluteItems,
                           nsFrameItems&    aFrameItems,
                           nsAbsoluteItems& aFixedItems,
                           nsAbsoluteItems& aFloatingItems,
                           PRBool           aCanHaveGeneratedContent = PR_FALSE);

  nsresult CreateInputFrame(nsIContent* aContent, nsIFrame*& aFrame);

  PRBool IsScrollable(nsIPresContext* aPresContext, const nsStyleDisplay* aDisplay);

  nsIFrame* GetFrameFor(nsIPresShell* aPresShell, nsIPresContext* aPresContext,
                        nsIContent* aContent);

  nsIFrame* GetAbsoluteContainingBlock(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame);

  nsIFrame* GetFloaterContainingBlock(nsIPresContext* aPresContext,
                                      nsIFrame*       aFrame);

  nsresult InitializeScrollFrame(nsIFrame *       aScrollFrame,
                                 nsIPresContext*  aPresContext,
                                 nsIContent*      aContent,
                                 nsIFrame*        aParentFrame,
                                 nsIStyleContext* aStyleContext,
                                 nsAbsoluteItems& aAbsoluteItems,
                                 nsIFrame*&       aNewFrame,
                                 nsAbsoluteItems& aFixedItems,
                                 PRBool           aIsAbsolutelyPositioned,
                                 PRBool           aIsFixedPositioned,
                                 PRBool           aCreateBlock);

  nsresult RecreateFramesForContent(nsIPresContext* aPresContext,
                                    nsIContent* aContent);

  PRInt32 FindRestyledFramesBelow(nsIFrame* aFrame, 
                                  nsIPresContext* aPresContext,
                                  PRInt32 aParentHint,
                                  nsStyleChangeList& aResults);

  nsresult RecreateFramesOnAttributeChange(nsIPresContext* aPresContext,
                                           nsIContent* aContent,
                                           nsIAtom* aAttribute);

  nsresult CreateContinuingOuterTableFrame(nsIPresContext*  aPresContext,
                                           nsIFrame*        aFrame,
                                           nsIFrame*        aParentFrame,
                                           nsIContent*      aContent,
                                           nsIStyleContext* aStyleContext,
                                           nsIFrame**       aContinuingFrame);

  nsresult CreateContinuingTableFrame(nsIPresContext*  aPresContext,
                                      nsIFrame*        aFrame,
                                      nsIFrame*        aParentFrame,
                                      nsIContent*      aContent,
                                      nsIStyleContext* aStyleContext,
                                      nsIFrame**       aContinuingFrame);

protected:
  nsIDocument*        mDocument;

  nsIFrame*           mInitialContainingBlock;
  nsIFrame*           mFixedContainingBlock;
  nsIFrame*           mDocElementContainingBlock;
};

#endif /* nsCSSFrameConstructor_h___ */
