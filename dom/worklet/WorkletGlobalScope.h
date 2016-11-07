/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WorkletGlobalScope_h
#define mozilla_dom_WorkletGlobalScope_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsIGlobalObject.h"
#include "nsWrapperCache.h"

#define WORKLET_IID \
  { 0x1b3f62e7, 0xe357, 0x44be, \
    { 0xbf, 0xe0, 0xdf, 0x85, 0xe6, 0x56, 0x85, 0xac } }

class nsIPrincipal;
class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class Console;

class WorkletGlobalScope final : public nsIGlobalObject
                               , public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(WORKLET_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WorkletGlobalScope)

  explicit WorkletGlobalScope(nsPIDOMWindowInner* aWindow);

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

  Console*
  GetConsole(ErrorResult& aRv);

  void
  Dump(const Optional<nsAString>& aString) const;

private:
  ~WorkletGlobalScope();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<Console> mConsole;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WorkletGlobalScope, WORKLET_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WorkletGlobalScope_h
