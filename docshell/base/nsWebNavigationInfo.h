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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsWebNavigationInfo_h__
#define nsWebNavigationInfo_h__

#include "nsIWebNavigationInfo.h"
#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "imgILoader.h"
#include "nsStringFwd.h"

// Class ID for webnavigationinfo
#define NS_WEBNAVIGATION_INFO_CID \
 { 0xf30bc0a2, 0x958b, 0x4287,{0xbf, 0x62, 0xce, 0x38, 0xba, 0x0c, 0x81, 0x1e}}

class nsWebNavigationInfo : public nsIWebNavigationInfo
{
public:
  nsWebNavigationInfo() {}
  
  NS_DECL_ISUPPORTS

  NS_DECL_NSIWEBNAVIGATIONINFO

  nsresult Init();

private:
  ~nsWebNavigationInfo() {}
  
  // Check whether aType is supported.  If this method throws, the
  // value of aIsSupported is not changed.
  nsresult IsTypeSupportedInternal(const nsCString& aType,
                                   PRUint32* aIsSupported);
  
  nsCOMPtr<nsICategoryManager> mCategoryManager;
  // XXXbz we only need this because images register for the same
  // contractid as documents, so we can't tell them apart based on
  // contractid.
  nsCOMPtr<imgILoader> mImgLoader;
};

#endif  // nsWebNavigationInfo_h__
