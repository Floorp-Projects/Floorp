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


#ifndef nsIAnonymousContentCreator_h___
#define nsIAnonymousContentCreator_h___

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"

class nsISupportsArray;
class nsIAtom;


// {41a69e00-2d6d-11d3-b033-a1357139787c}
#define NS_IANONYMOUS_CONTENT_CREATOR_IID { 0x41a69e00, 0x2d6d, 0x11d3, { 0xb0, 0x33, 0xa1, 0x35, 0x71, 0x39, 0x78, 0x7c } }


/**
 * Any source for anonymous content can implement this interface to provide it.
 * HTML frames like nsFileControlFrame currently use this as well as XUL frames
 * like nsScrollbarFrame & nsSliderFrame.
 */
class nsIAnonymousContentCreator : public nsISupports {
public:
     static const nsIID& GetIID() { static nsIID iid = NS_IANONYMOUS_CONTENT_CREATOR_IID; return iid; }
     NS_IMETHOD CreateAnonymousContent(nsISupportsArray& aAnonymousItems)=0;
};

nsresult NS_CreateAnonymousNode(nsIContent* aParent, nsIAtom* aTag, PRInt32 aNameSpaceId, nsCOMPtr<nsIContent>& aNewNode);


#endif

