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

#ifndef nsIProperties_h___
#define nsIProperties_h___

#include "nsID.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISupports.h"
#include "nsString.h"

// {1A180F60-93B2-11d2-9B8B-00805F8A16D9}
#define NS_IPROPERTIES_IID \
{ 0x1a180f60, 0x93b2, 0x11d2, \
  { 0x9b, 0x8b, 0x0, 0x80, 0x5f, 0x8a, 0x16, 0xd9 } }

// {2245E573-9464-11d2-9B8B-00805F8A16D9}
NS_DECLARE_ID(kPropertiesCID,
  0x2245e573, 0x9464, 0x11d2, 0x9b, 0x8b, 0x0, 0x80, 0x5f, 0x8a, 0x16, 0xd9);

class nsIProperties : public nsISupports
{
public:
  NS_IMETHOD Load(nsIInputStream* aIn) = 0;
  NS_IMETHOD GetProperty(const nsString& aKey, nsString& aValue) = 0;
  NS_IMETHOD SetProperty(const nsString& aKey, nsString& aNewValue,
                         nsString& aOldValue) = 0;
  NS_IMETHOD Save(nsIOutputStream* aOut, const nsString& aHeader) = 0;
  NS_IMETHOD Subclass(nsIProperties* aSubclass) = 0;
};

#endif /* nsIProperties_h___ */
