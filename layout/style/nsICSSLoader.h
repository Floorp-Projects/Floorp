/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsICSSLoader_h___
#define nsICSSLoader_h___

#include "nsISupports.h"
#include "nsAReadableString.h"
#include "nsICSSImportRule.h"

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
class nsICSSLoaderObserver;

// IID for the nsIStyleSheetLoader interface {a6cf9101-15b3-11d2-932e-00805f8add32}
#define NS_ICSS_LOADER_IID     \
{0xa6cf9101, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

typedef void (*nsCSSLoaderCallbackFunc)(nsICSSStyleSheet* aSheet, void *aData, PRBool aDidNotify);

class nsICSSLoader : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_LOADER_IID; return iid; }

  NS_IMETHOD Init(nsIDocument* aDocument) = 0;
  NS_IMETHOD DropDocumentReference(void) = 0; // notification that doc is going away

  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive) = 0;
  NS_IMETHOD SetQuirkMode(PRBool aQuirkMode) = 0;
  NS_IMETHOD SetPreferredSheet(const nsAReadableString& aTitle) = 0;

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
                             PRBool& aCompleted,
                             nsICSSLoaderObserver* aObserver) = 0;

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
                           PRBool& aCompleted,
                           nsICSSLoaderObserver* aObserver) = 0;

  // Load a child style sheet (@import)
  NS_IMETHOD LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                            nsIURI* aURL, 
                            const nsString& aMedia,
                            PRInt32 aDefaultNameSpaceID,
                            PRInt32 aSheetIndex,
                            nsICSSImportRule* aRule) = 0;

  // Load a user agent or user sheet immediately
  // (note that @imports mayl come in asynchronously)
  // - if aCompleted is PR_TRUE, the sheet is fully loaded
  // - if aCompleted is PR_FALSE, the sheet is still loading and 
  //   aCAllback will be called when the sheet is complete
  NS_IMETHOD LoadAgentSheet(nsIURI* aURL, 
                            nsICSSStyleSheet*& aSheet,
                            PRBool& aCompleted,
                            nsICSSLoaderObserver* aObserver) = 0;

  // sets the out-param to the current charset, as set by SetCharset
  NS_IMETHOD GetCharset(/*out*/nsString &aCharsetDest) const = 0;

  // SetCharset will ensure that the charset provided is the preferred charset
  // if an empty string, then it is set to the default charset
  NS_IMETHOD SetCharset(/*in*/ const nsString &aCharsetSrc) = 0;

  // stop loading all sheets
  NS_IMETHOD Stop(void) = 0;

  // stop loading one sheet
  NS_IMETHOD StopLoadingSheet(nsIURI* aURL) = 0;
};

extern NS_EXPORT nsresult 
NS_NewCSSLoader(nsIDocument* aDocument, nsICSSLoader** aLoader);

extern NS_EXPORT nsresult 
NS_NewCSSLoader(nsICSSLoader** aLoader);

#endif /* nsICSSLoader_h___ */
