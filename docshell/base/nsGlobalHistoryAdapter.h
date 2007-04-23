/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla gecko code.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <bsmedberg@covad.net>
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "nsIGlobalHistory2.h"
#include "nsIGlobalHistory.h"
#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"

/**
 * A compatibility wrapper for the old nsIGlobalHistory interface. It provides
 * the new nsIGlobalHistory2 interface for embedders who haven't implemented it
 * themselves.
 */

// {2e9b69dd-9087-438c-8b5d-f77b553abefb}
#define NS_GLOBALHISTORYADAPTER_CID \
 { 0x2e9b69dd, 0x9087, 0x438c, { 0x8b, 0x5d, 0xf7, 0x7b, 0x55, 0x3a, 0xbe, 0xfb } }

class nsGlobalHistoryAdapter : public nsIGlobalHistory2
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLOBALHISTORY2

  static NS_METHOD Create(nsISupports *aOuter,
                          REFNSIID aIID,
                          void **aResult);

  static NS_METHOD RegisterSelf(nsIComponentManager* aCompMgr,
                                nsIFile* aPath,
                                const char* aLoaderStr,
                                const char* aType,
                                const nsModuleComponentInfo *aInfo);

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_GLOBALHISTORYADAPTER_CID)

private:
  nsGlobalHistoryAdapter();
  ~nsGlobalHistoryAdapter();

  nsresult Init();
  nsCOMPtr<nsIGlobalHistory> mHistory;
};
