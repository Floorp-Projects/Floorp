/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Boris Zbarsky
 * <bzbarsky@mit.edu>.  Portions created by Boris Zbarsky are
 * Copyright (C) 2001. All Rights Reserved.
 *
 * Contributor(s):
 */
#ifndef nsIMediaList_h___
#define nsIMediaList_h___

#include "nsISupportsArray.h"

// IID for the nsIMediaList interface {c8c2bbce-1dd1-11b2-a108-f7290a0e6da2}
#define NS_IMEDIA_LIST_IID \
{0xc8c2bbce, 0x1dd1, 0x11b2, {0xa1, 0x08, 0xf7, 0x29, 0x0a, 0x0e, 0x6d, 0xa2}}

class nsIMediaList : public nsISupportsArray {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IMEDIA_LIST_IID; return iid; }

  NS_IMETHOD GetMediaText(nsAWritableString& aMediaText) = 0;
  NS_IMETHOD SetMediaText(nsAReadableString& aMediaText) = 0;

};

extern NS_HTML nsresult 
NS_NewMediaList(nsIMediaList** aInstancePtrResult, const nsAReadableString& aMediaText);

extern NS_HTML nsresult 
NS_NewMediaList(nsIMediaList** aInstancePtrResult);

#endif /* nsICSSLoader_h___ */
