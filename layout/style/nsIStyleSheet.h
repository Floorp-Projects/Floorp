/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
  NS_IMETHOD_(PRBool) UseForMedium(nsIAtom* aMedium) const = 0;

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

  // If changing the given attribute cannot affect style context, aAffects
  // will be PR_FALSE on return.
  NS_IMETHOD AttributeAffectsStyle(nsIAtom *aAttribute, nsIContent *aContent,
                                   PRBool &aAffects) = 0;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;

  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize) = 0;
#endif

};

#endif /* nsIStyleSheet_h___ */
