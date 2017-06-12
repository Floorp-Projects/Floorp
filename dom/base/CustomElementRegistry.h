/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsDocument;

namespace mozilla {
namespace dom {

struct CustomElementData;
struct ElementDefinitionOptions;
struct LifecycleCallbacks;
class CallbackFunction;
class CustomElementReaction;
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

  // https://dom.spec.whatwg.org/#concept-element-custom-element-state
  // CustomElementData is only created on the element which is a custom element
  // or an upgrade candidate, so the state of an element without
  // CustomElementData is "uncustomized".
  enum class State {
    eUndefined,
    eFailed,
    eCustom
  };

  explicit CustomElementData(nsIAtom* aType);
  CustomElementData(nsIAtom* aType, State aState);
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
  // Custom element state as described in the custom element spec.
  State mState;
  // custom element reaction queue as described in the custom element spec.
  // There is 1 reaction in reaction queue, when 1) it becomes disconnected,
  // 2) it’s adopted into a new document, 3) its attributes are changed,
  // appended, removed, or replaced.
  // There are 3 reactions in reaction queue when doing upgrade operation,
  // e.g., create an element, insert a node.
  AutoTArray<nsAutoPtr<CustomElementReaction>, 3> mReactionQueue;

  // Empties the callback queue.
  void RunCallbackQueue();

private:
  virtual ~CustomElementData() {}
};

// The required information for a custom element as defined in:
// https://html.spec.whatwg.org/multipage/scripting.html#custom-element-definition
struct CustomElementDefinition
{
  CustomElementDefinition(nsIAtom* aType,
                          nsIAtom* aLocalName,
                          JSObject* aConstructor,
                          JSObject* aPrototype,
                          mozilla::dom::LifecycleCallbacks* aCallbacks,
                          uint32_t aDocOrder);

  // The type (name) for this custom element.
  nsCOMPtr<nsIAtom> mType;

  // The localname to (e.g. <button is=type> -- this would be button).
  nsCOMPtr<nsIAtom> mLocalName;

  // The custom element constructor.
  JS::Heap<JSObject *> mConstructor;

  // The prototype to use for new custom elements of this type.
  JS::Heap<JSObject *> mPrototype;

  // The lifecycle callbacks to call for this custom element.
  nsAutoPtr<mozilla::dom::LifecycleCallbacks> mCallbacks;

  // A construction stack.
  // TODO: Bug 1287348 - Implement construction stack for upgrading an element

  // The document custom element order.
  uint32_t mDocOrder;

  bool IsCustomBuiltIn() {
    return mType != mLocalName;
  }
};

class CustomElementReaction
{
public:
  explicit CustomElementReaction(CustomElementRegistry* aRegistry,
                                 CustomElementDefinition* aDefinition)
    : mRegistry(aRegistry)
    , mDefinition(aDefinition)
  {
  };

  virtual ~CustomElementReaction() = default;
  virtual void Invoke(Element* aElement) = 0;

protected:
  CustomElementRegistry* mRegistry;
  CustomElementDefinition* mDefinition;
};

class CustomElementUpgradeReaction final : public CustomElementReaction
{
public:
  explicit CustomElementUpgradeReaction(CustomElementRegistry* aRegistry,
                                        CustomElementDefinition* aDefinition)
    : CustomElementReaction(aRegistry, aDefinition)
  {
  }

private:
   virtual void Invoke(Element* aElement) override;
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

  // nsWeakPtr is a weak pointer of Element
  // The element reaction queues are stored in CustomElementData.
  // We need to lookup ElementReactionQueueMap again to get relevant reaction queue.
  // The choice of 1 for the auto size here is based on gut feeling.
  typedef AutoTArray<nsWeakPtr, 1> ElementQueue;

  /**
   * Enqueue a custom element upgrade reaction
   * https://html.spec.whatwg.org/multipage/scripting.html#enqueue-a-custom-element-upgrade-reaction
   */
  void EnqueueUpgradeReaction(CustomElementRegistry* aRegistry,
                              Element* aElement,
                              CustomElementDefinition* aDefinition);

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
  AutoTArray<ElementQueue, 8> mReactionsStack;
  ElementQueue mBackupQueue;
  // https://html.spec.whatwg.org/#enqueue-an-element-on-the-appropriate-element-queue
  bool mIsBackupQueueProcessing;

  void InvokeBackupQueue();

  /**
   * Invoke custom element reactions
   * https://html.spec.whatwg.org/multipage/scripting.html#invoke-custom-element-reactions
   */
  void InvokeReactions(ElementQueue& aElementQueue);

  void Enqueue(Element* aElement, CustomElementReaction* aReaction);

private:
  class ProcessBackupQueueRunnable : public mozilla::Runnable {
    public:
      explicit ProcessBackupQueueRunnable(
        CustomElementReactionsStack* aReactionStack)
        : Runnable(
            "dom::CustomElementReactionsStack::ProcessBackupQueueRunnable")
        , mReactionStack(aReactionStack)
      {
        MOZ_ASSERT(!mReactionStack->mIsBackupQueueProcessing,
                   "mIsBackupQueueProcessing should be initially false");
        mReactionStack->mIsBackupQueueProcessing = true;
      }

      NS_IMETHOD Run() override
      {
        mReactionStack->InvokeBackupQueue();
        mReactionStack->mIsBackupQueueProcessing = false;
        return NS_OK;
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
                                     JSObject* aObject = nullptr);

  static void ProcessTopElementQueue();

  static void XPCOMShutdown();

  explicit CustomElementRegistry(nsPIDOMWindowInner* aWindow);

  /**
   * Looking up a custom element definition.
   * https://html.spec.whatwg.org/#look-up-a-custom-element-definition
   */
  CustomElementDefinition* LookupCustomElementDefinition(
    const nsAString& aLocalName, const nsAString* aIs = nullptr) const;

  CustomElementDefinition* LookupCustomElementDefinition(
    JSContext* aCx, JSObject *aConstructor) const;

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

  void Upgrade(Element* aElement, CustomElementDefinition* aDefinition);

private:
  ~CustomElementRegistry();

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

  void UpgradeCandidates(JSContext* aCx,
                         nsIAtom* aKey,
                         CustomElementDefinition* aDefinition,
                         ErrorResult& aRv);

  typedef nsClassHashtable<nsISupportsHashKey, CustomElementDefinition>
    DefinitionMap;
  typedef nsClassHashtable<nsISupportsHashKey, nsTArray<nsWeakPtr>>
    CandidateMap;
  typedef JS::GCHashMap<JS::Heap<JSObject*>,
                        nsCOMPtr<nsIAtom>,
                        js::MovableCellHasher<JS::Heap<JSObject*>>,
                        js::SystemAllocPolicy> ConstructorMap;

  // Hashtable for custom element definitions in web components.
  // Custom prototypes are stored in the compartment where
  // registerElement was called.
  DefinitionMap mCustomDefinitions;

  // Hashtable for looking up definitions by using constructor as key.
  // Custom elements' name are stored here and we need to lookup
  // mCustomDefinitions again to get definitions.
  ConstructorMap mConstructors;

  typedef nsRefPtrHashtable<nsISupportsHashKey, Promise>
    WhenDefinedPromiseMap;
  WhenDefinedPromiseMap mWhenDefinedPromiseMap;

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
    explicit AutoCEReaction(CustomElementReactionsStack* aReactionsStack)
      : mReactionsStack(aReactionsStack) {
      mReactionsStack->CreateAndPushElementQueue();
    }
    ~AutoCEReaction() {
      mReactionsStack->PopAndInvokeElementQueue();
    }
  private:
    RefPtr<CustomElementReactionsStack> mReactionsStack;
};

} // namespace dom
} // namespace mozilla


#endif // mozilla_dom_CustomElementRegistry_h
