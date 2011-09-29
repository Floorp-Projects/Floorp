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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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

#include "nsIStyleRuleProcessor.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIXBLService.h"
#include "nsIServiceManager.h"
#include "nsXBLResourceLoader.h"
#include "nsXBLPrototypeResources.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIDocumentObserver.h"
#include "mozilla/css/Loader.h"
#include "nsIURI.h"
#include "nsLayoutCID.h"
#include "nsCSSRuleProcessor.h"
#include "nsStyleSet.h"

nsXBLPrototypeResources::nsXBLPrototypeResources(nsXBLPrototypeBinding* aBinding)
{
  MOZ_COUNT_CTOR(nsXBLPrototypeResources);

  mLoader = new nsXBLResourceLoader(aBinding, this);
  NS_IF_ADDREF(mLoader);
}

nsXBLPrototypeResources::~nsXBLPrototypeResources()
{
  MOZ_COUNT_DTOR(nsXBLPrototypeResources);
  if (mLoader) {
    mLoader->mResources = nsnull;
    NS_RELEASE(mLoader);
  }
}

void 
nsXBLPrototypeResources::AddResource(nsIAtom* aResourceType, const nsAString& aSrc)
{
  if (mLoader)
    mLoader->AddResource(aResourceType, aSrc);
}
 
void
nsXBLPrototypeResources::LoadResources(bool* aResult)
{
  if (mLoader)
    mLoader->LoadResources(aResult);
  else
    *aResult = PR_TRUE; // All resources loaded.
}

void
nsXBLPrototypeResources::AddResourceListener(nsIContent* aBoundElement) 
{
  if (mLoader)
    mLoader->AddResourceListener(aBoundElement);
}

static bool IsChromeURI(nsIURI* aURI)
{
  bool isChrome=false;
  if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome)
    return PR_TRUE;
  return PR_FALSE;
}

nsresult
nsXBLPrototypeResources::FlushSkinSheets()
{
  if (mStyleSheetList.Length() == 0)
    return NS_OK;

  nsCOMPtr<nsIDocument> doc =
    mLoader->mBinding->XBLDocumentInfo()->GetDocument();
  mozilla::css::Loader* cssLoader = doc->CSSLoader();

  // We have scoped stylesheets.  Reload any chrome stylesheets we
  // encounter.  (If they aren't skin sheets, it doesn't matter, since
  // they'll still be in the chrome cache.
  mRuleProcessor = nsnull;

  sheet_array_type oldSheets(mStyleSheetList);
  mStyleSheetList.Clear();

  for (sheet_array_type::size_type i = 0, count = oldSheets.Length();
       i < count; ++i) {
    nsCSSStyleSheet* oldSheet = oldSheets[i];

    nsIURI* uri = oldSheet->GetSheetURI();

    nsRefPtr<nsCSSStyleSheet> newSheet;
    if (IsChromeURI(uri)) {
      if (NS_FAILED(cssLoader->LoadSheetSync(uri, getter_AddRefs(newSheet))))
        continue;
    }
    else {
      newSheet = oldSheet;
    }

    mStyleSheetList.AppendElement(newSheet);
  }
  mRuleProcessor = new nsCSSRuleProcessor(mStyleSheetList, 
                                          nsStyleSet::eDocSheet);

  return NS_OK;
}
