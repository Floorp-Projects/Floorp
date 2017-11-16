/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CustomElementRegistry_h
#define mozilla_dom_CustomElementRegistry_h

#include "js/GCHashTable.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/WebComponentsBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGenericHTMLElement.h"
#include "nsWrapperCache.h"
#include "nsContentUtils.h"

class nsDocument;

namespace mozilla {
namespace dom {

struct CustomElementData;
struct ElementDefinitionOptions;
class CallbackFunction;
class CustomElementReaction;
class Function;
class Promise;

struct LifecycleCallbackArgs
{
  nsString name;
  nsString oldValue;
  nsString newValue;
  nsString namespaceURI;
};

struct LifecycleAdoptedCallbackArgs
{
  nsCOMPtr<nsIDocument> mOldDocument;
  nsCOMPtr<nsIDocument> mNewDocument;
};

class CustomElementCallback
{
public:
  CustomElementCallback(Element* aThisObject,
                        nsIDocument::ElementCallbackType aCallbackType,
                        CallbackFunction* aCallback);
  void Traverse(nsCycleCollectionTraversalCallback& aCb) const;
  void Call();
  void SetArgs(LifecycleCallbackArgs& aArgs)
  {
    MOZ_ASSERT(mType == nsIDocument::eAttributeChanged,
               "Arguments are only used by attribute changed callback.");
    mArgs = aArgs;
  }

  void SetAdoptedCallbackArgs(LifecycleAdoptedCallbackArgs& aAdoptedCallbackArgs)
  {
    MOZ_ASSERT(mType == nsIDocument::eAdopted,
      "Arguments are only used by adopted callback.");
    mAdoptedCallbackArgs = aAdoptedCallbackArgs;
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
  LifecycleAdoptedCallbackArgs mAdoptedCallbackArgs;
};

class CustomElementConstructor final : public CallbackFunction
{
public:
  explicit CustomElementConstructor(CallbackFunction* aOther)
    : CallbackFunction(aOther)
  {
    MOZ_ASSERT(JS::IsConstructor(mCallback));
  }

  already_AddRefed<Element> Construct(const char* aExecutionReason, ErrorResult& aRv);
};

// Each custom element has an associated callback queue and an element is
// being created flag.
struct CustomElementData
{
  NS_INLINE_DECL_REFCOUNTING(CustomElementData)

  // https://dom.spec.whatwg.org/#concept-element-custom-element-state
  // CustomElementData is only created on the element which is a custom element
  // or an upgrade candidate, so the state of an element without
  // CustomElementData is "uncustomized".
  enum class State {
    eUndefined,
    eFailed,
    eCustom
  };

  explicit CustomElementData(nsAtom* aType);
  CustomElementData(nsAtom* aType, State aState);

  // Custom element state as described in the custom element spec.
  State mState;
  // custom element reaction queue as described in the custom element spec.
  // There is 1 reaction in reaction queue, when 1) it becomes disconnected,
  // 2) it’s adopted into a new document, 3) its attributes are changed,
  // appended, removed, or replaced.
  // There are 3 reactions in reaction queue when doing upgrade operation,
  // e.g., create an element, insert a node.
  AutoTArray<UniquePtr<CustomElementReaction>, 3> mReactionQueue;

  void SetCustomElementDefinition(CustomElementDefinition* aDefinition);
  CustomElementDefinition* GetCustomElementDefinition();
  nsAtom* GetCustomElementType();

  void Traverse(nsCycleCollectionTraversalCallback& aCb) const;
  void Unlink();

private:
  virtual ~CustomElementData() {}

  // Custom element type, for <button is="x-button"> or <x-button>
  // this would be x-button.
  RefPtr<nsAtom> mType;
  RefPtr<CustomElementDefinition> mCustomElementDefinition;
};

#define ALEADY_CONSTRUCTED_MARKER nullptr

// The required information for a custom element as defined in:
// https://html.spec.whatwg.org/multipage/scripting.html#custom-element-definition
struct CustomElementDefinition
{
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(CustomElementDefinition)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CustomElementDefinition)

  CustomElementDefinition(nsAtom* aType,
                          nsAtom* aLocalName,
                          Function* aConstructor,
                          nsTArray<RefPtr<nsAtom>>&& aObservedAttributes,
                          JS::Handle<JSObject*> aPrototype,
                          mozilla::dom::LifecycleCallbacks* aCallbacks,
                          uint32_t aDocOrder);

  // The type (name) for this custom element, for <button is="x-foo"> or <x-foo>
  // this would be x-foo.
  RefPtr<nsAtom> mType;

  // The localname to (e.g. <button is=type> -- this would be button).
  RefPtr<nsAtom> mLocalName;

  // The custom element constructor.
  RefPtr<CustomElementConstructor> mConstructor;

  // The list of attributes that this custom element observes.
  nsTArray<RefPtr<nsAtom>> mObservedAttributes;

  // The prototype to use for new custom elements of this type.
  JS::Heap<JSObject *> mPrototype;

  // The lifecycle callbacks to call for this custom element.
  UniquePtr<mozilla::dom::LifecycleCallbacks> mCallbacks;

  // A construction stack. Use nullptr to represent an "already constructed marker".
  nsTArray<RefPtr<Element>> mConstructionStack;

  // The document custom element order.
  uint32_t mDocOrder;

  bool IsCustomBuiltIn()
  {
    return mType != mLocalName;
  }

  bool IsInObservedAttributeList(nsAtom* aName)
  {
    if (mObservedAttributes.IsEmpty()) {
      return false;
    }

    return mObservedAttributes.Contains(aName);
  }

private:
  ~CustomElementDefinition() {}
};

class CustomElementReaction
{
public:
  virtual ~CustomElementReaction() = default;
  virtual void Invoke(Element* aElement, ErrorResult& aRv) = 0;
  virtual void Traverse(nsCycleCollectionTraversalCallback& aCb) const
  {
  }

#if DEBUG
  bool IsUpgradeReaction()
  {
    return mIsUpgradeReaction;
  }

protected:
  bool mIsUpgradeReaction = false;
#endif
};

class CustomElementUpgradeReaction final : public CustomElementReaction
{
public:
  explicit CustomElementUpgradeReaction(CustomElementDefinition* aDefinition)
    : mDefinition(aDefinition)
  {
#if DEBUG
    mIsUpgradeReaction = true;
#endif
  }

private:
   virtual void Invoke(Element* aElement, ErrorResult& aRv) override;

   CustomElementDefinition* mDefinition;
};

class CustomElementCallbackReaction final : public CustomElementReaction
{
  public:
    explicit CustomElementCallbackReaction(UniquePtr<CustomElementCallback> aCustomElementCallback)
      : mCustomElementCallback(Move(aCustomElementCallback))
    {
    }

    virtual void Traverse(nsCycleCollectionTraversalCallback& aCb) const override
    {
      mCustomElementCallback->Traverse(aCb);
    }

  private:
    virtual void Invoke(Element* aElement, ErrorResult& aRv) override;
    UniquePtr<CustomElementCallback> mCustomElementCallback;
};

// https://html.spec.whatwg.org/multipage/scripting.html#custom-element-reactions-stack
class CustomElementReactionsStack
{
public:
  NS_INLINE_DECL_REFCOUNTING(CustomElementReactionsStack)

  CustomElementReactionsStack()
    : mIsBackupQueueProcessing(false)
  {
  }

  // Hold a strong reference of Element so that it does not get cycle collected
  // before the reactions in its reaction queue are invoked.
  // The element reaction queues are stored in CustomElementData.
  // We need to lookup ElementReactionQueueMap again to get relevant reaction queue.
  // The choice of 1 for the auto size here is based on gut feeling.
  typedef AutoTArray<RefPtr<Element>, 1> ElementQueue;

  /**
   * Enqueue a custom element upgrade reaction
   * https://html.spec.whatwg.org/multipage/scripting.html#enqueue-a-custom-element-upgrade-reaction
   */
  void EnqueueUpgradeReaction(Element* aElement,
                              CustomElementDefinition* aDefinition);

  /**
   * Enqueue a custom element callback reaction
   * https://html.spec.whatwg.org/multipage/scripting.html#enqueue-a-custom-element-callback-reaction
   */
  void EnqueueCallbackReaction(Element* aElement,
                               UniquePtr<CustomElementCallback> aCustomElementCallback);

  // [CEReactions] Before executing the algorithm's steps
  // Push a new element queue onto the custom element reactions stack.
  void CreateAndPushElementQueue();

  // [CEReactions] After executing the algorithm's steps
  // Pop the element queue from the custom element reactions stack,
  // and invoke custom element reactions in that queue.
  void PopAndInvokeElementQueue();

private:
  ~CustomElementReactionsStack() {};

  // The choice of 8 for the auto size here is based on gut feeling.
  AutoTArray<UniquePtr<ElementQueue>, 8> mReactionsStack;
  ElementQueue mBackupQueue;
  // https://html.spec.whatwg.org/#enqueue-an-element-on-the-appropriate-element-queue
  bool mIsBackupQueueProcessing;

  void InvokeBackupQueue();

  /**
   * Invoke custom element reactions
   * https://html.spec.whatwg.org/multipage/scripting.html#invoke-custom-element-reactions
   */
  void InvokeReactions(ElementQueue* aElementQueue, nsIGlobalObject* aGlobal);

  void Enqueue(Element* aElement, CustomElementReaction* aReaction);

private:
  class BackupQueueMicroTask final : public mozilla::MicroTaskRunnable {
    public:
      explicit BackupQueueMicroTask(
        CustomElementReactionsStack* aReactionStack)
        : MicroTaskRunnable()
        , mReactionStack(aReactionStack)
      {
        MOZ_ASSERT(!mReactionStack->mIsBackupQueueProcessing,
                   "mIsBackupQueueProcessing should be initially false");
        mReactionStack->mIsBackupQueueProcessing = true;
      }

      virtual void Run(AutoSlowOperation& aAso) override
      {
        mReactionStack->InvokeBackupQueue();
        mReactionStack->mIsBackupQueueProcessing = false;
      }

    private:
      RefPtr<CustomElementReactionsStack> mReactionStack;
  };
};

class CustomElementRegistry final : public nsISupports,
                                    public nsWrapperCache
{
  // Allow nsDocument to access mCustomDefinitions and mCandidatesMap.
  friend class ::nsDocument;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CustomElementRegistry)

public:
  static bool IsCustomElementEnabled(JSContext* aCx = nullptr,
                                     JSObject* aObject = nullptr)
  {
    return nsContentUtils::IsCustomElementsEnabled();
  }

  explicit CustomElementRegistry(nsPIDOMWindowInner* aWindow);

  /**
   * Looking up a custom element definition.
   * https://html.spec.whatwg.org/#look-up-a-custom-element-definition
   */
  CustomElementDefinition* LookupCustomElementDefinition(
    const nsAString& aLocalName, nsAtom* aTypeAtom) const;

  CustomElementDefinition* LookupCustomElementDefinition(
    JSContext* aCx, JSObject *aConstructor) const;

  static void EnqueueLifecycleCallback(nsIDocument::ElementCallbackType aType,
                                       Element* aCustomElement,
                                       LifecycleCallbackArgs* aArgs,
                                       LifecycleAdoptedCallbackArgs* aAdoptedCallbackArgs,
                                       CustomElementDefinition* aDefinition);

  /**
   * Upgrade an element.
   * https://html.spec.whatwg.org/multipage/scripting.html#upgrades
   */
  static void Upgrade(Element* aElement, CustomElementDefinition* aDefinition, ErrorResult& aRv);

  /**
   * Registers an unresolved custom element that is a candidate for
   * upgrade. |aTypeName| is the name of the custom element type, if it is not
   * provided, then element name is used. |aTypeName| should be provided
   * when registering a custom element that extends an existing
   * element. e.g. <button is="x-button">.
   */
  void RegisterUnresolvedElement(Element* aElement,
                                 nsAtom* aTypeName = nullptr);

  /**
   * Unregister an unresolved custom element that is a candidate for
   * upgrade when a custom element is removed from tree.
   */
  void UnregisterUnresolvedElement(Element* aElement,
                                   nsAtom* aTypeName = nullptr);
private:
  ~CustomElementRegistry();

  static UniquePtr<CustomElementCallback> CreateCustomElementCallback(
    nsIDocument::ElementCallbackType aType, Element* aCustomElement,
    LifecycleCallbackArgs* aArgs,
    LifecycleAdoptedCallbackArgs* aAdoptedCallbackArgs,
    CustomElementDefinition* aDefinition);

  void UpgradeCandidates(nsAtom* aKey,
                         CustomElementDefinition* aDefinition,
                         ErrorResult& aRv);

  typedef nsRefPtrHashtable<nsRefPtrHashKey<nsAtom>, CustomElementDefinition>
    DefinitionMap;
  typedef nsClassHashtable<nsRefPtrHashKey<nsAtom>, nsTArray<nsWeakPtr>>
    CandidateMap;
  typedef JS::GCHashMap<JS::Heap<JSObject*>,
                        RefPtr<nsAtom>,
                        js::MovableCellHasher<JS::Heap<JSObject*>>,
                        js::SystemAllocPolicy> ConstructorMap;

  // Hashtable for custom element definitions in web components.
  // Custom prototypes are stored in the compartment where definition was
  // defined.
  DefinitionMap mCustomDefinitions;

  // Hashtable for looking up definitions by using constructor as key.
  // Custom elements' name are stored here and we need to lookup
  // mCustomDefinitions again to get definitions.
  ConstructorMap mConstructors;

  typedef nsRefPtrHashtable<nsRefPtrHashKey<nsAtom>, Promise>
    WhenDefinedPromiseMap;
  WhenDefinedPromiseMap mWhenDefinedPromiseMap;

  // The "upgrade candidates map" from the web components spec. Maps from a
  // namespace id and local name to a list of elements to upgrade if that
  // element is registered as a custom element.
  CandidateMap mCandidatesMap;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  // It is used to prevent reentrant invocations of element definition.
  bool mIsCustomDefinitionRunning;

private:
  class MOZ_RAII AutoSetRunningFlag final {
    public:
      explicit AutoSetRunningFlag(CustomElementRegistry* aRegistry)
        : mRegistry(aRegistry)
      {
        MOZ_ASSERT(!mRegistry->mIsCustomDefinitionRunning,
                   "IsCustomDefinitionRunning flag should be initially false");
        mRegistry->mIsCustomDefinitionRunning = true;
      }

      ~AutoSetRunningFlag() {
        mRegistry->mIsCustomDefinitionRunning = false;
      }

    private:
      CustomElementRegistry* mRegistry;
  };

public:
  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void Define(const nsAString& aName, Function& aFunctionConstructor,
              const ElementDefinitionOptions& aOptions, ErrorResult& aRv);

  void Get(JSContext* cx, const nsAString& name,
           JS::MutableHandle<JS::Value> aRetVal);

  already_AddRefed<Promise> WhenDefined(const nsAString& aName, ErrorResult& aRv);
};

class MOZ_RAII AutoCEReaction final {
  public:
    // JSContext is allowed to be a nullptr if we are guaranteeing that we're
    // not doing something that might throw but not finish reporting a JS
    // exception during the lifetime of the AutoCEReaction.
    AutoCEReaction(CustomElementReactionsStack* aReactionsStack, JSContext* aCx)
      : mReactionsStack(aReactionsStack)
      , mCx(aCx) {
      mReactionsStack->CreateAndPushElementQueue();
    }
    ~AutoCEReaction() {
      Maybe<JS::AutoSaveExceptionState> ases;
      if (mCx) {
        ases.emplace(mCx);
      }
      mReactionsStack->PopAndInvokeElementQueue();
    }
  private:
    RefPtr<CustomElementReactionsStack> mReactionsStack;
    JSContext* mCx;
};

} // namespace dom
} // namespace mozilla


#endif // mozilla_dom_CustomElementRegistry_h
