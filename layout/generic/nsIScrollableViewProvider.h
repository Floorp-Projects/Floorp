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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsIScrollableViewProvider_h
#define _nsIScrollableViewProvider_h

#include "nsISupports.h"

#define NS_ISCROLLABLEVIEWPROVIDER_IID_STR "2b2e0d30-1dd2-11b2-9169-eb82b04f6775"

#define NS_ISCROLLABLEVIEWPROVIDER_IID \
{0x2b2e0d30, 0x1dd2, 0x11b2, \
{0x91, 0x69, 0xeb, 0x82, 0xb0, 0x4f, 0x67, 0x75}}

class nsIScrollableView;

class nsIScrollableViewProvider : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCROLLABLEVIEWPROVIDER_IID)

  NS_IMETHOD GetScrollableView(nsIPresContext* aContext, nsIScrollableView** aResult)=0;
};

#endif /* _nsIScrollableViewProvider_h */
