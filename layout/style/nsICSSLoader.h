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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsICSSLoader_h___
#define nsICSSLoader_h___

#include "nslayout.h"
#include "nsISupports.h"

class nsIAtom;
class nsString;
class nsIURL;
class nsICSSParser;
class nsICSSStyleSheet;
class nsIPresContext;
class nsIHTMLContent;
class nsIParser;
class nsIDocument;
class nsIUnicharInputStream;

// IID for the nsIStyleSheetLoader interface {a6cf9101-15b3-11d2-932e-00805f8add32}
#define NS_ICSS_LOADER_IID     \
{0xa6cf9101, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsICSSLoader : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_LOADER_IID; return iid; }

  NS_IMETHOD Init(nsIDocument* aDocument) = 0;

  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive) = 0;
  NS_IMETHOD SetPreferredSheet(const nsString& aTitle) = 0;

  NS_IMETHOD GetParserFor(nsICSSStyleSheet* aSheet,
                          nsICSSParser** aParser) = 0;
  NS_IMETHOD RecycleParser(nsICSSParser* aParser) = 0;

  NS_IMETHOD LoadInlineStyle(nsIHTMLContent* aElement,
                             nsIUnicharInputStream* aIn, 
                             const nsString& aTitle, 
                             const nsString& aMedia, 
                             PRInt32 aDocIndex,
                             nsIParser* aParserToUnblock,
                             PRBool& aCompleted) = 0;

  NS_IMETHOD LoadStyleLink(nsIHTMLContent* aElement,
                           nsIURL* aURL, 
                           const nsString& aTitle, 
                           const nsString& aMedia, 
                           PRInt32 aDocIndex,
                           nsIParser* aParserToUnblock,
                           PRBool& aCompleted) = 0;

  NS_IMETHOD LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                            nsIURL* aURL, 
                            const nsString& aMedia,
                            PRInt32 aSheetIndex) = 0;
};

extern NS_HTML nsresult 
NS_NewCSSLoader(nsIDocument* aDocument, nsICSSLoader** aLoader);

extern NS_HTML nsresult 
NS_NewCSSLoader(nsICSSLoader** aLoader);

#endif /* nsICSSLoader_h___ */
