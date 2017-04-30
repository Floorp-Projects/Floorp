/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

using namespace mozilla::css;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// Implementation

//----------------------------------------------------------------------
// nsISupports methods:

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
SVGDocument::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                   bool aPreallocateChildren) const
{
  NS_ASSERTION(aNodeInfo->NodeInfoManager() == mNodeInfoManager,
               "Can't import this document into another document!");

  RefPtr<SVGDocument> clone = new SVGDocument();
  nsresult rv = CloneDocHelper(clone.get(), aPreallocateChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(clone.get(), aResult);
}

void
SVGDocument::EnsureNonSVGUserAgentStyleSheetsLoaded()
{
  if (mHasLoadedNonSVGUserAgentStyleSheets) {
    return;
  }

  if (IsStaticDocument()) {
    // If we're a static clone of a document, then
    // nsIDocument::CreateStaticClone will handle cloning the original
    // document's sheets, including the on-demand non-SVG UA sheets,
    // for us.
    return;
  }

  mHasLoadedNonSVGUserAgentStyleSheets = true;

  BeginUpdate(UPDATE_STYLE);

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
              RefPtr<StyleSheet> sheet;
              cssLoader->LoadSheetSync(uri,
                                       mozilla::css::eAgentSheetFeatures,
                                       true, &sheet);
              if (sheet) {
                EnsureOnDemandBuiltInUASheet(sheet);
              }
            }
          }
        }
      }
    }
  }

  auto cache = nsLayoutStylesheetCache::For(GetStyleBackendType());

  StyleSheet* sheet = cache->NumberControlSheet();
  if (sheet) {
    // number-control.css can be behind a pref
    EnsureOnDemandBuiltInUASheet(sheet);
  }
  EnsureOnDemandBuiltInUASheet(cache->FormsSheet());
  EnsureOnDemandBuiltInUASheet(cache->CounterStylesSheet());
  EnsureOnDemandBuiltInUASheet(cache->HTMLSheet());
  if (nsLayoutUtils::ShouldUseNoFramesSheet(this)) {
    EnsureOnDemandBuiltInUASheet(cache->NoFramesSheet());
  }
  if (nsLayoutUtils::ShouldUseNoScriptSheet(this)) {
    EnsureOnDemandBuiltInUASheet(cache->NoScriptSheet());
  }
  EnsureOnDemandBuiltInUASheet(cache->UASheet());

  EndUpdate(UPDATE_STYLE);
}

} // namespace dom
} // namespace mozilla

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult
NS_NewSVGDocument(nsIDocument** aInstancePtrResult)
{
  RefPtr<SVGDocument> doc = new SVGDocument();

  nsresult rv = doc->Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  doc.forget(aInstancePtrResult);
  return rv;
}
