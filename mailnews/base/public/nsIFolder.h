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

#ifndef nsIFolder_h__
#define nsIFolder_h__

#include "nsICollection.h"
#include "nsString.h"

// XXX regenerate this iid -- I was on a machine w/o an network card
#define NS_IFOLDER_IID                               \
{ /* fc232e90-c1f5-11d2-8612-000000000000 */         \
    0xfc232e90,                                      \
    0xc1f5,                                          \
    0x11d2,                                          \
    {0x86, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} \
}

/**
 * nsIFolder: Call Enumerate to get the sub-folders and folder items (the children).
 */
class nsIFolder : public nsICollection {
public:

  static const nsIID& IID(void) { static nsIID iid = NS_IFOLDER_IID; return iid; }

  // unique identifier for folder:
  NS_IMETHOD GetURI(const char* *name) = 0;

  NS_IMETHOD GetName(nsString& name) = 0;
  NS_IMETHOD SetName(const nsString& name) = 0;

  NS_IMETHOD GetChildNamed(const nsString& name, nsISupports* *result) = 0;

  // XXX does this make sense for virtual folders?
  NS_IMETHOD GetParent(nsIFolder* *parent) = 0;

  // subset of Enumerate such that each element is an nsIFolder
  NS_IMETHOD GetSubFolders(nsIEnumerator* *result) = 0;
};

#endif // nsIFolder_h__
