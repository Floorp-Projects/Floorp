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

#ifndef nsEditRules_h__
#define nsEditRules_h__

class nsIEditor;
class nsIDOMSelection;

/** Interface of editing rules.
  *  
  */
class nsEditRules
{
public:
  NS_IMETHOD Init(nsIEditor *aEditor)=0;
  NS_IMETHOD WillDoAction(int aAction, nsIDOMSelection *aSelection, void **aOtherInfo, PRBool *aCancel)=0;
  NS_IMETHOD DidDoAction(int aAction, nsIDOMSelection *aSelection, void **aOtherInfo, nsresult aResult)=0;

};

#endif //nsEditRules_h__

