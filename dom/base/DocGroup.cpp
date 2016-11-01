#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/Telemetry.h"
#include "nsIURI.h"
#include "nsIEffectiveTLDService.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsIDocShell.h"

namespace mozilla {
namespace dom {

/* static */ void
DocGroup::GetKey(nsIPrincipal* aPrincipal, nsACString& aKey)
{
  aKey.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
    if (tldService) {
      rv = tldService->GetBaseDomain(uri, 0, aKey);
      if (NS_FAILED(rv)) {
        aKey.Truncate();
      }
    }
  }
}

void
DocGroup::RemoveDocument(nsIDocument* aDocument)
{
  MOZ_ASSERT(mDocuments.Contains(aDocument));
  mDocuments.RemoveElement(aDocument);
}

DocGroup::DocGroup(TabGroup* aTabGroup, const nsACString& aKey)
  : mKey(aKey), mTabGroup(aTabGroup)
{
  // This method does not add itself to mTabGroup->mDocGroups as the caller does it for us.
}

DocGroup::~DocGroup()
{
  MOZ_ASSERT(mDocuments.IsEmpty());
  mTabGroup->mDocGroups.RemoveEntry(mKey);
}

NS_IMPL_ISUPPORTS(DocGroup, nsISupports)

}
}
