/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsIStyleSheet_h___
#define nsIStyleSheet_h___

#include <stdio.h>
#include "nsISupports.h"

class nsISizeOfHandler;

class nsIAtom;
class nsString;
class nsIURI;
class nsIStyleRule;
class nsISupportsArray;
class nsIPresContext;
class nsIContent;
class nsIDocument;
class nsIStyleContext;
class nsIStyleRuleProcessor;

// IID for the nsIStyleSheet interface {8c4a80a0-ad6a-11d1-8031-006008159b5a}
#define NS_ISTYLE_SHEET_IID     \
{0x8c4a80a0, 0xad6a, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsIStyleSheet : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISTYLE_SHEET_IID; return iid; }

  // basic style sheet data
  NS_IMETHOD GetURL(nsIURI*& aURL) const = 0;
  NS_IMETHOD GetTitle(nsString& aTitle) const = 0;
  NS_IMETHOD GetType(nsString& aType) const = 0;
  NS_IMETHOD GetMediumCount(PRInt32& aCount) const = 0;
  NS_IMETHOD GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const = 0;
  NS_IMETHOD UseForMedium(nsIAtom* aMedium) const = 0;

  NS_IMETHOD GetEnabled(PRBool& aEnabled) const = 0;
  NS_IMETHOD SetEnabled(PRBool aEnabled) = 0;

  // style sheet owner info
  NS_IMETHOD GetParentSheet(nsIStyleSheet*& aParent) const = 0;  // may be null
  NS_IMETHOD GetOwningDocument(nsIDocument*& aDocument) const = 0; // may be null
  NS_IMETHOD SetOwningDocument(nsIDocument* aDocument) = 0;

  // style rule processor access
  NS_IMETHOD GetStyleRuleProcessor(nsIStyleRuleProcessor*& aProcessor,
                                   nsIStyleRuleProcessor* aPrevProcessor) = 0;

  // XXX style rule enumerations

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;

  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize) = 0;
};

#endif /* nsIStyleSheet_h___ */
