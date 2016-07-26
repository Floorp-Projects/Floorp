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

class CustomElementHashKey : public PLDHashEntryHdr
{
public:
  typedef CustomElementHashKey *KeyType;
  typedef const CustomElementHashKey *KeyTypePointer;

  CustomElementHashKey(int32_t aNamespaceID, nsIAtom *aAtom)
    : mNamespaceID(aNamespaceID),
      mAtom(aAtom)
  {}
  explicit CustomElementHashKey(const CustomElementHashKey* aKey)
    : mNamespaceID(aKey->mNamespaceID),
      mAtom(aKey->mAtom)
  {}
  ~CustomElementHashKey()
  {}

  KeyType GetKey() const { return const_cast<KeyType>(this); }
  bool KeyEquals(const KeyTypePointer aKey) const
  {
    MOZ_ASSERT(mNamespaceID != kNameSpaceID_Unknown,
               "This equals method is not transitive, nor symmetric. "
               "A key with a namespace of kNamespaceID_Unknown should "
               "not be stored in a hashtable.");
    return (kNameSpaceID_Unknown == aKey->mNamespaceID ||
            mNamespaceID == aKey->mNamespaceID) &&
           aKey->mAtom == mAtom;
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(const KeyTypePointer aKey)
  {
    return aKey->mAtom->hash();
  }
  enum { ALLOW_MEMMOVE = true };

private:
  int32_t mNamespaceID;
  nsCOMPtr<nsIAtom> mAtom;
};

// The required information for a custom element as defined in:
// https://dvcs.w3.org/hg/webcomponents/raw-file/tip/spec/custom/index.html
struct CustomElementDefinition
{
  CustomElementDefinition(JSObject* aPrototype,
                          nsIAtom* aType,
                          nsIAtom* aLocalName,
                          mozilla::dom::LifecycleCallbacks* aCallbacks,
                          uint32_t aNamespaceID,
                          uint32_t aDocOrder);

  // The prototype to use for new custom elements of this type.
  JS::Heap<JSObject *> mPrototype;

  // The type (name) for this custom element.
  nsCOMPtr<nsIAtom> mType;

  // The localname to (e.g. <button is=type> -- this would be button).
  nsCOMPtr<nsIAtom> mLocalName;

  // The lifecycle callbacks to call for this custom element.
  nsAutoPtr<mozilla::dom::LifecycleCallbacks> mCallbacks;

  // Whether we're currently calling the created callback for a custom element
  // of this type.
  bool mElementIsBeingCreated;

  // Element namespace.
  int32_t mNamespaceID;

  // The document custom element order.
  uint32_t mDocOrder;
};

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
