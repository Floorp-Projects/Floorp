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

#ifndef nsDOMIterator_h__
#define nsDOMIterator_h__

#include "nsIDOMNodeIterator.h"
#include "nsIContent.h"
#include "nsIScriptObjectOwner.h"

class nsDOMIterator : public nsIDOMNodeIterator, public nsIScriptObjectOwner {
public:
  nsDOMIterator(nsIContent &aContent);
  virtual ~nsDOMIterator();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD ResetScriptObject();

  // nsIDOMIterator interface
  NS_IMETHOD SetFilter(PRInt32 aFilter, PRBool aFilterOn);
  NS_IMETHOD GetLength(PRUint32 *aLength);
  NS_IMETHOD GetCurrentNode(nsIDOMNode **aNode);
  NS_IMETHOD GetNextNode(nsIDOMNode **aNode);
  NS_IMETHOD GetPreviousNode(nsIDOMNode **aNode);
  NS_IMETHOD ToFirst(nsIDOMNode **aNode);
  NS_IMETHOD ToLast(nsIDOMNode **aNode);
  NS_IMETHOD MoveTo(int aNth, nsIDOMNode **aNode);

private:
  nsIContent &mContent;
  PRInt32 mPosition;
  void *mScriptObject;
};


#endif

