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
#include "mozilla/CycleCollectedJSContext.h"  // for MicroTaskRunnable
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/CustomElementRegistryBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/WebComponentsBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGenericHTMLElement.h"
#include "nsWrapperCache.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

struct CustomElementData;
struct ElementDefinitionOptions;
class CallbackFunction;
class CustomElementReaction;
class DocGroup;
class Promise;

struct LifecycleCallbackArgs {
  nsString name;
  nsString oldValue;
  nsString newValue;
  nsString namespaceURI;

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;
};

struct LifecycleAdoptedCallbackArgs {
  RefPtr<Document> mOldDocument;
  RefPtr<Document> mNewDocument;
};

class CustomElementCallback {
 public:
  CustomElementCallback(Element* aThisObject,
                        Document::ElementCallbackType aCallbackType,
                        CallbackFunction* aCallback);
  void Traverse(nsCycleCollectionTraversalCallback& aCb) const;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;
  void Call();
  void SetArgs(LifecycleCallbackArgs& aArgs) {
    MOZ_ASSERT(mType == Document::eAttributeChanged,
               "Arguments are only used by attribute changed callback.");
    mArgs = aArgs;
  }

  void SetAdoptedCallbackArgs(
      LifecycleAdoptedCallbackArgs& aAdoptedCallbackArgs) {
    MOZ_ASSERT(mType == Document::eAdopted,
               "Arguments are only used by adopted callback.");
    mAdoptedCallbackArgs = aAdoptedCallbackArgs;
  }

 private:
  // The this value to use for invocation of the callback.
  RefPtr<Element> mThisObject;
  RefPtr<CallbackFunction> mCallback;
  // The type of callback (eCreated, eAttached, etc.)
  Document::ElementCallbackType mType;
  // Arguments to be passed to the callback,
  // used by the attribute changed callback.
  LifecycleCallbackArgs mArgs;
  LifecycleAdoptedCallbackArgs mAdoptedCallbackArgs;
};

// Each custom element has an associated callback queue and an element is
// being created flag.
struct CustomElementData {
  NS_INLINE_DECL_REFCOUNTING(CustomElementData)

  // https://dom.spec.whatwg.org/#concept-element-custom-element-state
  // CustomElementData is only created on the element which is a custom element
  // or an upgrade candidate, so the state of an element without
  // CustomElementData is "uncustomized".
  enum class State { eUndefined, eFailed, eCustom };

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
  nsAtom* GetCustomElementType() const { return mType; }

  void Traverse(nsCycleCollectionTraversalCallback& aCb) const;
  void Unlink();
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  nsAtom* GetIs(const Element* aElement) const {
    // If mType isn't the same as name atom, this is a customized built-in
    // element, which has 'is' value set.
    return aElement->NodeInfo()->NameAtom() == mType ? nullptr : mType.get();
  }

 private:
  virtual ~CustomElementData() {}

  // Custom element type, for <button is="x-button"> or <x-button>
  // this would be x-button.
  RefPtr<nsAtom> mType;
  RefPtr<CustomElementDefinition> mCustomElementDefinition;
};

#define ALREADY_CONSTRUCTED_MARKER nullptr

// The required information for a custom element as defined in:
// https://html.spec.whatwg.org/multipage/scripting.html#custom-element-definition
struct CustomElementDefinition {
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(CustomElementDefinition)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CustomElementDefinition)

  CustomElementDefinition(nsAtom* aType, nsAtom* aLocalName,
                          int32_t aNamespaceID,
                          CustomElementConstructor* aConstructor,
                          nsTArray<RefPtr<nsAtom>>&& aObservedAttributes,
                          UniquePtr<LifecycleCallbacks>&& aCallbacks);

  // The type (name) for this custom element, for <button is="x-foo"> or <x-foo>
  // this would be x-foo.
  RefPtr<nsAtom> mType;

  // The localname to (e.g. <button is=type> -- this would be button).
  RefPtr<nsAtom> mLocalName;

  // The namespace for this custom element
  int32_t mNamespaceID;

  // The custom element constructor.
  RefPtr<CustomElementConstructor> mConstructor;

  // The list of attributes that this custom element observes.
  nsTArray<RefPtr<nsAtom>> mObservedAttributes;

  // The lifecycle callbacks to call for this custom element.
  UniquePtr<LifecycleCallbacks> mCallbacks;

  // A construction stack. Use nullptr to represent an "already constructed
  // marker".
  nsTArray<RefPtr<Element>> mConstructionStack;

  // See step 6.1.10 of https://dom.spec.whatwg.org/#concept-create-element
  // which set up the prefix after a custom element is created. However, In
  // Gecko, the prefix isn't allowed to be changed in NodeInfo, so we store the
  // prefix information here and propagate to where NodeInfo is assigned to a
  // custom element instead.
  nsTArray<RefPtr<nsAtom>> mPrefixStack;

  // This basically is used for distinguishing the custom element constructor
  // is invoked from document.createElement or directly from JS, i.e.
  // `new CustomElementConstructor()`.
  uint32_t mConstructionDepth = 0;

  bool IsCustomBuiltIn() { return mType != mLocalName; }

  bool IsInObservedAttributeList(nsAtom* aName) {
    if (mObservedAttributes.IsEmpty()) {
      return false;
    }

    return mObservedAttributes.Contains(aName);
  }

 private:
  ~CustomElementDefinition() {}
};

class CustomElementReaction {
 public:
  virtual ~CustomElementReaction() = default;
  MOZ_CAN_RUN_SCRIPT
  virtual void Invoke(Element* aElement, ErrorResult& aRv) = 0;
  virtual void Traverse(nsCycleCollectionTraversalCallback& aCb) const = 0;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const = 0;

  bool IsUpgradeReaction() { return mIsUpgradeReaction; }

 protected:
  bool mIsUpgradeReaction = false;
};

// https://html.spec.whatwg.org/multipage/scripting.html#custom-element-reactions-stack
class CustomElementReactionsStack {
 public:
  NS_INLINE_DECL_REFCOUNTING(CustomElementReactionsStack)

  CustomElementReactionsStack()
      : mIsBackupQueueProcessing(false),
        mRecursionDepth(0),
        mIsElementQueuePushedForCurrentRecursionDepth(false) {}

  // Hold a strong reference of Element so that it does not get cycle collected
  // before the reactions in its reaction queue are invoked.
  // The element reaction queues are stored in CustomElementData.
  // We need to lookup ElementReactionQueueMap again to get relevant reaction
  // queue. The choice of 3 for the auto size here is based on running Custom
  // Elements wpt tests.
  typedef AutoTArray<RefPtr<Element>, 3> ElementQueue;

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
  void EnqueueCallbackReaction(
      Element* aElement,
      UniquePtr<CustomElementCallback> aCustomElementCallback);

  /**
   * [CEReactions] Before executing the algorithm's steps.
   * Increase the current recursion depth, and the element queue is pushed
   * lazily when we really enqueue reactions.
   *
   * @return true if the element queue is pushed for "previous" recursion depth.
   */
  bool EnterCEReactions() {
    bool temp = mIsElementQueuePushedForCurrentRecursionDepth;
    mRecursionDepth++;
    // The is-element-queue-pushed flag is initially false when entering a new
    // recursion level. The original value will be cached in AutoCEReaction
    // and restored after leaving this recursion level.
    mIsElementQueuePushedForCurrentRecursionDepth = false;
    return temp;
  }

  /**
   * [CEReactions] After executing the algorithm's steps.
   * Pop and invoke the element queue if it is created and pushed for current
   * recursion depth, then decrease the current recursion depth.
   *
   * @param aCx JSContext used for handling exception thrown by algorithm's
   *            steps, this could be a nullptr.
   *        aWasElementQueuePushed used for restoring status after leaving
   *                               current recursion.
   */
  MOZ_CAN_RUN_SCRIPT
  void LeaveCEReactions(JSContext* aCx, bool aWasElementQueuePushed) {
    MOZ_ASSERT(mRecursionDepth);

    if (mIsElementQueuePushedForCurrentRecursionDepth) {
      Maybe<JS::AutoSaveExceptionState> ases;
      if (aCx) {
        ases.emplace(aCx);
      }
      PopAndInvokeElementQueue();
    }
    mRecursionDepth--;
    // Restore the is-element-queue-pushed flag cached in AutoCEReaction when
    // leaving the recursion level.
    mIsElementQueuePushedForCurrentRecursionDepth = aWasElementQueuePushed;

    MOZ_ASSERT_IF(!mRecursionDepth, mReactionsStack.IsEmpty());
  }

 private:
  ~CustomElementReactionsStack(){};

  /**
   * Push a new element queue onto the custom element reactions stack.
   */
  void CreateAndPushElementQueue();

  /**
   * Pop the element queue from the custom element reactions stack, and invoke
   * custom element reactions in that queue.
   */
  MOZ_CAN_RUN_SCRIPT void PopAndInvokeElementQueue();

  // The choice of 8 for the auto size here is based on gut feeling.
  AutoTArray<UniquePtr<ElementQueue>, 8> mReactionsStack;
  ElementQueue mBackupQueue;
  // https://html.spec.whatwg.org/#enqueue-an-element-on-the-appropriate-element-queue
  bool mIsBackupQueueProcessing;

  MOZ_CAN_RUN_SCRIPT void InvokeBackupQueue();

  /**
   * Invoke custom element reactions
   * https://html.spec.whatwg.org/multipage/scripting.html#invoke-custom-element-reactions
   */
  MOZ_CAN_RUN_SCRIPT
  void InvokeReactions(ElementQueue* aElementQueue, nsIGlobalObject* aGlobal);

  void Enqueue(Element* aElement, CustomElementReaction* aReaction);

  // Current [CEReactions] recursion depth.
  uint32_t mRecursionDepth;
  // True if the element queue is pushed into reaction stack for current
  // recursion depth. This will be cached in AutoCEReaction when entering a new
  // CEReaction recursion and restored after leaving the recursion.
  bool mIsElementQueuePushedForCurrentRecursionDepth;

 private:
  class BackupQueueMicroTask final : public mozilla::MicroTaskRunnable {
   public:
    explicit BackupQueueMicroTask(CustomElementReactionsStack* aReactionStack)
        : MicroTaskRunnable(), mReactionStack(aReactionStack) {
      MOZ_ASSERT(!mReactionStack->mIsBackupQueueProcessing,
                 "mIsBackupQueueProcessing should be initially false");
      mReactionStack->mIsBackupQueueProcessing = true;
    }

    MOZ_CAN_RUN_SCRIPT virtual void Run(AutoSlowOperation& aAso) override {
      mReactionStack->InvokeBackupQueue();
      mReactionStack->mIsBackupQueueProcessing = false;
    }

   private:
    const RefPtr<CustomElementReactionsStack> mReactionStack;
  };
};

class CustomElementRegistry final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CustomElementRegistry)

 public:
  explicit CustomElementRegistry(nsPIDOMWindowInner* aWindow);

 private:
  class RunCustomElementCreationCallback : public mozilla::Runnable {
   public:
    // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.
    // See bug 1535398.
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    NS_DECL_NSIRUNNABLE

    explicit RunCustomElementCreationCallback(
        CustomElementRegistry* aRegistry, nsAtom* aAtom,
        CustomElementCreationCallback* aCallback)
        : mozilla::Runnable(
              "CustomElementRegistry::RunCustomElementCreationCallback"),
          mRegistry(aRegistry),
          mAtom(aAtom),
          mCallback(aCallback) {}

   private:
    RefPtr<CustomElementRegistry> mRegistry;
    RefPtr<nsAtom> mAtom;
    RefPtr<CustomElementCreationCallback> mCallback;
  };

 public:
  /**
   * Returns whether there's a definition that is likely to match this type
   * atom. This is not exact, so should only be used for optimization, but it's
   * good enough to prove that the chrome code doesn't need an XBL binding.
   */
  bool IsLikelyToBeCustomElement(nsAtom* aTypeAtom) const {
    return mCustomDefinitions.GetWeak(aTypeAtom) ||
           mElementCreationCallbacks.GetWeak(aTypeAtom);
  }

  /**
   * Looking up a custom element definition.
   * https://html.spec.whatwg.org/#look-up-a-custom-element-definition
   */
  CustomElementDefinition* LookupCustomElementDefinition(nsAtom* aNameAtom,
                                                         int32_t aNameSpaceID,
                                                         nsAtom* aTypeAtom);

  CustomElementDefinition* LookupCustomElementDefinition(
      JSContext* aCx, JSObject* aConstructor) const;

  static void EnqueueLifecycleCallback(
      Document::ElementCallbackType aType, Element* aCustomElement,
      LifecycleCallbackArgs* aArgs,
      LifecycleAdoptedCallbackArgs* aAdoptedCallbackArgs,
      CustomElementDefinition* aDefinition);

  /**
   * Upgrade an element.
   * https://html.spec.whatwg.org/multipage/scripting.html#upgrades
   */
  MOZ_CAN_RUN_SCRIPT
  static void Upgrade(Element* aElement, CustomElementDefinition* aDefinition,
                      ErrorResult& aRv);

  /**
   * To allow native code to call methods of chrome-implemented custom elements,
   * a helper method may be defined in the custom element called
   * 'getCustomInterfaceCallback'. This method takes an IID and returns an
   * object which implements an XPCOM interface.
   *
   * This returns null if aElement is not from a chrome document.
   */
  static already_AddRefed<nsISupports> CallGetCustomInterface(
      Element* aElement, const nsIID& aIID);

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

  /**
   * Register an element to be upgraded when the custom element creation
   * callback is executed.
   *
   * To be used when LookupCustomElementDefinition() didn't return a definition,
   * but with the callback scheduled to be run.
   */
  inline void RegisterCallbackUpgradeElement(Element* aElement,
                                             nsAtom* aTypeName = nullptr) {
    if (mElementCreationCallbacksUpgradeCandidatesMap.IsEmpty()) {
      return;
    }

    RefPtr<nsAtom> typeName = aTypeName;
    if (!typeName) {
      typeName = aElement->NodeInfo()->NameAtom();
    }

    nsTHashtable<nsRefPtrHashKey<nsIWeakReference>>* elements =
        mElementCreationCallbacksUpgradeCandidatesMap.Get(typeName);

    // If there isn't a table, there won't be a definition added by the
    // callback.
    if (!elements) {
      return;
    }

    nsWeakPtr elem = do_GetWeakReference(aElement);
    elements->PutEntry(elem);
  }

 private:
  ~CustomElementRegistry();

  static UniquePtr<CustomElementCallback> CreateCustomElementCallback(
      Document::ElementCallbackType aType, Element* aCustomElement,
      LifecycleCallbackArgs* aArgs,
      LifecycleAdoptedCallbackArgs* aAdoptedCallbackArgs,
      CustomElementDefinition* aDefinition);

  void UpgradeCandidates(nsAtom* aKey, CustomElementDefinition* aDefinition,
                         ErrorResult& aRv);

  typedef nsRefPtrHashtable<nsRefPtrHashKey<nsAtom>, CustomElementDefinition>
      DefinitionMap;
  typedef nsRefPtrHashtable<nsRefPtrHashKey<nsAtom>,
                            CustomElementCreationCallback>
      ElementCreationCallbackMap;
  typedef nsClassHashtable<nsRefPtrHashKey<nsAtom>,
                           nsTHashtable<nsRefPtrHashKey<nsIWeakReference>>>
      CandidateMap;
  typedef JS::GCHashMap<JS::Heap<JSObject*>, RefPtr<nsAtom>,
                        js::MovableCellHasher<JS::Heap<JSObject*>>,
                        js::SystemAllocPolicy>
      ConstructorMap;

  // Hashtable for custom element definitions in web components.
  // Custom prototypes are stored in the compartment where definition was
  // defined.
  DefinitionMap mCustomDefinitions;

  // Hashtable for chrome-only callbacks that is called *before* we return
  // a CustomElementDefinition, when the typeAtom matches.
  // The callbacks are registered with the setElementCreationCallback method.
  ElementCreationCallbackMap mElementCreationCallbacks;

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

  // If an element creation callback is found, the nsTHashtable for the
  // type is created here, and elements will later be upgraded.
  CandidateMap mElementCreationCallbacksUpgradeCandidatesMap;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  // It is used to prevent reentrant invocations of element definition.
  bool mIsCustomDefinitionRunning;

 private:
  class MOZ_RAII AutoSetRunningFlag final {
   public:
    explicit AutoSetRunningFlag(CustomElementRegistry* aRegistry)
        : mRegistry(aRegistry) {
      MOZ_ASSERT(!mRegistry->mIsCustomDefinitionRunning,
                 "IsCustomDefinitionRunning flag should be initially false");
      mRegistry->mIsCustomDefinitionRunning = true;
    }

    ~AutoSetRunningFlag() { mRegistry->mIsCustomDefinitionRunning = false; }

   private:
    CustomElementRegistry* mRegistry;
  };

  int32_t InferNamespace(JSContext* aCx, JS::Handle<JSObject*> constructor);

 public:
  nsISupports* GetParentObject() const;

  DocGroup* GetDocGroup() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void Define(JSContext* aCx, const nsAString& aName,
              CustomElementConstructor& aFunctionConstructor,
              const ElementDefinitionOptions& aOptions, ErrorResult& aRv);

  void Get(JSContext* cx, const nsAString& name,
           JS::MutableHandle<JS::Value> aRetVal);

  already_AddRefed<Promise> WhenDefined(const nsAString& aName,
                                        ErrorResult& aRv);

  // Chrome-only method that give JS an opportunity to only load the custom
  // element definition script when needed.
  void SetElementCreationCallback(const nsAString& aName,
                                  CustomElementCreationCallback& aCallback,
                                  ErrorResult& aRv);

  void Upgrade(nsINode& aRoot);
};

class MOZ_RAII AutoCEReaction final {
 public:
  // JSContext is allowed to be a nullptr if we are guaranteeing that we're
  // not doing something that might throw but not finish reporting a JS
  // exception during the lifetime of the AutoCEReaction.
  AutoCEReaction(CustomElementReactionsStack* aReactionsStack, JSContext* aCx)
      : mReactionsStack(aReactionsStack), mCx(aCx) {
    mIsElementQueuePushedForPreviousRecursionDepth =
        mReactionsStack->EnterCEReactions();
  }

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY because this is called from Maybe<>.reset().
  MOZ_CAN_RUN_SCRIPT_BOUNDARY ~AutoCEReaction() {
    mReactionsStack->LeaveCEReactions(
        mCx, mIsElementQueuePushedForPreviousRecursionDepth);
  }

 private:
  const RefPtr<CustomElementReactionsStack> mReactionsStack;
  JSContext* mCx;
  bool mIsElementQueuePushedForPreviousRecursionDepth;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CustomElementRegistry_h
