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
class nsIURI;
class nsICSSParser;
class nsICSSStyleSheet;
class nsIPresContext;
class nsIContent;
class nsIParser;
class nsIDocument;
class nsIUnicharInputStream;

// IID for the nsIStyleSheetLoader interface {a6cf9101-15b3-11d2-932e-00805f8add32}
#define NS_ICSS_LOADER_IID     \
{0xa6cf9101, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

typedef void (*nsCSSLoaderCallbackFunc)(nsICSSStyleSheet* aSheet, void *aData);

class nsICSSLoader : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_LOADER_IID; return iid; }

  NS_IMETHOD Init(nsIDocument* aDocument) = 0;
  NS_IMETHOD DropDocumentReference(void) = 0; // notification that doc is going away

  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive) = 0;
  NS_IMETHOD SetQuirkMode(PRBool aQuirkMode) = 0;
  NS_IMETHOD SetPreferredSheet(const nsString& aTitle) = 0;

  // Get/Recycle a CSS parser for general use
  NS_IMETHOD GetParserFor(nsICSSStyleSheet* aSheet,
                          nsICSSParser** aParser) = 0;
  NS_IMETHOD RecycleParser(nsICSSParser* aParser) = 0;

  // Load an inline style sheet
  // - if aCompleted is PR_TRUE, the sheet is fully loaded, don't
  //   block for it.
  // - if aCompleted is PR_FALSE, the sheet is still loading and 
  //   will be inserted in the document when complete
  NS_IMETHOD LoadInlineStyle(nsIContent* aElement,
                             nsIUnicharInputStream* aIn, 
                             const nsString& aTitle, 
                             const nsString& aMedia, 
                             PRInt32 aDefaultNameSpaceID,
                             PRInt32 aDocIndex,
                             nsIParser* aParserToUnblock,
                             PRBool& aCompleted) = 0;

  // Load a linked style sheet
  // - if aCompleted is PR_TRUE, the sheet is fully loaded, don't
  //   block for it.
  // - if aCompleted is PR_FALSE, the sheet is still loading and 
  //   will be inserted in the document when complete
  NS_IMETHOD LoadStyleLink(nsIContent* aElement,
                           nsIURI* aURL, 
                           const nsString& aTitle, 
                           const nsString& aMedia, 
                           PRInt32 aDefaultNameSpaceID,
                           PRInt32 aDocIndex,
                           nsIParser* aParserToUnblock,
                           PRBool& aCompleted) = 0;

  // Load a child style sheet (@import)
  NS_IMETHOD LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                            nsIURI* aURL, 
                            const nsString& aMedia,
                            PRInt32 aDefaultNameSpaceID,
                            PRInt32 aSheetIndex) = 0;

  // Load a user agent or user sheet immediately
  // (note that @imports mayl come in asynchronously)
  // - if aCompleted is PR_TRUE, the sheet is fully loaded
  // - if aCompleted is PR_FALSE, the sheet is still loading and 
  //   aCAllback will be called when the sheet is complete
  NS_IMETHOD LoadAgentSheet(nsIURI* aURL, 
                            nsICSSStyleSheet*& aSheet,
                            PRBool& aCompleted,
                            nsCSSLoaderCallbackFunc aCallback,
                            void *aData) = 0;
};

extern NS_HTML nsresult 
NS_NewCSSLoader(nsIDocument* aDocument, nsICSSLoader** aLoader);

extern NS_HTML nsresult 
NS_NewCSSLoader(nsICSSLoader** aLoader);

#endif /* nsICSSLoader_h___ */
