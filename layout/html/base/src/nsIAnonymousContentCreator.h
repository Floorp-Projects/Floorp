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


#ifndef nsIAnonymousContentCreator_h___
#define nsIAnonymousContentCreator_h___

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"

class nsIPresContext;
class nsISupportsArray;
class nsIAtom;
class nsIStyleContext;
class nsIFrame;


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
     NS_IMETHOD CreateAnonymousContent(nsIPresContext* aPresContext,
                                       nsISupportsArray& aAnonymousItems)=0;

     // If the creator doesn't want to create special fframe ro frame hierarchy
     // then it should null out the style content arg and return NS_ERROR_FAILURE
     NS_IMETHOD CreateFrameFor(nsIPresContext*   aPresContext,
                               nsIContent *      aContent,
                               nsIFrame**        aFrame)=0;
};

nsresult NS_CreateAnonymousNode(nsIContent* aParent, nsIAtom* aTag, PRInt32 aNameSpaceId, nsCOMPtr<nsIContent>& aNewNode);


#endif

