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
#ifndef nsIStyleRule_h___
#define nsIStyleRule_h___

#include <stdio.h>

#include "nsISupports.h"

class nsISizeOfHandler;

class nsIStyleSheet;
class nsIPresContext;
class nsIContent;
struct nsRuleData;

// IID for the nsIStyleRule interface {40ae5c90-ad6a-11d1-8031-006008159b5a}
#define NS_ISTYLE_RULE_IID     \
{0x40ae5c90, 0xad6a, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsIStyleRule : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISTYLE_RULE_IID; return iid; }

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const = 0;

  // Strength is an out-of-band weighting, useful for mapping CSS ! important
  NS_IMETHOD GetStrength(PRInt32& aStrength) const = 0;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData)=0;

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize) = 0;
#endif
};


#endif /* nsIStyleRule_h___ */
