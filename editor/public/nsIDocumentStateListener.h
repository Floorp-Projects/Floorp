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
#ifndef nsIDocumentStateListener_h___
#define nsIDocumentStateListener_h___


#define NS_IDOCUMENTSTATELISTENER_IID            \
{ /* 050cdc00-3b8e-11d3-9ce4-a458f454fcbc */     \
0x050cdc00, 0x3b8e, 0x11d3,                      \
{0x9c, 0xe4, 0xa4, 0x58, 0xf4, 0x54, 0xfc, 0xbc} }


// A simple listener interface that is implemented by those who want to
// get notified when the dirty state of the document changes.
// To get hooked up as a document state listener, called
// AddDocumentStateListener on nsIEditor.

class nsIDocumentStateListener : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOCUMENTSTATELISTENER_IID; return iid; }
  
  NS_IMETHOD  NotifyDocumentStateChanged(PRBool aNowDirty) = 0;
  
};


#endif // nsIDocumentStateListener_h___


