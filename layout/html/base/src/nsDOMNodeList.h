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

#ifndef nsDOMNodeList_h__
#define nsDOMNodeList_h__

#include "nsIDOMNodeList.h"
#include "nsIContent.h"
#include "nsIScriptObjectOwner.h"

class nsDOMNodeList : public nsIDOMNodeList, public nsIScriptObjectOwner {
public:
  nsDOMNodeList(nsIContent *aContent);
  virtual ~nsDOMNodeList();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIDOMNodeList interface
  NS_DECL_IDOMNODELIST

  // Called to tell us that the content is going away and that we
  // should drop our (non ref-counted) reference to it
  void ReleaseContent();

private:
  nsIContent *mContent;
  void *mScriptObject;
};


#endif

