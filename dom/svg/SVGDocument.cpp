/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGDocument.h"

#include "mozilla/css/Loader.h"
#include "nsICategoryManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIStyleSheetService.h"
#include "nsISupportsPrimitives.h"
#include "nsLayoutStylesheetCache.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsLiteralString.h"
#include "nsIDOMSVGElement.h"
#include "mozilla/dom/Element.h"
#include "nsSVGElement.h"
#include "mozilla/dom/SVGDocumentBinding.h"

using namespace mozilla::css;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// Implementation

//----------------------------------------------------------------------
// nsISupports methods:

void
SVGDocument::GetDomain(nsAString& aDomain, ErrorResult& aRv)
{
  SetDOMStringToNull(aDomain);

  if (mDocumentURI) {
    nsAutoCString domain;
    nsresult rv = mDocumentURI->GetHost(domain);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
    if (domain.IsEmpty()) {
      return;
    }
    CopyUTF8toUTF16(domain, aDomain);
  }
}

nsSVGElement*
SVGDocument::GetRootElement(ErrorResult& aRv)
{
  Element* root = nsDocument::GetRootElement();
  if (!root) {
    return nullptr;
  }
  if (!root->IsSVGElement()) {
    aRv.Throw(NS_NOINTERFACE);
    return nullptr;
  }
  return static_cast<nsSVGElement*>(root);
}

nsresult
SVGDocument::InsertChildAt(nsIContent* aKid, uint32_t aIndex, bool aNotify)
{
  if (aKid->IsElement() && !aKid->IsSVGElement()) {
    // We can get here when well formed XML with a non-SVG root element is
    // served with the SVG MIME type, for example. In that case we need to load
    // the non-SVG UA sheets or else we can get bugs like bug 1016145.  Note
    // that we have to do this _before_ the XMLDocument::InsertChildAt call,
    // since that can try to construct frames, and we need to have the sheets
    // loaded by then.
    EnsureNonSVGUserAgentStyleSheetsLoaded();
  }

  return XMLDocument::InsertChildAt(aKid, aIndex, aNotify);
}

nsresult
SVGDocument::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const
{
  NS_ASSERTION(aNodeInfo->NodeInfoManager() == mNodeInfoManager,
               "Can't import this document into another document!");

  nsRefPtr<SVGDocument> clone = new SVGDocument();
  nsresult rv = CloneDocHelper(clone.get());
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(clone.get(), aResult);
}

void
SVGDocument::EnsureNonSVGUserAgentStyleSheetsLoaded()
{
  if (mHasLoadedNonSVGUserAgentStyleSheets) {
    return;
  }

  mHasLoadedNonSVGUserAgentStyleSheets = true;

  if (IsBeingUsedAsImage()) {
    // nsDocumentViewer::CreateStyleSet skipped loading all user-agent/user
    // style sheets in this case, but we'll need B2G/Fennec's
    // content.css. We could load all the sheets registered with the
    // nsIStyleSheetService (and maybe we should) but most likely it isn't
    // desirable or necessary for foreignObject in SVG-as-an-image. Instead we
    // only load the "agent-style-sheets" that nsStyleSheetService::Init()
    // pulls in from the category manager. That keeps memory use of
    // SVG-as-an-image down.
    //
    // We do this before adding UASheet() etc. below because
    // EnsureOnDemandBuiltInUASheet prepends, and B2G/Fennec's
    // content.css must come after UASheet() etc.
    nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
    if (catMan) {
      nsCOMPtr<nsISimpleEnumerator> sheets;
      catMan->EnumerateCategory("agent-style-sheets", getter_AddRefs(sheets));
      if (sheets) {
        bool hasMore;
        while (NS_SUCCEEDED(sheets->HasMoreElements(&hasMore)) && hasMore) {
          nsCOMPtr<nsISupports> sheet;
          if (NS_FAILED(sheets->GetNext(getter_AddRefs(sheet))))
            break;

          nsCOMPtr<nsISupportsCString> icStr = do_QueryInterface(sheet);
          MOZ_ASSERT(icStr,
                     "category manager entries must be nsISupportsCStrings");

          nsAutoCString name;
          icStr->GetData(name);

          nsXPIDLCString spec;
          catMan->GetCategoryEntry("agent-style-sheets", name.get(),
                                   getter_Copies(spec));

          mozilla::css::Loader* cssLoader = CSSLoader();
          if (cssLoader->GetEnabled()) {
            nsCOMPtr<nsIURI> uri;
            NS_NewURI(getter_AddRefs(uri), spec);
            if (uri) {
              nsRefPtr<CSSStyleSheet> cssSheet;
              cssLoader->LoadSheetSync(uri, true, true, getter_AddRefs(cssSheet));
              if (cssSheet) {
                EnsureOnDemandBuiltInUASheet(cssSheet);
              }
            }
          }
        }
      }
    }
  }

  CSSStyleSheet* sheet = nsLayoutStylesheetCache::NumberControlSheet();
  if (sheet) {
    // number-control.css can be behind a pref
    EnsureOnDemandBuiltInUASheet(sheet);
  }
  EnsureOnDemandBuiltInUASheet(nsLayoutStylesheetCache::FormsSheet());
  EnsureOnDemandBuiltInUASheet(nsLayoutStylesheetCache::CounterStylesSheet());
  EnsureOnDemandBuiltInUASheet(nsLayoutStylesheetCache::HTMLSheet());
  EnsureOnDemandBuiltInUASheet(nsLayoutStylesheetCache::UASheet());
}

JSObject*
SVGDocument::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGDocumentBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult
NS_NewSVGDocument(nsIDocument** aInstancePtrResult)
{
  nsRefPtr<SVGDocument> doc = new SVGDocument();

  nsresult rv = doc->Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  doc.forget(aInstancePtrResult);
  return rv;
}
