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

#ifndef nsIHTMLEditRules_h__
#define nsIHTMLEditRules_h__

#define NS_IHTMLEDITRULES_IID \
{ /* a6cf9121-15b3-11d2-932e-00805f8add32 */ \
0xa6cf9121, 0x15b3, 0x11d2, \
{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

#include "nsIHTMLEditor.h"

class nsIHTMLEditRules : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IHTMLEDITRULES_IID; return iid; }
  
  NS_IMETHOD GetListState(PRBool *aMixed, PRBool *aOL, PRBool *aUL, PRBool *aDL)=0;
  NS_IMETHOD GetListItemState(PRBool *aMixed, PRBool *aLI, PRBool *aDT, PRBool *aDD)=0;
  NS_IMETHOD GetIndentState(PRBool *aCanIndent, PRBool *aCanOutdent)=0;
  NS_IMETHOD GetAlignment(PRBool *aMixed, nsIHTMLEditor::EAlignment *aAlign)=0;
  NS_IMETHOD GetParagraphState(PRBool *aMixed, nsAWritableString &outFormat)=0;
};


#endif //nsIHTMLEditRules_h__
