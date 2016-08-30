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

class nsDocument;

namespace mozilla {
namespace dom {

struct CustomElementData;
struct ElementDefinitionOptions;
struct LifecycleCallbacks;
class CallbackFunction;
class Function;
class Promise;

struct LifecycleCallbackArgs
{
  nsString name;
  nsString oldValue;
  nsString newValue;
};

class CustomElementCallback
{
public:
  CustomElementCallback(Element* aThisObject,
                        nsIDocument::ElementCallbackType aCallbackType,
                        CallbackFunction* aCallback,
                        CustomElementData* aOwnerData);
  void Traverse(nsCycleCollectionTraversalCallback& aCb) const;
  void Call();
  void SetArgs(LifecycleCallbackArgs& aArgs)
  {
    MOZ_ASSERT(mType == nsIDocument::eAttributeChanged,
               "Arguments are only used by attribute changed callback.");
    mArgs = aArgs;
  }

private:
  // The this value to use for invocation of the callback.
  RefPtr<Element> mThisObject;
  RefPtr<CallbackFunction> mCallback;
  // The type of callback (eCreated, eAttached, etc.)
  nsIDocument::ElementCallbackType mType;
  // Arguments to be passed to the callback,
  // used by the attribute changed callback.
  LifecycleCallbackArgs mArgs;
  // CustomElementData that contains this callback in the
  // callback queue.
  CustomElementData* mOwnerData;
};

// Each custom element has an associated callback queue and an element is
// being created flag.
struct CustomElementData
{
  NS_INLINE_DECL_REFCOUNTING(CustomElementData)

  explicit CustomElementData(nsIAtom* aType);
  // Objects in this array are transient and empty after each microtask
  // checkpoint.
  nsTArray<nsAutoPtr<CustomElementCallback>> mCallbackQueue;
  // Custom element type, for <button is="x-button"> or <x-button>
  // this would be x-button.
  nsCOMPtr<nsIAtom> mType;
  // The callback that is next to be processed upon calling RunCallbackQueue.
  int32_t mCurrentCallback;
  // Element is being created flag as described in the custom elements spec.
  bool mElementIsBeingCreated;
  // Flag to determine if the created callback has been invoked, thus it
  // determines if other callbacks can be enqueued.
  bool mCreatedCallbackInvoked;
  // The microtask level associated with the callbacks in the callback queue,
  // it is used to determine if a new queue needs to be pushed onto the
  // processing stack.
  int32_t mAssociatedMicroTask;

  // Empties the callback queue.
  void RunCallbackQueue();

private:
  virtual ~CustomElementData() {}
};

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
  // Allow nsDocument to access mCustomDefinitions and mCandidatesMap.
  friend class ::nsDocument;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CustomElementsRegistry)

public:
  static bool IsCustomElementsEnabled(JSContext* aCx, JSObject* aObject);
  static already_AddRefed<CustomElementsRegistry> Create(nsPIDOMWindowInner* aWindow);
  static void ProcessTopElementQueue();

  static void XPCOMShutdown();

  /**
   * Looking up a custom element definition.
   * https://html.spec.whatwg.org/#look-up-a-custom-element-definition
   */
  CustomElementDefinition* LookupCustomElementDefinition(
    const nsAString& aLocalName, const nsAString* aIs = nullptr) const;

  /**
   * Enqueue created callback or register upgrade candidate for
   * newly created custom elements, possibly extending an existing type.
   * ex. <x-button>, <button is="x-button> (type extension)
   */
  void SetupCustomElement(Element* aElement, const nsAString* aTypeExtension);

  void EnqueueLifecycleCallback(nsIDocument::ElementCallbackType aType,
                                Element* aCustomElement,
                                LifecycleCallbackArgs* aArgs,
                                CustomElementDefinition* aDefinition);

  void GetCustomPrototype(nsIAtom* aAtom,
                          JS::MutableHandle<JSObject*> aPrototype);

private:
  explicit CustomElementsRegistry(nsPIDOMWindowInner* aWindow);
  ~CustomElementsRegistry();

  /**
   * Registers an unresolved custom element that is a candidate for
   * upgrade when the definition is registered via registerElement.
   * |aTypeName| is the name of the custom element type, if it is not
   * provided, then element name is used. |aTypeName| should be provided
   * when registering a custom element that extends an existing
   * element. e.g. <button is="x-button">.
   */
  void RegisterUnresolvedElement(Element* aElement,
                                 nsIAtom* aTypeName = nullptr);

  typedef nsClassHashtable<CustomElementHashKey, CustomElementDefinition>
    DefinitionMap;
  typedef nsClassHashtable<CustomElementHashKey, nsTArray<nsWeakPtr>>
    CandidateMap;

  // Hashtable for custom element definitions in web components.
  // Custom prototypes are stored in the compartment where
  // registerElement was called.
  DefinitionMap mCustomDefinitions;

  // The "upgrade candidates map" from the web components spec. Maps from a
  // namespace id and local name to a list of elements to upgrade if that
  // element is registered as a custom element.
  CandidateMap mCandidatesMap;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  // Array representing the processing stack in the custom elements
  // specification. The processing stack is conceptually a stack of
  // element queues. Each queue is represented by a sequence of
  // CustomElementData in this array, separated by nullptr that
  // represent the boundaries of the items in the stack. The first
  // queue in the stack is the base element queue.
  static mozilla::Maybe<nsTArray<RefPtr<CustomElementData>>> sProcessingStack;

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
