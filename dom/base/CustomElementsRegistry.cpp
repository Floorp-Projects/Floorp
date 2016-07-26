/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CustomElementsRegistry.h"
#include "mozilla/dom/CustomElementsRegistryBinding.h"
#include "mozilla/dom/WebComponentsBinding.h"

namespace mozilla {
namespace dom {


// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CustomElementsRegistry, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(CustomElementsRegistry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CustomElementsRegistry)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CustomElementsRegistry)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ bool
CustomElementsRegistry::IsCustomElementsEnabled(JSContext* aCx, JSObject* aObject)
{
  JS::Rooted<JSObject*> obj(aCx, aObject);
  if (Preferences::GetBool("dom.webcomponents.customelements.enabled") ||
      nsDocument::IsWebComponentsEnabled(aCx, obj)) {
    return true;
  }

  return false;
}

/* static */ already_AddRefed<CustomElementsRegistry>
CustomElementsRegistry::Create(nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  if (!aWindow->GetDocShell()) {
    return nullptr;
  }

  RefPtr<CustomElementsRegistry> customElementsRegistry =
    new CustomElementsRegistry(aWindow);
  return customElementsRegistry.forget();
}

CustomElementsRegistry::CustomElementsRegistry(nsPIDOMWindowInner* aWindow)
 : mWindow(aWindow)
{
}

CustomElementsRegistry::~CustomElementsRegistry()
{
}

JSObject*
CustomElementsRegistry::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CustomElementsRegistryBinding::Wrap(aCx, this, aGivenProto);
}

nsISupports* CustomElementsRegistry::GetParentObject() const
{
  return mWindow;
}

void CustomElementsRegistry::Define(const nsAString& aName,
                                    Function& aFunctionConstructor,
                                    const ElementDefinitionOptions& aOptions,
                                    ErrorResult& aRv)
{
  // TODO: This function will be implemented in bug 1275835
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void
CustomElementsRegistry::Get(JSContext* aCx, const nsAString& aName,
                            JS::MutableHandle<JS::Value> aRetVal,
                            ErrorResult& aRv)
{
  // TODO: This function will be implemented in bug 1275838
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

already_AddRefed<Promise>
CustomElementsRegistry::WhenDefined(const nsAString& name, ErrorResult& aRv)
{
  // TODO: This function will be implemented in bug 1275839
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

CustomElementDefinition::CustomElementDefinition(JSObject* aPrototype,
                                                 nsIAtom* aType,
                                                 nsIAtom* aLocalName,
                                                 LifecycleCallbacks* aCallbacks,
                                                 uint32_t aNamespaceID,
                                                 uint32_t aDocOrder)
  : mPrototype(aPrototype),
    mType(aType),
    mLocalName(aLocalName),
    mCallbacks(aCallbacks),
    mNamespaceID(aNamespaceID),
    mDocOrder(aDocOrder)
{
}

} // namespace dom
} // namespace mozilla