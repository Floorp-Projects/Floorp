/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorageManager2.h"

#include "LSObject.h"

namespace mozilla {
namespace dom {

LocalStorageManager2::LocalStorageManager2()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(NextGenLocalStorageEnabled());
}

LocalStorageManager2::~LocalStorageManager2()
{
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMPL_ISUPPORTS(LocalStorageManager2,
                  nsIDOMStorageManager,
                  nsILocalStorageManager)

NS_IMETHODIMP
LocalStorageManager2::PrecacheStorage(nsIPrincipal* aPrincipal,
                                      Storage** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LocalStorageManager2::CreateStorage(mozIDOMWindow* aWindow,
                                    nsIPrincipal* aPrincipal,
                                    const nsAString& aDocumentURI,
                                    bool aPrivate,
                                    Storage** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  nsCOMPtr<nsPIDOMWindowInner> inner = nsPIDOMWindowInner::From(aWindow);

  RefPtr<LSObject> object;
  nsresult rv = LSObject::CreateForPrincipal(inner,
                                             aPrincipal,
                                             aDocumentURI,
                                             aPrivate,
                                             getter_AddRefs(object));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  object.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LocalStorageManager2::GetStorage(mozIDOMWindow* aWindow,
                                 nsIPrincipal* aPrincipal,
                                 bool aPrivate,
                                 Storage** _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LocalStorageManager2::CloneStorage(Storage* aStorageToCloneFrom)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aStorageToCloneFrom);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LocalStorageManager2::CheckStorage(nsIPrincipal* aPrincipal,
                                   Storage *aStorage,
                                   bool* _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aStorage);
  MOZ_ASSERT(_retval);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LocalStorageManager2::GetNextGenLocalStorageEnabled(bool* aResult)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aResult);

  *aResult = NextGenLocalStorageEnabled();
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
