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
#ifndef nsStyleSet_h___
#define nsStyleSet_h___

#include <stdio.h>
#include "nsISupports.h"
#include "nslayout.h"

class nsIAtom;
class nsIStyleRule;
class nsIStyleSheet;
class nsIStyleContext;
class nsIPresContext;
class nsIContent;
class nsIFrame;
class nsIDocument;


// IID for the nsIStyleSet interface {e59396b0-b244-11d1-8031-006008159b5a}
#define NS_ISTYLE_SET_IID     \
{0xe59396b0, 0xb244, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsIStyleSet : public nsISupports {
public:
  // Style sheets are ordered, most significant first
  // NOTE: this is the reverse of the way documents store the sheets
  virtual void AppendOverrideStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual void InsertOverrideStyleSheetAfter(nsIStyleSheet* aSheet,
                                             nsIStyleSheet* aAfterSheet) = 0;
  virtual void InsertOverrideStyleSheetBefore(nsIStyleSheet* aSheet,
                                              nsIStyleSheet* aBeforeSheet) = 0;
  virtual void RemoveOverrideStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual PRInt32 GetNumberOfOverrideStyleSheets() = 0;
  virtual nsIStyleSheet* GetOverrideStyleSheetAt(PRInt32 aIndex) = 0;

  // the ordering of document style sheets is given by the document
  virtual void AddDocStyleSheet(nsIStyleSheet* aSheet, nsIDocument* aDocument) = 0;
  virtual void RemoveDocStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual PRInt32 GetNumberOfDocStyleSheets() = 0;
  virtual nsIStyleSheet* GetDocStyleSheetAt(PRInt32 aIndex) = 0;

  virtual void AppendBackstopStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual void InsertBackstopStyleSheetAfter(nsIStyleSheet* aSheet,
                                             nsIStyleSheet* aAfterSheet) = 0;
  virtual void InsertBackstopStyleSheetBefore(nsIStyleSheet* aSheet,
                                              nsIStyleSheet* aBeforeSheet) = 0;
  virtual void RemoveBackstopStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual PRInt32 GetNumberOfBackstopStyleSheets() = 0;
  virtual nsIStyleSheet* GetBackstopStyleSheetAt(PRInt32 aIndex) = 0;

  // get a style context for a non-pseudo frame
  virtual nsIStyleContext* ResolveStyleFor(nsIPresContext* aPresContext,
                                           nsIContent* aContent,
                                           nsIStyleContext* aParentContext,
                                           PRBool aForceUnique = PR_FALSE) = 0;

  // get a style context for a pseudo-frame (ie: tag = NS_NewAtom(":first-line");
  virtual nsIStyleContext* ResolvePseudoStyleFor(nsIPresContext* aPresContext,
                                                 nsIContent* aParentContent,
                                                 nsIAtom* aPseudoTag,
                                                 nsIStyleContext* aParentContext,
                                                 PRBool aForceUnique = PR_FALSE) = 0;

  // This funtions just like ResolvePseudoStyleFor except that it will
  // return nsnull if there are no explicit style rules for that
  // pseudo element
  virtual nsIStyleContext* ProbePseudoStyleFor(nsIPresContext* aPresContext,
                                               nsIContent* aParentContent,
                                               nsIAtom* aPseudoTag,
                                               nsIStyleContext* aParentContext,
                                               PRBool aForceUnique = PR_FALSE) = 0;

  // Test if style is dependent on content state
  NS_IMETHOD  HasStateDependentStyle(nsIPresContext* aPresContext,
                                     nsIContent*     aContent) = 0;

  // Create frames for the root content element and its child content
  NS_IMETHOD  ConstructRootFrame(nsIPresContext* aPresContext,
                                 nsIContent*     aDocElement,
                                 nsIFrame*&      aFrameSubTree) = 0;

  // Causes reconstruction of a frame hierarchy rooted by the
  // frame document element frame. This is often called when radical style
  // change precludes incremental reflow.
  NS_IMETHOD ReconstructDocElementHierarchy(nsIPresContext* aPresContext) = 0;

  // Notifications of changes to the content mpodel
  NS_IMETHOD ContentAppended(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             PRInt32         aNewIndexInContainer) = 0;
  NS_IMETHOD ContentInserted(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInContainer) = 0;
  NS_IMETHOD ContentReplaced(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aOldChild,
                             nsIContent*     aNewChild,
                             PRInt32         aIndexInContainer) = 0;
  NS_IMETHOD ContentRemoved(nsIPresContext*  aPresContext,
                            nsIContent*      aContainer,
                            nsIContent*      aChild,
                            PRInt32          aIndexInContainer) = 0;

  NS_IMETHOD ContentChanged(nsIPresContext*  aPresContext,
                            nsIContent* aContent,
                            nsISupports* aSubContent) = 0;
  NS_IMETHOD ContentStatesChanged(nsIPresContext* aPresContext, 
                                  nsIContent* aContent1,
                                  nsIContent* aContent2) = 0;
  NS_IMETHOD AttributeChanged(nsIPresContext*  aPresContext,
                              nsIContent* aChild,
                              nsIAtom* aAttribute,
                              PRInt32 aHint) = 0; // See nsStyleConsts fot hint values

  // Style change notifications
  NS_IMETHOD StyleRuleChanged(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) = 0; // See nsStyleConsts fot hint values
  NS_IMETHOD StyleRuleAdded(nsIPresContext* aPresContext,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) = 0;
  NS_IMETHOD StyleRuleRemoved(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) = 0;

  // Notification that we were unable to render a replaced element.
  // Called when the replaced element can not be rendered, and we should
  // instead render the element's contents.
  // The content object associated with aFrame should either be a IMG
  // element or an OBJECT element.
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame) = 0;

  // Request to create a continuing frame
  NS_IMETHOD CreateContinuingFrame(nsIPresContext* aPresContext,
                                   nsIFrame*       aFrame,
                                   nsIFrame*       aParentFrame,
                                   nsIFrame**      aContinuingFrame) = 0;

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) = 0;
  virtual void ListContexts(nsIStyleContext* aRootContext, FILE* out = stdout, PRInt32 aIndent = 0) = 0;
};

extern NS_LAYOUT nsresult
  NS_NewStyleSet(nsIStyleSet** aInstancePtrResult);

#endif /* nsIStyleSet_h___ */
