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

#ifndef nsIEditorStyleSheets_h__
#define nsIEditorStyleSheets_h__


#include "nsISupports.h"

class nsString;
class nsICSSStyleSheet;

#define NS_IEDITORSTYLESHEETS_IID                  \
{ /* 4805e682-49b9-11d3-9ce4-ed60bd6cb5bc} */      \
0x4805e682, 0x49b9, 0x11d3,                        \
{ 0x9c, 0xe4, 0xed, 0x60, 0xbd, 0x6c, 0xb5, 0xbc } }


class nsIEditorStyleSheets : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITORSTYLESHEETS_IID; return iid; }

  /** load and apply the style sheet, specified by aURL, to
    * the editor's document. This can involve asynchronous
    * network I/O
    * @param aURL  The style sheet to be loaded and applied.
    */
  NS_IMETHOD ApplyStyleSheet(const nsString& aURL)=0;

  /** Add the given Style Sheet to the editor's document
    * This is always synchronous
    * @param aSheet  The style sheet to be  applied.
    */
  NS_IMETHOD AddStyleSheet(nsICSSStyleSheet* aSheet)=0;

  /** Remove the given Style Sheet from the editor's document
    * This is always synchronous
    * @param aSheet  The style sheet to be  applied.
    */
  NS_IMETHOD RemoveStyleSheet(nsICSSStyleSheet* aSheet)=0;

};


#endif // nsIEditorStyleSheets_h__

