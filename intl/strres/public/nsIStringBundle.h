/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#ifndef nsIStringBundle_h___
#define nsIStringBundle_h___

#include "nsIFactory.h"
#include "nsILocale.h"
#include "nsIURL.h"
#include "nsString.h"

// {D85A17C0-AA7C-11d2-9B8C-00805F8A16D9}
#define NS_ISTRINGBUNDLEFACTORY_IID \
{ 0xd85a17c0, 0xaa7c, 0x11d2, \
  { 0x9b, 0x8c, 0x0, 0x80, 0x5f, 0x8a, 0x16, 0xd9 } }

// {D85A17C1-AA7C-11d2-9B8C-00805F8A16D9}
#define NS_STRINGBUNDLEFACTORY_CID \
{ 0xd85a17c1, 0xaa7c, 0x11d2, \
  { 0x9b, 0x8c, 0x0, 0x80, 0x5f, 0x8a, 0x16, 0xd9 } }

// {D85A17C2-AA7C-11d2-9B8C-00805F8A16D9}
#define NS_ISTRINGBUNDLE_IID \
{ 0xd85a17c2, 0xaa7c, 0x11d2, \
  { 0x9b, 0x8c, 0x0, 0x80, 0x5f, 0x8a, 0x16, 0xd9 } }

// {D85A17C3-AA7C-11d2-9B8C-00805F8A16D9}
#define NS_STRINGBUNDLE_CID \
{ 0xd85a17c3, 0xaa7c, 0x11d2, \
  { 0x9b, 0x8c, 0x0, 0x80, 0x5f, 0x8a, 0x16, 0xd9 } }

class nsIStringBundle : public nsISupports
{
public:
  NS_IMETHOD GetStringFromID(PRInt32 aID, nsString& aResult) = 0;
  NS_IMETHOD GetStringFromName(const nsString& aName, nsString& aResult) = 0;
};

class nsIStringBundleFactory : public nsIFactory
{
public:
  NS_IMETHOD CreateBundle(nsIURL* aURL, nsILocale* aLocale,
                          nsIStringBundle** aResult) = 0;
};

#endif /* nsIStringBundle_h___ */
