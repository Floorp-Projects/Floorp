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
#include "nsIPresContext.h"

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
class nsIFrameManager;

class nsFrameConstructorState;
class nsIDOMHTMLSelectElement;

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

  NS_IMETHOD ReconstructDocElementHierarchy(nsIPresContext* aPresContext);

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
  NS_IMETHOD ContentStatesChanged(nsIPresContext* aPresContext, 
                                  nsIContent* aContent1,
                                  nsIContent* aContent2);
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

  // Request to find the primary frame associated with a given content object.
  // This is typically called by the pres shell when there is no mapping in
  // the pres shell hash table
  NS_IMETHOD FindPrimaryFrameFor(nsIPresContext*  aPresContext,
                                 nsIFrameManager* aFrameManager,
                                 nsIContent*      aContent,
                                 nsIFrame**       aFrame);

  NS_IMETHOD CreateTreeWidgetContent(nsIPresContext* aPresContext,
                                        nsIFrame*       aParentFrame,
                                        nsIFrame*       aPrevFrame,
                                        nsIContent*     aChild,
                                        nsIFrame**      aResult,
                                        PRBool          aIsAppend,
                                        PRBool          aIsScrollbar);
  NS_IMETHOD RemoveMappingsForFrameSubtree(nsIPresContext* aParentFrame,
                                           nsIFrame*       aRemovedFrame);

protected:

  nsresult ResolveStyleContext(nsIPresContext*   aPresContext,
                               nsIFrame*         aParentFrame,
                               nsIContent*       aContent,
                               nsIAtom*          aTag,
                               nsIStyleContext** aStyleContext);

  nsresult ConstructFrame(nsIPresContext*          aPresContext,
                          nsFrameConstructorState& aState,
                          nsIContent*              aContent,
                          nsIFrame*                aParentFrame,
                          PRBool                   aHaveFirstLetterStyle,
                          nsFrameItems&            aFrameItems);

  nsresult ConstructDocElementFrame(nsIPresContext*          aPresContext,
                                    nsFrameConstructorState& aState,
                                    nsIContent*              aDocElement,
                                    nsIFrame*                aParentFrame,
                                    nsIStyleContext*         aParentStyleContext,
                                    nsIFrame*&               aNewFrame);

  nsresult ConstructDocElementTableFrame(nsIPresContext* aPresContext,
                                         nsIContent*     aDocElement,
                                         nsIFrame*       aParentFrame,
                                         nsIFrame*&      aNewTableFrame);

  nsresult CreateGeneratedFrameFor(nsIPresContext*       aPresContext,
                                   nsIFrame*             aParentFrame,
                                   nsIContent*           aContent,
                                   nsIStyleContext*      aStyleContext,
                                   const nsStyleContent* aStyleContent,
                                   PRUint32              aContentIndex,
                                   nsIFrame**            aFrame);

  PRBool CreateGeneratedContentFrame(nsIPresContext*  aPresContext,
                                     nsFrameConstructorState& aState,
                                     nsIFrame*        aFrame,
                                     nsIContent*      aContent,
                                     nsIStyleContext* aStyleContext,
                                     nsIAtom*         aPseudoElement,
                                     PRBool           aMakeFirstLetterFrame,
                                     PRBool           aForBlock,
                                     nsIFrame**       aResult);

  nsresult AppendFrames(nsIPresContext*  aPresContext,
                        nsIPresShell*    aPresShell,
                        nsIFrameManager* aFrameManager,
                        nsIContent*      aContainer,
                        nsIFrame*        aParentFrame,
                        nsIFrame*        aFrameList);

  // BEGIN TABLE SECTION
  nsresult ConstructTableFrame(nsIPresContext*          aPresContext,
                               nsFrameConstructorState& aState,
                               nsIContent*              aContent,
                               nsIFrame*                aParent,
                               nsIStyleContext*         aStyleContext,
                               nsIFrame*&               aNewFrame,
                               nsTableCreator&          aTableCreator);

  nsresult ConstructAnonymousTableFrame(nsIPresContext*          aPresContext, 
                                        nsFrameConstructorState& aState,
                                        nsIContent*              aContent, 
                                        nsIFrame*                aParentFrame,
                                        nsIFrame*&               aNewTopFrame,
                                        nsIFrame*&               aOuterFrame, 
                                        nsIFrame*&               aInnerFrame,
                                        nsTableCreator&          aTableCreator);

  nsresult ConstructTableCaptionFrame(nsIPresContext*          aPresContext,
                                      nsFrameConstructorState& aState,
                                      nsIContent*              aContent,
                                      nsIFrame*                aParent,
                                      nsIStyleContext*         aStyleContext,
                                      nsIFrame*&               aNewTopMostFrame,
                                      nsIFrame*&               aNewCaptionFrame,
                                      nsTableCreator&          aTableCreator);

  nsresult ConstructTableGroupFrame(nsIPresContext*          aPresContext,
                                    nsFrameConstructorState& aState,
                                    nsIContent*              aContent,
                                    nsIFrame*                aParent,
                                    nsIStyleContext*         aStyleContext,
                                    PRBool                   aIsRowGroup,
                                    nsIFrame*&               aNewTopFrame,
                                    nsIFrame*&               aNewGroupFrame,
                                    nsTableCreator&          aTableCreator,
                                    nsTableList*             aToDo = nsnull);

  nsresult ConstructTableGroupFrameOnly(nsIPresContext*          aPresContext,
                                        nsFrameConstructorState& aState,
                                        nsIContent*              aContent,
                                        nsIFrame*                aParent,
                                        nsIStyleContext*         aStyleContext,
                                        PRBool                   aIsRowGroup,
                                        nsIFrame*&               aNewTopMostFrame,
                                        nsIFrame*&               aNewGroupFrame,
                                        nsTableCreator&          aTableCreator,
                                        PRBool                   aProcessChildren = PR_TRUE);

  nsresult ConstructTableRowFrame(nsIPresContext*          aPresContext,
                                  nsFrameConstructorState& aState,
                                  nsIContent*              aContent,
                                  nsIFrame*                aParent,
                                  nsIStyleContext*         aStyleContext,
                                  nsIFrame*&               aNewTopMostFrame,
                                  nsIFrame*&               aNewRowFrame,
                                  nsTableCreator&          aTableCreator,
                                  nsTableList*             aToDo = nsnull);

  nsresult ConstructTableRowFrameOnly(nsIPresContext*          aPresContext,
                                      nsFrameConstructorState& aState,
                                      nsIContent*              aContent,
                                      nsIFrame*                aParentFrame,
                                      nsIStyleContext*         aStyleContext,
                                      PRBool                   aProcessChildren,
                                      nsIFrame*&               aNewRowFrame,
                                      nsTableCreator&          aTableCreator);

  nsresult ConstructTableColFrame(nsIPresContext*          aPresContext,
                                  nsFrameConstructorState& aState,
                                  nsIContent*              aContent,
                                  nsIFrame*                aParent,
                                  nsIStyleContext*         aStyleContext,
                                  nsIFrame*&               aNewTopMostFrame,
                                  nsIFrame*&               aNewColFrame,
                                  nsTableCreator&          aTableCreator);

  nsresult ConstructTableColFrameOnly(nsIPresContext*          aPresContext,
                                      nsFrameConstructorState& aState,
                                      nsIContent*              aContent,
                                      nsIFrame*                aParentFrame,
                                      nsIStyleContext*         aStyleContext,
                                      nsIFrame*&               aNewColFrame,
                                      nsTableCreator&          aTableCreator);

  nsresult ConstructTableCellFrame(nsIPresContext*          aPresContext,
                                   nsFrameConstructorState& aState,
                                   nsIContent*              aContent,
                                   nsIFrame*                aParentFrame,
                                   nsIStyleContext*         aStyleContext,
                                   nsIFrame*&               aNewTopMostFrame,
                                   nsIFrame*&               aNewCellFrame,
                                   nsIFrame*&               aNewCellBodyFrame,
                                   nsTableCreator&          aTableCreator,
                                   PRBool                   aProcessChildren = PR_TRUE);

  nsresult ConstructTableCellFrameOnly(nsIPresContext*          aPresContext,
                                       nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsIFrame*                aParentFrame,
                                       nsIStyleContext*         aStyleContext,
                                       nsIFrame*&               aNewCellFrame,
                                       nsIFrame*&               aNewCellBodyFrame,
                                       nsTableCreator&          aTableCreator,
                                       PRBool                   aProcessChildren);

  nsresult TableProcessChildren(nsIPresContext*          aPresContext,
                                nsFrameConstructorState& aState,
                                nsIContent*              aContent,
                                nsIFrame*                aParentFrame,
                                nsFrameItems&            aChildList,
                                nsTableCreator&          aTableCreator);

  nsresult TableProcessChild(nsIPresContext*          aPresContext,
                             nsFrameConstructorState& aState,
                             nsIContent*              aChildContent,
                             nsIFrame*                aParentFrame,
                             nsIStyleContext*         aParentStyleContext,
                             nsFrameItems&            aChildItems,
                             nsTableCreator&          aTableCreator);

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

  PRBool IsTableRelated(PRUint8 aDisplay);
 
  // END TABLE SECTION

  nsresult CreatePlaceholderFrameFor(nsIPresContext*  aPresContext,
                                     nsIFrameManager* aFrameManager,
                                     nsIContent*      aContent,
                                     nsIFrame*        aFrame,
                                     nsIStyleContext* aStyleContext,
                                     nsIFrame*        aParentFrame,
                                     nsIFrame**       aPlaceholderFrame);

  nsresult ConstructAlternateImageFrame(nsIPresContext*  aPresContext,
                                        nsIContent*      aContent,
                                        nsIStyleContext* aStyleContext,
                                        nsIFrame*        aParentFrame,
                                        nsIFrame*&       aFrame);

	nsWidgetRendering GetFormElementRenderingMode(nsIPresContext*       aPresContext,
																								nsWidgetType					aWidgetType);

  nsresult ConstructRadioControlFrame(nsIPresContext*    aPresContext,
                                      nsIFrame*&         aNewFrame,
                                      nsIContent*        aContent,
                                      nsIStyleContext*   aStyleContext);

  nsresult ConstructCheckboxControlFrame(nsIPresContext*      aPresContext,
                                     nsIFrame*&               aNewFrame);

  nsresult ConstructButtonControlFrame(nsIPresContext*      	aPresContext,
                                     nsIFrame*&               aNewFrame);

  nsresult ConstructTextControlFrame(nsIPresContext*          aPresContext,
                                     nsIFrame*&               aNewFrame);

  
  nsresult ConstructSelectFrame(nsIPresContext*          aPresContext,
                                nsFrameConstructorState& aState,
                                nsIContent*              aContent,
                                nsIFrame*                aParentFrame,
                                nsIAtom*                 aTag,
                                nsIStyleContext*         aStyleContext,
                                nsIFrame*&               aNewFrame,
                                PRBool&                  aProcessChildren,
                                PRBool                   aIsAbsolutelyPositioned,
                                PRBool&                  aFrameHasBeenInitialized,
                                PRBool                   aIsFixedPositioned,
                                nsFrameItems&            aFrameItems);

  nsresult ConstructFrameByTag(nsIPresContext*          aPresContext,
                               nsFrameConstructorState& aState,
                               nsIContent*              aContent,
                               nsIFrame*                aParentFrame,
                               nsIAtom*                 aTag,
                               nsIStyleContext*         aStyleContext,
                               nsFrameItems&            aFrameItems);

  nsresult CreateAnonymousFrames(nsIPresContext*          aPresContext,
                                             nsIAtom*                 aTag,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aContent,
                                             nsIFrame*                aNewFrame,
                                             nsFrameItems&            aChildItems);


#ifdef INCLUDE_XUL
  nsresult ConstructXULFrame(nsIPresContext*          aPresContext,
                             nsFrameConstructorState& aState,
                             nsIContent*              aContent,
                             nsIFrame*                aParentFrame,
                             nsIAtom*                 aTag,
                             nsIStyleContext*         aStyleContext,
                             nsFrameItems&            aFrameItems,
                             PRBool&                  aHaltProcessing);

#endif

  nsresult ConstructFrameByDisplayType(nsIPresContext*          aPresContext,
                                       nsFrameConstructorState& aState,
                                       const nsStyleDisplay*    aDisplay,
                                       nsIContent*              aContent,
                                       nsIFrame*                aParentFrame,
                                       nsIStyleContext*         aStyleContext,
                                       PRBool                   aHaveFirstLetterStyle,
                                       nsFrameItems&            aFrameItems);

  nsresult GetAdjustedParentFrame(nsIFrame*  aCurrentParentFrame, 
                                  PRUint8    aChildDisplayType,
                                  nsIFrame*& aNewParentFrame);


  nsresult ProcessChildren(nsIPresContext*          aPresContext,
                           nsFrameConstructorState& aState,
                           nsIContent*              aContent,
                           nsIFrame*                aFrame,
                           PRBool                   aCanHaveGeneratedContent,
                           nsFrameItems&            aFrameItems);

  nsresult CreateInputFrame(nsIPresContext  *aPresContext,
                            nsIContent      *aContent, 
                            nsIFrame        *&aFrame,
                            nsIStyleContext *aStyleContext);

  nsresult RemoveDummyFrameFromSelect(nsIPresContext* aPresContext,
                                      nsIPresShell *  aPresShell,
                                      nsIContent*     aContainer,
                                      nsIContent*     aChild,
                                      nsIDOMHTMLSelectElement * aSelectElement);

  PRBool IsScrollable(nsIPresContext* aPresContext, const nsStyleDisplay* aDisplay);

  nsIFrame* GetFrameFor(nsIPresShell*   aFrameManager,
                        nsIPresContext* aPresContext,
                        nsIContent*     aContent);

  nsIFrame* GetAbsoluteContainingBlock(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame);

  nsIFrame* GetFloaterContainingBlock(nsIPresContext* aPresContext,
                                      nsIFrame*       aFrame);

  nsresult InitializeScrollFrame(nsIPresContext*          aPresContext,
                                 nsFrameConstructorState& aState,
                                 nsIFrame*                aScrollFrame,
                                 nsIContent*              aContent,
                                 nsIFrame*                aParentFrame,
                                 nsIStyleContext*         aStyleContext,
                                 nsIFrame*&               aNewFrame,
                                 PRBool                   aIsAbsolutelyPositioned,
                                 PRBool                   aIsFixedPositioned,
                                 PRBool                   aCreateBlock);

  nsresult RecreateFramesForContent(nsIPresContext* aPresContext,
                                    nsIContent* aContent);

  PRInt32 FindRestyledFramesBelow(nsIFrame* aFrame, 
                                  nsIPresContext* aPresContext,
                                  PRInt32 aParentHint,
                                  nsStyleChangeList& aResults);

  PRInt32 ComputeStateChangeFor(nsIPresContext* aPresContext, nsIFrame* aFrame, 
                                nsStyleChangeList& aChangeList,
                                PRInt32 aFrameChange);

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

  //----------------------------------------

  // Methods support creating block frames and their children

  PRBool HaveFirstLetterStyle(nsIPresContext* aPresContext,
                              nsIContent* aContent,
                              nsIStyleContext* aStyleContext);

  nsIStyleContext* GetFirstLetterStyle(nsIPresContext* aPresContext,
                                       nsIContent* aContent,
                                       nsIStyleContext* aStyleContext);

  PRBool ShouldCreateFirstLetterFrame(nsIPresContext* aPresContext,
                                      nsIContent*      aContent,
                                      nsIFrame*        aFrame);

  nsresult ProcessBlockChildren(nsIPresContext*          aPresContext,
                                nsFrameConstructorState& aState,
                                nsIContent*              aContent,
                                nsIFrame*                aFrame,
                                nsFrameItems&            aFrameItems,
                                PRBool                   aCanHaveGeneratedContent,
                                PRBool                   aForBlock);

  nsresult WrapTextFrame(nsIPresContext* aPresContext,
                         nsFrameConstructorState& aState,
                         nsIFrame* aTextFrame,
                         nsIContent* aParentContent,
                         nsIContent* aChildContent,
                         nsIFrame* aParentFrame,
                         nsFrameItems& aFrameItems,
                         nsAbsoluteItems& aFloatingItems,
                         PRBool aForBlock);

  void CreateFloatingFirstLetterFrame(nsIPresContext* aPresContext,
                                      nsIFrameManager* aFrameManager,
                                      nsIFrame* aTextFrame,
                                      nsIContent* aContent,
                                      nsIContent* aChildContent,
                                      nsIFrame* aParentFrame,
                                      nsFrameItems& aFrameItems,
                                      nsFrameItems& aFloatingItems,
                                      nsIStyleContext* aStyleContext);

protected:
  nsIDocument*        mDocument;

  nsIFrame*           mInitialContainingBlock;
  nsIFrame*           mFixedContainingBlock;
  nsIFrame*           mDocElementContainingBlock;
};

#endif /* nsCSSFrameConstructor_h___ */
