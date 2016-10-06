#include "mozilla/dom/DocGroup.h"
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

TabGroup::TabGroup()
{}

TabGroup::~TabGroup()
{
  MOZ_ASSERT(mDocGroups.IsEmpty());
  MOZ_ASSERT(mWindows.IsEmpty());
}

static StaticRefPtr<TabGroup> sChromeTabGroup;

TabGroup*
TabGroup::GetChromeTabGroup()
{
  if (!sChromeTabGroup) {
    sChromeTabGroup = new TabGroup();
    ClearOnShutdown(&sChromeTabGroup);
  }
  return sChromeTabGroup;
}

already_AddRefed<DocGroup>
TabGroup::GetDocGroup(const nsACString& aKey)
{
  RefPtr<DocGroup> docGroup(mDocGroups.GetEntry(aKey)->mDocGroup);
  return docGroup.forget();
}

already_AddRefed<DocGroup>
TabGroup::AddDocument(const nsACString& aKey, nsIDocument* aDocument)
{
  HashEntry* entry = mDocGroups.PutEntry(aKey);
  RefPtr<DocGroup> docGroup;
  if (entry->mDocGroup) {
    docGroup = entry->mDocGroup;
  } else {
    docGroup = new DocGroup(this, aKey);
    entry->mDocGroup = docGroup;
  }

  // Make sure that the hashtable was updated and now contains the correct value
  MOZ_ASSERT(RefPtr<DocGroup>(GetDocGroup(aKey)) == docGroup);

  docGroup->mDocuments.AppendElement(aDocument);

  return docGroup.forget();
}

/* static */ already_AddRefed<TabGroup>
TabGroup::Join(nsPIDOMWindowOuter* aWindow, TabGroup* aTabGroup)
{
  RefPtr<TabGroup> tabGroup = aTabGroup;
  if (!tabGroup) {
    tabGroup = new TabGroup();
  }
  MOZ_ASSERT(!tabGroup->mWindows.Contains(aWindow));
  tabGroup->mWindows.AppendElement(aWindow);
  return tabGroup.forget();
}

void
TabGroup::Leave(nsPIDOMWindowOuter* aWindow)
{
  MOZ_ASSERT(mWindows.Contains(aWindow));
  mWindows.RemoveElement(aWindow);
}

NS_IMPL_ISUPPORTS(TabGroup, nsISupports)

TabGroup::HashEntry::HashEntry(const nsACString* aKey)
  : nsCStringHashKey(aKey), mDocGroup(nullptr)
{}

}
}
