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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
 
#include "nsISupports.h"

class nsIEditor;
class nsIDocShell;
class nsISelectionController;

#define NS_IGFXTEXTCONTROLFRAME2_IID \
{/* A744CFC9-2DA8-416d-A058-ADB1D4B3B534*/ \
0xa744cfc9, 0x2da8, 0x416d, \
{ 0xa0, 0x58, 0xad, 0xb1, 0xd4, 0xb3, 0xb5, 0x34 } }

class nsIGfxTextControlFrame2 : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IGFXTEXTCONTROLFRAME2_IID; return iid; }

	NS_IMETHOD    GetEditor(nsIEditor **aEditor) = 0;
  
  NS_IMETHOD    GetTextLength(PRInt32* aTextLength) = 0;
  
  NS_IMETHOD    SetSelectionStart(PRInt32 aSelectionStart) = 0;
  NS_IMETHOD    SetSelectionEnd(PRInt32 aSelectionEnd) = 0;
  
  NS_IMETHOD    SetSelectionRange(PRInt32 aSelectionStart, PRInt32 aSelectionEnd) = 0;
  NS_IMETHOD    GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd) = 0;

  NS_IMETHOD    GetSelectionContr(nsISelectionController **aSelCon) = 0;
};
