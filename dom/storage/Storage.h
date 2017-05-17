/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Storage_h
#define mozilla_dom_Storage_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "nsIDOMStorage.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"

class nsIPrincipal;
class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class Storage : public nsIDOMStorage
              , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Storage,
                                                         nsIDOMStorage)

  Storage(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal);

  virtual int64_t GetOriginQuotaUsage() const = 0;

  virtual bool CanAccess(nsIPrincipal* aPrincipal);

  // WebIDL
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mWindow;
  }

  virtual uint32_t
  GetLength(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) = 0;

  virtual void
  Key(uint32_t aIndex, nsAString& aResult,
      nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) = 0;

  virtual void
  GetItem(const nsAString& aKey, nsAString& aResult,
          nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) = 0;

  virtual void
  GetSupportedNames(nsTArray<nsString>& aKeys) = 0;

  void NamedGetter(const nsAString& aKey, bool& aFound, nsAString& aResult,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aRv)
  {
    GetItem(aKey, aResult, aSubjectPrincipal, aRv);
    aFound = !aResult.IsVoid();
  }

  virtual void
  SetItem(const nsAString& aKey, const nsAString& aValue,
          nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) = 0;

  void NamedSetter(const nsAString& aKey, const nsAString& aValue,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aRv)
  {
    SetItem(aKey, aValue, aSubjectPrincipal, aRv);
  }

  virtual void
  RemoveItem(const nsAString& aKey, nsIPrincipal& aSubjectPrincipal,
             ErrorResult& aRv) = 0;

  void NamedDeleter(const nsAString& aKey, bool& aFound,
                    nsIPrincipal& aSubjectPrincipal,
                    ErrorResult& aRv)
  {
    RemoveItem(aKey, aSubjectPrincipal, aRv);

    aFound = !aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION);
  }

  virtual void
  Clear(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) = 0;

  virtual bool IsSessionOnly() const = 0;

protected:
  virtual ~Storage();

  nsIPrincipal*
  Principal() const
  {
    return mPrincipal;
  }

private:
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Storage_h
