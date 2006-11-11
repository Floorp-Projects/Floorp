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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

/* internal interface for observing CSS style sheet loads */

#ifndef nsICSSLoaderObserver_h___
#define nsICSSLoaderObserver_h___

#include "nsISupports.h"

#define NS_ICSSLOADEROBSERVER_IID     \
{ 0xf5e8eb0f, 0x4c44, 0x49d5,       \
{0xb1, 0xe9, 0xab, 0x39, 0x23, 0x93, 0xc0, 0xf8} }

class nsICSSStyleSheet;

class nsICSSLoaderObserver : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSSLOADEROBSERVER_IID)

  /**
   * StyleSheetLoaded is called after aSheet is marked complete and before any
   * load events associated with aSheet are fired.
   * @param aSheet the sheet that was loaded. Guaranteed to always be
   *        non-null, even if aStatus indicates failure.
   * @param aWasAlternate whether the sheet was an alternate.  This will always
   *        match the value LoadStyleLink or LoadInlineStyle returned in
   *        aIsAlternate if one of those methods were used to load the sheet,
   *        and will always be false otherwise.
   * @param aStatus is a success code if the sheet loaded successfully and a
   *        failure code otherwise.  Note that successful load of aSheet
   *        doesn't indicate anything about whether the data actually parsed
   *        as CSS, and doesn't indicate anything about the status of any child
   *        sheets of aSheet.
   */
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aWasAlternate,
                              nsresult aStatus) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSLoaderObserver, NS_ICSSLOADEROBSERVER_IID)

#endif // nsICSSLoaderObserver_h___
