/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WorkletGlobalScope_h
#define mozilla_dom_WorkletGlobalScope_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsIGlobalObject.h"
#include "nsWrapperCache.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {

class WorkletGlobalScope final : public nsIGlobalObject
                               , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WorkletGlobalScope)

  WorkletGlobalScope()
  {}

  nsIGlobalObject* GetParentObject() const
  {
    return nullptr;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool
  WrapGlobalObject(JSContext* aCx, nsIPrincipal* aPrincipal,
                   JS::MutableHandle<JSObject*> aReflector);

  virtual JSObject*
  GetGlobalJSObject() override
  {
    return GetWrapper();
  }

private:
  ~WorkletGlobalScope()
  {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WorkletGlobalScope_h
