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
 * A compatibility wrapper for the nsIGlobalHistory2 interface. It provides
 * the old nsIGlobalHistory interface for extensions which still use it.
 */

// {a772eee4-0464-40d5-a329-a29dfda3791a}
#define NS_GLOBALHISTORY2ADAPTER_CID \
 { 0xa772eee4, 0x0464, 0x405d, { 0xa3, 0x29, 0xa2, 0x9d, 0xfd, 0xa3, 0x79, 0x1a } }

class nsGlobalHistory2Adapter : public nsIGlobalHistory
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLOBALHISTORY

  static NS_METHOD Create(nsISupports *aOuter,
                          REFNSIID aIID,
                          void **aResult);

  static NS_METHOD RegisterSelf(nsIComponentManager* aCompMgr,
                                nsIFile* aPath,
                                const char* aLoaderStr,
                                const char* aType,
                                const nsModuleComponentInfo *aInfo);

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_GLOBALHISTORY2ADAPTER_CID)

private:
  nsGlobalHistory2Adapter();
  ~nsGlobalHistory2Adapter();

  nsresult Init();
  nsCOMPtr<nsIGlobalHistory2> mHistory;
};
