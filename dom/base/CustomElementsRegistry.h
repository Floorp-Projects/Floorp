/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CustomElementsRegistry_h
#define mozilla_dom_CustomElementsRegistry_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/FunctionBinding.h"

namespace mozilla {
namespace dom {

struct ElementDefinitionOptions;
struct LifecycleCallbacks;
class Function;
class Promise;

class CustomElementsRegistry final : public nsISupports,
                                     public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CustomElementsRegistry)

public:
  static bool IsCustomElementsEnabled(JSContext* aCx, JSObject* aObject);
  static already_AddRefed<CustomElementsRegistry> Create(nsPIDOMWindowInner* aWindow);
  already_AddRefed<nsIDocument> GetOwnerDocument() const;

private:
  explicit CustomElementsRegistry(nsPIDOMWindowInner* aWindow);
  ~CustomElementsRegistry();
  nsCOMPtr<nsPIDOMWindowInner> mWindow;

public:
  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void Define(const nsAString& aName, Function& aFunctionConstructor,
              const ElementDefinitionOptions& aOptions, ErrorResult& aRv);

  void Get(JSContext* cx, const nsAString& name,
           JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv);

  already_AddRefed<Promise> WhenDefined(const nsAString& name, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla


#endif // mozilla_dom_CustomElementsRegistry_h
