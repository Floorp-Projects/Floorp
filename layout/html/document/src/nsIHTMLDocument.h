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
#ifndef nsIHTMLDocument_h___
#define nsIHTMLDocument_h___

#include "nsISupports.h"
class nsIImageMap;
class nsString;

/* b2a848b0-d0a9-11d1-89b1-006008911b81 */
#define NS_IHTMLDOCUMENT_IID \
{0xb2a848b0, 0xd0a9, 0x11d1, {0x89, 0xb1, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81}}

/**
 * HTML document extensions to nsIDocument.
 */
class nsIHTMLDocument : public nsISupports {
public:
  NS_IMETHOD SetTitle(const nsString& aTitle) = 0;

  NS_IMETHOD AddImageMap(nsIImageMap* aMap) = 0;

  NS_IMETHOD GetImageMap(const nsString& aMapName, nsIImageMap** aResult) = 0;
};

#endif /* nsIHTMLDocument_h___ */
