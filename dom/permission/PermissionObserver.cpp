/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PermissionObserver.h"

#include "mozilla/dom/PermissionStatus.h"
#include "mozilla/Services.h"
#include "mozilla/UniquePtr.h"
#include "nsIObserverService.h"
#include "nsIPermission.h"
#include "PermissionUtils.h"

namespace mozilla {
namespace dom {

namespace {
PermissionObserver* gInstance = nullptr;
} // namespace

NS_IMPL_ISUPPORTS(PermissionObserver,
                  nsIObserver,
                  nsISupportsWeakReference)

PermissionObserver::PermissionObserver()
{
  MOZ_ASSERT(!gInstance);
}

PermissionObserver::~PermissionObserver()
{
  MOZ_ASSERT(mSinks.IsEmpty());
  MOZ_ASSERT(gInstance == this);

  gInstance = nullptr;
}

/* static */ already_AddRefed<PermissionObserver>
PermissionObserver::GetInstance()
{
  RefPtr<PermissionObserver> instance = gInstance;
  if (!instance) {
    instance = new PermissionObserver();

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (NS_WARN_IF(!obs)) {
      return nullptr;
    }

    nsresult rv = obs->AddObserver(instance, "perm-changed", true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    gInstance = instance;
  }

  return instance.forget();
}

void
PermissionObserver::AddSink(PermissionStatus* aSink)
{
  MOZ_ASSERT(aSink);
  MOZ_ASSERT(!mSinks.Contains(aSink));

  mSinks.AppendElement(aSink);
}

void
PermissionObserver::RemoveSink(PermissionStatus* aSink)
{
  MOZ_ASSERT(aSink);
  MOZ_ASSERT(mSinks.Contains(aSink));

  mSinks.RemoveElement(aSink);
}

void
PermissionObserver::Notify(PermissionName aName, nsIPrincipal& aPrincipal)
{
  for (auto* sink : mSinks) {
    if (sink->mName != aName) {
      continue;
    }

    nsIPrincipal* sinkPrincipal = sink->GetPrincipal();
    if (NS_WARN_IF(!sinkPrincipal) || !aPrincipal.Equals(sinkPrincipal)) {
      continue;
    }

    sink->PermissionChanged();
  }
}

NS_IMETHODIMP
PermissionObserver::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const char16_t* aData)
{
  MOZ_ASSERT(!strcmp(aTopic, "perm-changed"));

  if (mSinks.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIPermission> perm = do_QueryInterface(aSubject);
  if (!perm) {
    return NS_OK;
  }

  nsCOMPtr<nsIPrincipal> principal;
  perm->GetPrincipal(getter_AddRefs(principal));
  if (!principal) {
    return NS_OK;
  }

  nsAutoCString type;
  perm->GetType(type);
  Maybe<PermissionName> permission = TypeToPermissionName(type.get());
  if (permission) {
    Notify(permission.value(), *principal);
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
