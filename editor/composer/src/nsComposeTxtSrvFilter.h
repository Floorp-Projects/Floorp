/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsComposeTxtSrvFilter_h__
#define nsComposeTxtSrvFilter_h__

#include "nsITextServicesFilter.h"
#include "nsIAtom.h"

/**
 * This class implements a filter interface, that enables
 * those using it to skip over certain nodes when traversing content
 *
 * This filter is used to skip over various form control nodes and
 * mail's cite nodes
 */
class nsComposeTxtSrvFilter : public nsITextServicesFilter
{
public:
  nsComposeTxtSrvFilter();
  virtual ~nsComposeTxtSrvFilter() {}

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsITextServicesFilter 
  NS_DECL_NSITEXTSERVICESFILTER

  // Helper - Intializer
  void Init(PRBool aIsForMail) { mIsForMail = aIsForMail; }

protected:
  PRBool            mIsForMail;
  nsCOMPtr<nsIAtom> mBlockQuoteAtom;
  nsCOMPtr<nsIAtom> mPreAtom;          // mail plain text quotes are wrapped in pre tags
  nsCOMPtr<nsIAtom> mSpanAtom;         //or they may be wrapped in span tags (editor.quotesPreformatted). 
  nsCOMPtr<nsIAtom> mMozQuoteAtom;     // _moz_quote_
  nsCOMPtr<nsIAtom> mTableAtom;
  nsCOMPtr<nsIAtom> mClassAtom;
  nsCOMPtr<nsIAtom> mTypeAtom;
  nsCOMPtr<nsIAtom> mScriptAtom;
  nsCOMPtr<nsIAtom> mTextAreaAtom;
  nsCOMPtr<nsIAtom> mSelectAreaAtom;
  nsCOMPtr<nsIAtom> mMapAtom;
  nsCOMPtr<nsIAtom> mCiteAtom;
  nsCOMPtr<nsIAtom> mTrueAtom;
  nsCOMPtr<nsIAtom> mMozSignatureAtom;
};

#define NS_COMPOSERTXTSRVFILTER_CID \
{/* {171E72DB-0F8A-412a-8461-E4C927A3A2AC}*/ \
0x171e72db, 0xf8a, 0x412a, \
{ 0x84, 0x61, 0xe4, 0xc9, 0x27, 0xa3, 0xa2, 0xac} } 

#define NS_COMPOSERTXTSRVFILTERMAIL_CID \
{/* {7FBD2146-5FF4-4674-B069-A7BBCE66E773}*/ \
0x7fbd2146, 0x5ff4, 0x4674, \
{ 0xb0, 0x69, 0xa7, 0xbb, 0xce, 0x66, 0xe7, 0x73} } 

// Generic for the editor
#define COMPOSER_TXTSRVFILTER_CONTRACTID     "@mozilla.org/editor/txtsrvfilter;1"

// This is the same but includes "cite" typed blocked quotes
#define COMPOSER_TXTSRVFILTERMAIL_CONTRACTID "@mozilla.org/editor/txtsrvfiltermail;1"

#endif
