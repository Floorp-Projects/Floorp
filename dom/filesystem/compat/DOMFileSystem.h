/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMFileSystem_h
#define mozilla_dom_DOMFileSystem_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class DirectoryEntry;

class DOMFileSystem final
  : public nsISupports
  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMFileSystem)

  explicit DOMFileSystem(nsIGlobalObject* aGlobalObject);

  nsIGlobalObject*
  GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  GetName(nsAString& aName, ErrorResult& aRv) const
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  already_AddRefed<DirectoryEntry>
  GetRoot(ErrorResult& aRv) const
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

private:
  ~DOMFileSystem();

  nsCOMPtr<nsIGlobalObject> mParent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DOMFileSystem_h
