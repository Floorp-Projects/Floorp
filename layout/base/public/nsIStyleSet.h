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
class nsIStyleMolecule;
class nsIStyleRule;
class nsIStyleSheet;
class nsIStyleContext;
class nsIPresContext;
class nsIContent;
class nsIFrame;


// IID for the nsIStyleSet interface {e59396b0-b244-11d1-8031-006008159b5a}
#define NS_ISTYLE_SET_IID     \
{0xe59396b0, 0xb244, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsIStyleSet : public nsISupports {
public:
  virtual void AppendOverrideStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual void InsertOverrideStyleSheetAfter(nsIStyleSheet* aSheet,
                                             nsIStyleSheet* aAfterSheet) = 0;
  virtual void InsertOverrideStyleSheetBefore(nsIStyleSheet* aSheet,
                                              nsIStyleSheet* aBeforeSheet) = 0;
  virtual void RemoveOverrideStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual PRInt32 GetNumberOfOverrideStyleSheets() = 0;
  virtual nsIStyleSheet* GetOverrideStyleSheetAt(PRInt32 aIndex) = 0;

  virtual void AppendDocStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual void InsertDocStyleSheetAfter(nsIStyleSheet* aSheet,
                                        nsIStyleSheet* aAfterSheet) = 0;
  virtual void InsertDocStyleSheetBefore(nsIStyleSheet* aSheet,
                                         nsIStyleSheet* aBeforeSheet) = 0;
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
                                           nsIFrame* aParentFrame) = 0;

  // get a style context for a pseudo-frame (ie: tag = NS_NewAtom(":FIRST-LINE");
  virtual nsIStyleContext* ResolvePseudoStyleFor(nsIPresContext* aPresContext,
                                                 nsIAtom* aPseudoTag,
                                                 nsIFrame* aParentFrame) = 0;

  // This funtions just like ResolvePseudoStyleFor except that it will
  // return nsnull if there are no explicit style rules for that
  // pseudo element
  virtual nsIStyleContext* ProbePseudoStyleFor(nsIPresContext* aPresContext,
                                               nsIAtom* aPseudoTag,
                                               nsIFrame* aParentFrame) = 0;

  // xxx style rules enumeration

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) = 0;
  virtual void ListContexts(FILE* out = stdout, PRInt32 aIndent = 0) = 0;
};

extern NS_LAYOUT nsresult
  NS_NewStyleSet(nsIStyleSet** aInstancePtrResult);

#endif /* nsIStyleSet_h___ */
