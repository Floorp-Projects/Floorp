#include "mozilla/dom/DocGroup.h"
#include "nsIURI.h"
#include "nsIEffectiveTLDService.h"

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
DocGroup::Remove(nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(mWindows.Contains(aWindow));
  mWindows.RemoveElement(aWindow);
}

DocGroup::DocGroup(TabGroup* aTabGroup, const nsACString& aKey)
  : mKey(aKey), mTabGroup(aTabGroup)
{
  // This method does not add itself to mTabGroup->mDocGroups as the caller does it for us
}

DocGroup::~DocGroup()
{
  MOZ_ASSERT(mWindows.IsEmpty());
  mTabGroup->mDocGroups.RemoveEntry(mKey);
}

TabGroup::~TabGroup()
{
  MOZ_ASSERT(mDocGroups.IsEmpty());
}

already_AddRefed<DocGroup>
TabGroup::GetDocGroup(const nsACString& aKey)
{
  RefPtr<DocGroup> docGroup(mDocGroups.GetEntry(aKey)->mDocGroup);
  return docGroup.forget();
}

already_AddRefed<DocGroup>
TabGroup::JoinDocGroup(const nsACString& aKey, nsPIDOMWindowInner* aWindow)
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

  docGroup->mWindows.AppendElement(aWindow);
  return docGroup.forget();
}

TabGroup::HashEntry::HashEntry(const nsACString* aKey)
  : nsCStringHashKey(aKey), mDocGroup(nullptr)
{}

}
}
