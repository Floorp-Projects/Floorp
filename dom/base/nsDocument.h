/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all our document implementations.
 */

#ifndef nsDocument_h___
#define nsDocument_h___

#include "nsIDocument.h"

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCRT.h"
#include "nsWeakReference.h"
#include "nsWeakPtr.h"
#include "nsVoidArray.h"
#include "nsTArray.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMDocumentXBL.h"
#include "nsStubDocumentObserver.h"
#include "nsIScriptGlobalObject.h"
#include "nsIContent.h"
#include "nsIPrincipal.h"
#include "nsIParser.h"
#include "nsBindingManager.h"
#include "nsInterfaceHashtable.h"
#include "nsJSThingHashtable.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsScriptLoader.h"
#include "nsIRadioGroupContainer.h"
#include "nsILayoutHistoryState.h"
#include "nsIRequest.h"
#include "nsILoadGroup.h"
#include "nsTObserverArray.h"
#include "nsStubMutationObserver.h"
#include "nsIChannel.h"
#include "nsCycleCollectionParticipant.h"
#include "nsContentList.h"
#include "nsGkAtoms.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheContainer.h"
#include "nsStyleSet.h"
#include "pldhash.h"
#include "nsAttrAndChildArray.h"
#include "nsDOMAttributeMap.h"
#include "nsIContentViewer.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsIProgressEventSink.h"
#include "nsISecurityEventSink.h"
#include "nsIChannelEventSink.h"
#include "imgIRequest.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/dom/DOMImplementation.h"
#include "mozilla/dom/StyleSheetList.h"
#include "nsDataHashtable.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Attributes.h"
#include "nsIDOMXPathEvaluator.h"
#include "jsfriendapi.h"
#include "ImportManager.h"

#define XML_DECLARATION_BITS_DECLARATION_EXISTS   (1 << 0)
#define XML_DECLARATION_BITS_ENCODING_EXISTS      (1 << 1)
#define XML_DECLARATION_BITS_STANDALONE_EXISTS    (1 << 2)
#define XML_DECLARATION_BITS_STANDALONE_YES       (1 << 3)


class nsDOMStyleSheetSetList;
class nsIOutputStream;
class nsDocument;
class nsIDTD;
class nsIRadioVisitor;
class nsIFormControl;
struct nsRadioGroupStruct;
class nsOnloadBlocker;
class nsUnblockOnloadEvent;
class nsChildContentList;
class nsHTMLStyleSheet;
class nsHTMLCSSStyleSheet;
class nsDOMNavigationTiming;
class nsWindowSizes;
class nsHtml5TreeOpExecutor;
class nsDocumentOnStack;
class nsPointerLockPermissionRequest;
class nsISecurityConsoleMessage;
class nsPIBoxObject;

namespace mozilla {
class EventChainPreVisitor;
namespace dom {
class BoxObject;
class UndoManager;
struct LifecycleCallbacks;
class CallbackFunction;
}
}

/**
 * Right now our identifier map entries contain information for 'name'
 * and 'id' mappings of a given string. This is so that
 * nsHTMLDocument::ResolveName only has to do one hash lookup instead
 * of two. It's not clear whether this still matters for performance.
 * 
 * We also store the document.all result list here. This is mainly so that
 * when all elements with the given ID are removed and we remove
 * the ID's nsIdentifierMapEntry, the document.all result is released too.
 * Perhaps the document.all results should have their own hashtable
 * in nsHTMLDocument.
 */
class nsIdentifierMapEntry : public nsStringHashKey
{
public:
  typedef mozilla::dom::Element Element;
  typedef mozilla::net::ReferrerPolicy ReferrerPolicy;

  explicit nsIdentifierMapEntry(const nsAString& aKey) :
    nsStringHashKey(&aKey), mNameContentList(nullptr)
  {
  }
  explicit nsIdentifierMapEntry(const nsAString* aKey) :
    nsStringHashKey(aKey), mNameContentList(nullptr)
  {
  }
  nsIdentifierMapEntry(const nsIdentifierMapEntry& aOther) :
    nsStringHashKey(&aOther.GetKey())
  {
    NS_ERROR("Should never be called");
  }
  ~nsIdentifierMapEntry();

  void AddNameElement(nsINode* aDocument, Element* aElement);
  void RemoveNameElement(Element* aElement);
  bool IsEmpty();
  nsBaseContentList* GetNameContentList() {
    return mNameContentList;
  }
  bool HasNameElement() const {
    return mNameContentList && mNameContentList->Length() != 0;
  }

  /**
   * Returns the element if we know the element associated with this
   * id. Otherwise returns null.
   */
  Element* GetIdElement();
  /**
   * Returns the list of all elements associated with this id.
   */
  const nsSmallVoidArray* GetIdElements() const {
    return &mIdContentList;
  }
  /**
   * If this entry has a non-null image element set (using SetImageElement),
   * the image element will be returned, otherwise the same as GetIdElement().
   */
  Element* GetImageIdElement();
  /**
   * Append all the elements with this id to aElements
   */
  void AppendAllIdContent(nsCOMArray<nsIContent>* aElements);
  /**
   * This can fire ID change callbacks.
   * @return true if the content could be added, false if we failed due
   * to OOM.
   */
  bool AddIdElement(Element* aElement);
  /**
   * This can fire ID change callbacks.
   */
  void RemoveIdElement(Element* aElement);
  /**
   * Set the image element override for this ID. This will be returned by
   * GetIdElement(true) if non-null.
   */
  void SetImageElement(Element* aElement);
  bool HasIdElementExposedAsHTMLDocumentProperty();

  bool HasContentChangeCallback() { return mChangeCallbacks != nullptr; }
  void AddContentChangeCallback(nsIDocument::IDTargetObserver aCallback,
                                void* aData, bool aForImage);
  void RemoveContentChangeCallback(nsIDocument::IDTargetObserver aCallback,
                                void* aData, bool aForImage);

  void Traverse(nsCycleCollectionTraversalCallback* aCallback);

  struct ChangeCallback {
    nsIDocument::IDTargetObserver mCallback;
    void* mData;
    bool mForImage;
  };

  struct ChangeCallbackEntry : public PLDHashEntryHdr {
    typedef const ChangeCallback KeyType;
    typedef const ChangeCallback* KeyTypePointer;

    explicit ChangeCallbackEntry(const ChangeCallback* aKey) :
      mKey(*aKey) { }
    ChangeCallbackEntry(const ChangeCallbackEntry& toCopy) :
      mKey(toCopy.mKey) { }

    KeyType GetKey() const { return mKey; }
    bool KeyEquals(KeyTypePointer aKey) const {
      return aKey->mCallback == mKey.mCallback &&
             aKey->mData == mKey.mData &&
             aKey->mForImage == mKey.mForImage;
    }

    static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }
    static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      return mozilla::HashGeneric(aKey->mCallback, aKey->mData);
    }
    enum { ALLOW_MEMMOVE = true };

    ChangeCallback mKey;
  };

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  void FireChangeCallbacks(Element* aOldElement, Element* aNewElement,
                           bool aImageOnly = false);

  // empty if there are no elements with this ID.
  // The elements are stored as weak pointers.
  nsSmallVoidArray mIdContentList;
  nsRefPtr<nsBaseContentList> mNameContentList;
  nsAutoPtr<nsTHashtable<ChangeCallbackEntry> > mChangeCallbacks;
  nsRefPtr<Element> mImageElement;
};

namespace mozilla {
namespace dom {

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

struct LifecycleCallbackArgs
{
  nsString name;
  nsString oldValue;
  nsString newValue;
};

struct CustomElementData;

class CustomElementCallback
{
public:
  CustomElementCallback(Element* aThisObject,
                        nsIDocument::ElementCallbackType aCallbackType,
                        mozilla::dom::CallbackFunction* aCallback,
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
  nsRefPtr<mozilla::dom::Element> mThisObject;
  nsRefPtr<mozilla::dom::CallbackFunction> mCallback;
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

class Registry : public nsISupports
{
public:
  friend class ::nsDocument;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Registry)

  Registry();

protected:
  virtual ~Registry();

  typedef nsClassHashtable<mozilla::dom::CustomElementHashKey,
                           mozilla::dom::CustomElementDefinition>
    DefinitionMap;
  typedef nsClassHashtable<mozilla::dom::CustomElementHashKey,
                           nsTArray<nsRefPtr<mozilla::dom::Element>>>
    CandidateMap;

  // Hashtable for custom element definitions in web components.
  // Custom prototypes are stored in the compartment where
  // registerElement was called.
  DefinitionMap mCustomDefinitions;

  // The "upgrade candidates map" from the web components spec. Maps from a
  // namespace id and local name to a list of elements to upgrade if that
  // element is registered as a custom element.
  CandidateMap mCandidatesMap;
};

} // namespace dom
} // namespace mozilla

class nsDocHeaderData
{
public:
  nsDocHeaderData(nsIAtom* aField, const nsAString& aData)
    : mField(aField), mData(aData), mNext(nullptr)
  {
  }

  ~nsDocHeaderData(void)
  {
    delete mNext;
  }

  nsCOMPtr<nsIAtom> mField;
  nsString          mData;
  nsDocHeaderData*  mNext;
};

class nsDOMStyleSheetList : public mozilla::dom::StyleSheetList,
                            public nsStubDocumentObserver
{
public:
  explicit nsDOMStyleSheetList(nsIDocument* aDocument);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETADDED
  NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETREMOVED

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  virtual nsINode* GetParentObject() const override
  {
    return mDocument;
  }

  virtual uint32_t Length() override;
  virtual mozilla::CSSStyleSheet*
  IndexedGetter(uint32_t aIndex, bool& aFound) override;

protected:
  virtual ~nsDOMStyleSheetList();

  int32_t       mLength;
  nsIDocument*  mDocument;
};

class nsOnloadBlocker final : public nsIRequest
{
public:
  nsOnloadBlocker() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST

private:
  ~nsOnloadBlocker() {}
};

class nsExternalResourceMap
{
public:
  typedef nsIDocument::ExternalResourceLoad ExternalResourceLoad;
  nsExternalResourceMap();

  /**
   * Request an external resource document.  This does exactly what
   * nsIDocument::RequestExternalResource is documented to do.
   */
  nsIDocument* RequestResource(nsIURI* aURI,
                               nsINode* aRequestingNode,
                               nsDocument* aDisplayDocument,
                               ExternalResourceLoad** aPendingLoad);

  /**
   * Enumerate the resource documents.  See
   * nsIDocument::EnumerateExternalResources.
   */
  void EnumerateResources(nsIDocument::nsSubDocEnumFunc aCallback, void* aData);

  /**
   * Traverse ourselves for cycle-collection
   */
  void Traverse(nsCycleCollectionTraversalCallback* aCallback) const;

  /**
   * Shut ourselves down (used for cycle-collection unlink), as well
   * as for document destruction.
   */
  void Shutdown()
  {
    mPendingLoads.Clear();
    mMap.Clear();
    mHaveShutDown = true;
  }

  bool HaveShutDown() const
  {
    return mHaveShutDown;
  }

  // Needs to be public so we can traverse them sanely
  struct ExternalResource
  {
    ~ExternalResource();
    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsIContentViewer> mViewer;
    nsCOMPtr<nsILoadGroup> mLoadGroup;
  };

  // Hide all our viewers
  void HideViewers();

  // Show all our viewers
  void ShowViewers();

protected:
  class PendingLoad : public ExternalResourceLoad,
                      public nsIStreamListener
  {
    ~PendingLoad() {}

  public:
    explicit PendingLoad(nsDocument* aDisplayDocument) :
      mDisplayDocument(aDisplayDocument)
    {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    /**
     * Start aURI loading.  This will perform the necessary security checks and
     * so forth.
     */
    nsresult StartLoad(nsIURI* aURI, nsINode* aRequestingNode);

    /**
     * Set up an nsIContentViewer based on aRequest.  This is guaranteed to
     * put null in *aViewer and *aLoadGroup on all failures.
     */
    nsresult SetupViewer(nsIRequest* aRequest, nsIContentViewer** aViewer,
                         nsILoadGroup** aLoadGroup);

  private:
    nsRefPtr<nsDocument> mDisplayDocument;
    nsCOMPtr<nsIStreamListener> mTargetListener;
    nsCOMPtr<nsIURI> mURI;
  };
  friend class PendingLoad;

  class LoadgroupCallbacks final : public nsIInterfaceRequestor
  {
    ~LoadgroupCallbacks() {}
  public:
    explicit LoadgroupCallbacks(nsIInterfaceRequestor* aOtherCallbacks)
      : mCallbacks(aOtherCallbacks)
    {}
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
  private:
    // The only reason it's safe to hold a strong ref here without leaking is
    // that the notificationCallbacks on a loadgroup aren't the docshell itself
    // but a shim that holds a weak reference to the docshell.
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;

    // Use shims for interfaces that docshell implements directly so that we
    // don't hand out references to the docshell.  The shims should all allow
    // getInterface back on us, but other than that each one should only
    // implement one interface.
    
    // XXXbz I wish we could just derive the _allcaps thing from _i
#define DECL_SHIM(_i, _allcaps)                                              \
    class _i##Shim final : public nsIInterfaceRequestor,                     \
                           public _i                                         \
    {                                                                        \
      ~_i##Shim() {}                                                         \
    public:                                                                  \
      _i##Shim(nsIInterfaceRequestor* aIfreq, _i* aRealPtr)                  \
        : mIfReq(aIfreq), mRealPtr(aRealPtr)                                 \
      {                                                                      \
        NS_ASSERTION(mIfReq, "Expected non-null here");                      \
        NS_ASSERTION(mRealPtr, "Expected non-null here");                    \
      }                                                                      \
      NS_DECL_ISUPPORTS                                                      \
      NS_FORWARD_NSIINTERFACEREQUESTOR(mIfReq->)                             \
      NS_FORWARD_##_allcaps(mRealPtr->)                                      \
    private:                                                                 \
      nsCOMPtr<nsIInterfaceRequestor> mIfReq;                                \
      nsCOMPtr<_i> mRealPtr;                                                 \
    };

    DECL_SHIM(nsILoadContext, NSILOADCONTEXT)
    DECL_SHIM(nsIProgressEventSink, NSIPROGRESSEVENTSINK)
    DECL_SHIM(nsIChannelEventSink, NSICHANNELEVENTSINK)
    DECL_SHIM(nsISecurityEventSink, NSISECURITYEVENTSINK)
    DECL_SHIM(nsIApplicationCacheContainer, NSIAPPLICATIONCACHECONTAINER)
#undef DECL_SHIM
  };

  /**
   * Add an ExternalResource for aURI.  aViewer and aLoadGroup might be null
   * when this is called if the URI didn't result in an XML document.  This
   * function makes sure to remove the pending load for aURI, if any, from our
   * hashtable, and to notify its observers, if any.
   */
  nsresult AddExternalResource(nsIURI* aURI, nsIContentViewer* aViewer,
                               nsILoadGroup* aLoadGroup,
                               nsIDocument* aDisplayDocument);
  
  nsClassHashtable<nsURIHashKey, ExternalResource> mMap;
  nsRefPtrHashtable<nsURIHashKey, PendingLoad> mPendingLoads;
  bool mHaveShutDown;
};

class CSPErrorQueue
{
  public:
    /**
     * Note this was designed to be passed string literals. If you give it
     * a dynamically allocated string, it is your responsibility to make sure
     * it never dies and is properly freed!
     */
    void Add(const char* aMessageName);
    void Flush(nsIDocument* aDocument);

    CSPErrorQueue()
    {
    }

    ~CSPErrorQueue()
    {
    }

  private:
    nsAutoTArray<const char*,5> mErrors;
};

// Base class for our document implementations.
//
// Note that this class *implements* nsIDOMXMLDocument, but it's not
// really an nsIDOMXMLDocument. The reason for implementing
// nsIDOMXMLDocument on this class is to avoid having to duplicate all
// its inherited methods on document classes that *are*
// nsIDOMXMLDocument's. nsDocument's QI should *not* claim to support
// nsIDOMXMLDocument unless someone writes a real implementation of
// the interface.
class nsDocument : public nsIDocument,
                   public nsIDOMXMLDocument, // inherits nsIDOMDocument
                   public nsIDOMDocumentXBL,
                   public nsSupportsWeakReference,
                   public nsIScriptObjectPrincipal,
                   public nsIRadioGroupContainer,
                   public nsIApplicationCacheContainer,
                   public nsStubMutationObserver,
                   public nsIObserver,
                   public nsIDOMXPathEvaluator
{
  friend class nsIDocument;

public:
  typedef mozilla::dom::Element Element;
  using nsIDocument::GetElementsByTagName;
  typedef mozilla::net::ReferrerPolicy ReferrerPolicy;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_SIZEOF_EXCLUDING_THIS

  virtual void Reset(nsIChannel *aChannel, nsILoadGroup *aLoadGroup) override;
  virtual void ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup,
                          nsIPrincipal* aPrincipal) override;

  // StartDocumentLoad is pure virtual so that subclasses must override it.
  // The nsDocument StartDocumentLoad does some setup, but does NOT set
  // *aDocListener; this is the job of subclasses.
  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     bool aReset = true,
                                     nsIContentSink* aContentSink = nullptr) override = 0;

  virtual void StopDocumentLoad() override;

  virtual void NotifyPossibleTitleChange(bool aBoundTitleElement) override;

  virtual void SetDocumentURI(nsIURI* aURI) override;

  virtual void SetChromeXHRDocURI(nsIURI* aURI) override;

  virtual void SetChromeXHRDocBaseURI(nsIURI* aURI) override;

  /**
   * Set the principal responsible for this document.
   */
  virtual void SetPrincipal(nsIPrincipal *aPrincipal) override;

  /**
   * Get the Content-Type of this document.
   */
  // NS_IMETHOD GetContentType(nsAString& aContentType);
  // Already declared in nsIDOMDocument

  /**
   * Set the Content-Type of this document.
   */
  virtual void SetContentType(const nsAString& aContentType) override;

  virtual nsresult SetBaseURI(nsIURI* aURI) override;

  /**
   * Get/Set the base target of a link in a document.
   */
  virtual void GetBaseTarget(nsAString &aBaseTarget) override;

  /**
   * Return a standard name for the document's character set. This will
   * trigger a startDocumentLoad if necessary to answer the question.
   */
  virtual void SetDocumentCharacterSet(const nsACString& aCharSetID) override;

  /**
   * Add an observer that gets notified whenever the charset changes.
   */
  virtual nsresult AddCharSetObserver(nsIObserver* aObserver) override;

  /**
   * Remove a charset observer.
   */
  virtual void RemoveCharSetObserver(nsIObserver* aObserver) override;

  virtual Element* AddIDTargetObserver(nsIAtom* aID, IDTargetObserver aObserver,
                                       void* aData, bool aForImage) override;
  virtual void RemoveIDTargetObserver(nsIAtom* aID, IDTargetObserver aObserver,
                                      void* aData, bool aForImage) override;

  /**
   * Access HTTP header data (this may also get set from other sources, like
   * HTML META tags).
   */
  virtual void GetHeaderData(nsIAtom* aHeaderField, nsAString& aData) const override;
  virtual void SetHeaderData(nsIAtom* aheaderField,
                             const nsAString& aData) override;

  /**
   * Create a new presentation shell that will use aContext for
   * its presentation context (presentation contexts <b>must not</b> be
   * shared among multiple presentation shells).
   */
  virtual already_AddRefed<nsIPresShell> CreateShell(nsPresContext* aContext,
                                                     nsViewManager* aViewManager,
                                                     nsStyleSet* aStyleSet) override;
  virtual void DeleteShell() override;

  virtual nsresult GetAllowPlugins(bool* aAllowPlugins) override;

  virtual already_AddRefed<mozilla::dom::UndoManager> GetUndoManager() override;

  static bool IsWebAnimationsEnabled(JSContext* aCx, JSObject* aObject);
  virtual mozilla::dom::DocumentTimeline* Timeline() override;

  virtual nsresult SetSubDocumentFor(Element* aContent,
                                     nsIDocument* aSubDoc) override;
  virtual nsIDocument* GetSubDocumentFor(nsIContent* aContent) const override;
  virtual Element* FindContentForSubDocument(nsIDocument *aDocument) const override;
  virtual Element* GetRootElementInternal() const override;

  virtual void EnsureOnDemandBuiltInUASheet(mozilla::CSSStyleSheet* aSheet) override;

  /**
   * Get the (document) style sheets owned by this document.
   * These are ordered, highest priority last
   */
  virtual int32_t GetNumberOfStyleSheets() const override;
  virtual nsIStyleSheet* GetStyleSheetAt(int32_t aIndex) const override;
  virtual int32_t GetIndexOfStyleSheet(nsIStyleSheet* aSheet) const override;
  virtual void AddStyleSheet(nsIStyleSheet* aSheet) override;
  virtual void RemoveStyleSheet(nsIStyleSheet* aSheet) override;

  virtual void UpdateStyleSheets(nsCOMArray<nsIStyleSheet>& aOldSheets,
                                 nsCOMArray<nsIStyleSheet>& aNewSheets) override;
  virtual void AddStyleSheetToStyleSets(nsIStyleSheet* aSheet);
  virtual void RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet);

  virtual void InsertStyleSheetAt(nsIStyleSheet* aSheet, int32_t aIndex) override;
  virtual void SetStyleSheetApplicableState(nsIStyleSheet* aSheet,
                                            bool aApplicable) override;

  virtual nsresult LoadAdditionalStyleSheet(additionalSheetType aType, nsIURI* aSheetURI) override;
  virtual nsresult AddAdditionalStyleSheet(additionalSheetType aType, nsIStyleSheet* aSheet) override;
  virtual void RemoveAdditionalStyleSheet(additionalSheetType aType, nsIURI* sheetURI) override;
  virtual nsIStyleSheet* FirstAdditionalAuthorSheet() override;

  virtual nsIChannel* GetChannel() const override {
    return mChannel;
  }

  virtual nsIChannel* GetFailedChannel() const override {
    return mFailedChannel;
  }
  virtual void SetFailedChannel(nsIChannel* aChannel) override {
    mFailedChannel = aChannel;
  }

  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject) override;

  virtual void SetScriptHandlingObject(nsIScriptGlobalObject* aScriptObject) override;

  virtual nsIGlobalObject* GetScopeObject() const override;
  void SetScopeObject(nsIGlobalObject* aGlobal) override;
  /**
   * Get the script loader for this document
   */
  virtual nsScriptLoader* ScriptLoader() override;

  /**
   * Add/Remove an element to the document's id and name hashes
   */
  virtual void AddToIdTable(Element* aElement, nsIAtom* aId) override;
  virtual void RemoveFromIdTable(Element* aElement, nsIAtom* aId) override;
  virtual void AddToNameTable(Element* aElement, nsIAtom* aName) override;
  virtual void RemoveFromNameTable(Element* aElement, nsIAtom* aName) override;

  /**
   * Add a new observer of document change notifications. Whenever
   * content is changed, appended, inserted or removed the observers are
   * informed.
   */
  virtual void AddObserver(nsIDocumentObserver* aObserver) override;

  /**
   * Remove an observer of document change notifications. This will
   * return false if the observer cannot be found.
   */
  virtual bool RemoveObserver(nsIDocumentObserver* aObserver) override;

  // Observation hooks used to propagate notifications to document
  // observers.
  virtual void BeginUpdate(nsUpdateType aUpdateType) override;
  virtual void EndUpdate(nsUpdateType aUpdateType) override;
  virtual void BeginLoad() override;
  virtual void EndLoad() override;

  virtual void SetReadyStateInternal(ReadyState rs) override;

  virtual void ContentStateChanged(nsIContent* aContent,
                                   mozilla::EventStates aStateMask)
                                     override;
  virtual void DocumentStatesChanged(
                 mozilla::EventStates aStateMask) override;

  virtual void StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aOldStyleRule,
                                nsIStyleRule* aNewStyleRule) override;
  virtual void StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) override;
  virtual void StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule) override;

  virtual void FlushPendingNotifications(mozFlushType aType) override;
  virtual void FlushExternalResources(mozFlushType aType) override;
  virtual void SetXMLDeclaration(const char16_t *aVersion,
                                 const char16_t *aEncoding,
                                 const int32_t aStandalone) override;
  virtual void GetXMLDeclaration(nsAString& aVersion,
                                 nsAString& aEncoding,
                                 nsAString& Standalone) override;
  virtual bool IsScriptEnabled() override;

  virtual void OnPageShow(bool aPersisted, mozilla::dom::EventTarget* aDispatchStartTarget) override;
  virtual void OnPageHide(bool aPersisted, mozilla::dom::EventTarget* aDispatchStartTarget) override;

  virtual void WillDispatchMutationEvent(nsINode* aTarget) override;
  virtual void MutationEventDispatched(nsINode* aTarget) override;

  // nsINode
  virtual bool IsNodeOfType(uint32_t aFlags) const override;
  virtual nsIContent *GetChildAt(uint32_t aIndex) const override;
  virtual nsIContent * const * GetChildArray(uint32_t* aChildCount) const override;
  virtual int32_t IndexOf(const nsINode* aPossibleChild) const override;
  virtual uint32_t GetChildCount() const override;
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify) override;
  virtual void RemoveChildAt(uint32_t aIndex, bool aNotify) override;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName,
                            nsIRadioVisitor* aVisitor,
                            bool aFlushContent) override;
  virtual void
    SetCurrentRadioButton(const nsAString& aName,
                          mozilla::dom::HTMLInputElement* aRadio) override;
  virtual mozilla::dom::HTMLInputElement*
    GetCurrentRadioButton(const nsAString& aName) override;
  NS_IMETHOD
    GetNextRadioButton(const nsAString& aName,
                       const bool aPrevious,
                       mozilla::dom::HTMLInputElement*  aFocusedRadio,
                       mozilla::dom::HTMLInputElement** aRadioOut) override;
  virtual void AddToRadioGroup(const nsAString& aName,
                               nsIFormControl* aRadio) override;
  virtual void RemoveFromRadioGroup(const nsAString& aName,
                                    nsIFormControl* aRadio) override;
  virtual uint32_t GetRequiredRadioCount(const nsAString& aName) const override;
  virtual void RadioRequiredWillChange(const nsAString& aName,
                                       bool aRequiredAdded) override;
  virtual bool GetValueMissingState(const nsAString& aName) const override;
  virtual void SetValueMissingState(const nsAString& aName, bool aValue) override;

  // for radio group
  nsRadioGroupStruct* GetRadioGroup(const nsAString& aName) const;
  nsRadioGroupStruct* GetOrCreateRadioGroup(const nsAString& aName);

  virtual nsViewportInfo GetViewportInfo(const mozilla::ScreenIntSize& aDisplaySize) override;

  /**
   * Called when an app-theme-changed observer notification is
   * received by this document.
   */
  void OnAppThemeChanged();

private:
  void AddOnDemandBuiltInUASheet(mozilla::CSSStyleSheet* aSheet);
  nsRadioGroupStruct* GetRadioGroupInternal(const nsAString& aName) const;
  void SendToConsole(nsCOMArray<nsISecurityConsoleMessage>& aMessages);

public:
  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE_OVERRIDABLE

  // nsIDOMDocument
  NS_DECL_NSIDOMDOCUMENT

  // nsIDOMXMLDocument
  NS_DECL_NSIDOMXMLDOCUMENT

  // nsIDOMDocumentXBL
  NS_DECL_NSIDOMDOCUMENTXBL

  // nsIDOMEventTarget
  virtual nsresult PreHandleEvent(
                     mozilla::EventChainPreVisitor& aVisitor) override;
  virtual mozilla::EventListenerManager*
    GetOrCreateListenerManager() override;
  virtual mozilla::EventListenerManager*
    GetExistingListenerManager() const override;

  // nsIScriptObjectPrincipal
  virtual nsIPrincipal* GetPrincipal() override;

  // nsIApplicationCacheContainer
  NS_DECL_NSIAPPLICATIONCACHECONTAINER

  // nsIObserver
  NS_DECL_NSIOBSERVER

  NS_DECL_NSIDOMXPATHEVALUATOR

  virtual nsresult Init();

  virtual nsresult CreateElem(const nsAString& aName, nsIAtom *aPrefix,
                              int32_t aNamespaceID,
                              nsIContent **aResult) override;

  virtual void Sanitize() override;

  virtual void EnumerateSubDocuments(nsSubDocEnumFunc aCallback,
                                                 void *aData) override;

  virtual bool CanSavePresentation(nsIRequest *aNewRequest) override;
  virtual void Destroy() override;
  virtual void RemovedFromDocShell() override;
  virtual already_AddRefed<nsILayoutHistoryState> GetLayoutHistoryState() const override;

  virtual void BlockOnload() override;
  virtual void UnblockOnload(bool aFireSync) override;

  virtual void AddStyleRelevantLink(mozilla::dom::Link* aLink) override;
  virtual void ForgetLink(mozilla::dom::Link* aLink) override;

  virtual void ClearBoxObjectFor(nsIContent* aContent) override;

  virtual already_AddRefed<mozilla::dom::BoxObject>
  GetBoxObjectFor(mozilla::dom::Element* aElement,
                  mozilla::ErrorResult& aRv) override;

  virtual Element*
    GetAnonymousElementByAttribute(nsIContent* aElement,
                                   nsIAtom* aAttrName,
                                   const nsAString& aAttrValue) const override;

  virtual Element* ElementFromPointHelper(float aX, float aY,
                                                      bool aIgnoreRootScrollFrame,
                                                      bool aFlushLayout) override;

  virtual nsresult NodesFromRectHelper(float aX, float aY,
                                                   float aTopSize, float aRightSize,
                                                   float aBottomSize, float aLeftSize,
                                                   bool aIgnoreRootScrollFrame,
                                                   bool aFlushLayout,
                                                   nsIDOMNodeList** aReturn) override;

  virtual void FlushSkinBindings() override;

  virtual nsresult InitializeFrameLoader(nsFrameLoader* aLoader) override;
  virtual nsresult FinalizeFrameLoader(nsFrameLoader* aLoader, nsIRunnable* aFinalizer) override;
  virtual void TryCancelFrameLoaderInitialization(nsIDocShell* aShell) override;
  virtual nsIDocument*
    RequestExternalResource(nsIURI* aURI,
                            nsINode* aRequestingNode,
                            ExternalResourceLoad** aPendingLoad) override;
  virtual void
    EnumerateExternalResources(nsSubDocEnumFunc aCallback, void* aData) override;

  nsTArray<nsCString> mHostObjectURIs;

  // Returns our (lazily-initialized) animation controller.
  // If HasAnimationController is true, this is guaranteed to return non-null.
  nsSMILAnimationController* GetAnimationController() override;

  virtual mozilla::PendingAnimationTracker*
  GetPendingAnimationTracker() final override
  {
    return mPendingAnimationTracker;
  }

  virtual mozilla::PendingAnimationTracker*
  GetOrCreatePendingAnimationTracker() override;

  void SetImagesNeedAnimating(bool aAnimating) override;

  virtual void SuppressEventHandling(SuppressionType aWhat,
                                     uint32_t aIncrease) override;

  virtual void UnsuppressEventHandlingAndFireEvents(SuppressionType aWhat,
                                                    bool aFireEvents) override;

  void DecreaseEventSuppression() {
    MOZ_ASSERT(mEventsSuppressed);
    --mEventsSuppressed;
    MaybeRescheduleAnimationFrameNotifications();
  }

  void ResumeAnimations() {
    MOZ_ASSERT(mAnimationsPaused);
    --mAnimationsPaused;
    MaybeRescheduleAnimationFrameNotifications();
  }

  virtual nsIDocument* GetTemplateContentsOwner() override;

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDocument,
                                                                   nsIDocument)

  void DoNotifyPossibleTitleChange();

  nsExternalResourceMap& ExternalResourceMap()
  {
    return mExternalResourceMap;
  }

  void SetLoadedAsData(bool aLoadedAsData) { mLoadedAsData = aLoadedAsData; }
  void SetLoadedAsInteractiveData(bool aLoadedAsInteractiveData)
  {
    mLoadedAsInteractiveData = aLoadedAsInteractiveData;
  }

  nsresult CloneDocHelper(nsDocument* clone) const;

  void MaybeInitializeFinalizeFrameLoaders();

  void MaybeEndOutermostXBLUpdate();

  virtual void PreloadPictureOpened() override;
  virtual void PreloadPictureClosed() override;

  virtual void
    PreloadPictureImageSource(const nsAString& aSrcsetAttr,
                              const nsAString& aSizesAttr,
                              const nsAString& aTypeAttr,
                              const nsAString& aMediaAttr) override;

  virtual already_AddRefed<nsIURI>
    ResolvePreloadImage(nsIURI *aBaseURI,
                        const nsAString& aSrcAttr,
                        const nsAString& aSrcsetAttr,
                        const nsAString& aSizesAttr) override;

  virtual void MaybePreLoadImage(nsIURI* uri,
                                 const nsAString &aCrossOriginAttr,
                                 ReferrerPolicy aReferrerPolicy) override;
  virtual void ForgetImagePreload(nsIURI* aURI) override;

  virtual void PreloadStyle(nsIURI* uri, const nsAString& charset,
                            const nsAString& aCrossOriginAttr,
                            ReferrerPolicy aReferrerPolicy) override;

  virtual nsresult LoadChromeSheetSync(nsIURI* uri, bool isAgentSheet,
                                       mozilla::CSSStyleSheet** sheet) override;

  virtual nsISupports* GetCurrentContentSink() override;

  virtual mozilla::EventStates GetDocumentState() override;

  virtual void RegisterHostObjectUri(const nsACString& aUri) override;
  virtual void UnregisterHostObjectUri(const nsACString& aUri) override;

  // Only BlockOnload should call this!
  void AsyncBlockOnload();

  virtual void SetScrollToRef(nsIURI *aDocumentURI) override;
  virtual void ScrollToRef() override;
  virtual void ResetScrolledToRefAlready() override;
  virtual void SetChangeScrollPosWhenScrollingToRef(bool aValue) override;

  virtual Element *GetElementById(const nsAString& aElementId) override;
  virtual const nsSmallVoidArray* GetAllElementsForId(const nsAString& aElementId) const override;

  virtual Element *LookupImageElement(const nsAString& aElementId) override;
  virtual void MozSetImageElement(const nsAString& aImageElementId,
                                  Element* aElement) override;

  virtual nsresult AddImage(imgIRequest* aImage) override;
  virtual nsresult RemoveImage(imgIRequest* aImage, uint32_t aFlags) override;
  virtual nsresult SetImageLockingState(bool aLocked) override;

  // AddPlugin adds a plugin-related element to mPlugins when the element is
  // added to the tree.
  virtual nsresult AddPlugin(nsIObjectLoadingContent* aPlugin) override;
  // RemovePlugin removes a plugin-related element to mPlugins when the
  // element is removed from the tree.
  virtual void RemovePlugin(nsIObjectLoadingContent* aPlugin) override;
  // GetPlugins returns the plugin-related elements from
  // the frame and any subframes.
  virtual void GetPlugins(nsTArray<nsIObjectLoadingContent*>& aPlugins) override;

  virtual nsresult GetStateObject(nsIVariant** aResult) override;

  virtual nsDOMNavigationTiming* GetNavigationTiming() const override;
  virtual nsresult SetNavigationTiming(nsDOMNavigationTiming* aTiming) override;

  virtual Element* FindImageMap(const nsAString& aNormalizedMapName) override;

  virtual Element* GetFullScreenElement() override;
  virtual void AsyncRequestFullScreen(Element* aElement,
                                      mozilla::dom::FullScreenOptions& aOptions) override;
  virtual void RestorePreviousFullScreenState() override;
  virtual bool IsFullscreenLeaf() override;
  virtual bool IsFullScreenDoc() override;
  virtual void SetApprovedForFullscreen(bool aIsApproved) override;
  virtual nsresult RemoteFrameFullscreenChanged(nsIDOMElement* aFrameElement,
                                                const nsAString& aNewOrigin) override;

  virtual nsresult RemoteFrameFullscreenReverted() override;
  virtual nsIDocument* GetFullscreenRoot() override;
  virtual void SetFullscreenRoot(nsIDocument* aRoot) override;

  // Returns the size of the mBlockedTrackingNodes array. (nsIDocument.h)
  //
  // This array contains nodes that have been blocked to prevent
  // user tracking. They most likely have had their nsIChannel
  // canceled by the URL classifier (Safebrowsing).
  //
  // A script can subsequently use GetBlockedTrackingNodes()
  // to get a list of references to these nodes.
  //
  // Note:
  // This expresses how many tracking nodes have been blocked for this
  // document since its beginning, not how many of them are still around
  // in the DOM tree. Weak references to blocked nodes are added in the
  // mBlockedTrackingNodesArray but they are not removed when those nodes
  // are removed from the tree or even garbage collected.
  long BlockedTrackingNodeCount() const;

  //
  // Returns strong references to mBlockedTrackingNodes. (nsIDocument.h)
  //
  // This array contains nodes that have been blocked to prevent
  // user tracking. They most likely have had their nsIChannel
  // canceled by the URL classifier (Safebrowsing).
  //
  already_AddRefed<nsSimpleContentList> BlockedTrackingNodes() const;

  static void ExitFullscreen(nsIDocument* aDoc);

  // This is called asynchronously by nsIDocument::AsyncRequestFullScreen()
  // to move this document into full-screen mode if allowed. aWasCallerChrome
  // should be true when nsIDocument::AsyncRequestFullScreen() was called
  // by chrome code. aNotifyOnOriginChange denotes whether we should send a
  // fullscreen-origin-change notification if requesting fullscreen in this
  // document causes the origin which is fullscreen to change. We may want to
  // *not* send this notification if we're calling RequestFullscreen() as part
  // of a continuation of a request in a subdocument, whereupon the caller will
  // need to send the notification with the origin of the document which
  // originally requested fullscreen, not *this* document's origin.
  void RequestFullScreen(Element* aElement,
                         mozilla::dom::FullScreenOptions& aOptions,
                         bool aWasCallerChrome,
                         bool aNotifyOnOriginChange);

  // Removes all elements from the full-screen stack, removing full-scren
  // styles from the top element in the stack.
  void CleanupFullscreenState();

  // Add/remove "fullscreen-approved" observer service notification listener.
  // Chrome sends us a notification when fullscreen is approved for a
  // document, with the notification subject as the document that was approved.
  // We maintain this listener while in fullscreen mode.
  nsresult AddFullscreenApprovedObserver();
  nsresult RemoveFullscreenApprovedObserver();

  // Pushes aElement onto the full-screen stack, and removes full-screen styles
  // from the former full-screen stack top, and its ancestors, and applies the
  // styles to aElement. aElement becomes the new "full-screen element".
  bool FullScreenStackPush(Element* aElement);

  // Remove the top element from the full-screen stack. Removes the full-screen
  // styles from the former top element, and applies them to the new top
  // element, if there is one.
  void FullScreenStackPop();

  // Returns the top element from the full-screen stack.
  Element* FullScreenStackTop();

  // DOM-exposed fullscreen API
  virtual bool MozFullScreenEnabled() override;
  virtual Element* GetMozFullScreenElement(mozilla::ErrorResult& rv) override;

  void RequestPointerLock(Element* aElement) override;
  bool ShouldLockPointer(Element* aElement, Element* aCurrentLock,
                         bool aNoFocusCheck = false);
  bool SetPointerLock(Element* aElement, int aCursorStyle);
  static void UnlockPointer(nsIDocument* aDoc = nullptr);

  // This method may fire a DOM event; if it does so it will happen
  // synchronously.
  void UpdateVisibilityState();
  // Posts an event to call UpdateVisibilityState
  virtual void PostVisibilityUpdateEvent() override;

  virtual void DocAddSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const override;
  // DocAddSizeOfIncludingThis is inherited from nsIDocument.

  virtual nsIDOMNode* AsDOMNode() override { return this; }

  virtual void EnqueueLifecycleCallback(nsIDocument::ElementCallbackType aType,
                                        Element* aCustomElement,
                                        mozilla::dom::LifecycleCallbackArgs* aArgs = nullptr,
                                        mozilla::dom::CustomElementDefinition* aDefinition = nullptr) override;

  static void ProcessTopElementQueue(bool aIsBaseQueue = false);

  void GetCustomPrototype(int32_t aNamespaceID,
                          nsIAtom* aAtom,
                          JS::MutableHandle<JSObject*> prototype)
  {
    if (!mRegistry) {
      prototype.set(nullptr);
      return;
    }

    mozilla::dom::CustomElementHashKey key(aNamespaceID, aAtom);
    mozilla::dom::CustomElementDefinition* definition;
    if (mRegistry->mCustomDefinitions.Get(&key, &definition)) {
      prototype.set(definition->mPrototype);
    } else {
      prototype.set(nullptr);
    }
  }

  static bool RegisterEnabled();

  virtual nsresult RegisterUnresolvedElement(mozilla::dom::Element* aElement,
                                             nsIAtom* aTypeName = nullptr) override;

  // WebIDL bits
  virtual mozilla::dom::DOMImplementation*
    GetImplementation(mozilla::ErrorResult& rv) override;
  virtual void
    RegisterElement(JSContext* aCx, const nsAString& aName,
                    const mozilla::dom::ElementRegistrationOptions& aOptions,
                    JS::MutableHandle<JSObject*> aRetval,
                    mozilla::ErrorResult& rv) override;
  virtual mozilla::dom::StyleSheetList* StyleSheets() override;
  virtual void SetSelectedStyleSheetSet(const nsAString& aSheetSet) override;
  virtual void GetLastStyleSheetSet(nsString& aSheetSet) override;
  virtual mozilla::dom::DOMStringList* StyleSheetSets() override;
  virtual void EnableStyleSheetsForSet(const nsAString& aSheetSet) override;
  using nsIDocument::CreateElement;
  using nsIDocument::CreateElementNS;
  virtual already_AddRefed<Element> CreateElement(const nsAString& aTagName,
                                                  const nsAString& aTypeExtension,
                                                  mozilla::ErrorResult& rv) override;
  virtual already_AddRefed<Element> CreateElementNS(const nsAString& aNamespaceURI,
                                                    const nsAString& aQualifiedName,
                                                    const nsAString& aTypeExtension,
                                                    mozilla::ErrorResult& rv) override;
  virtual void UseRegistryFromDocument(nsIDocument* aDocument) override;

  virtual nsIDocument* MasterDocument() override
  {
    return mMasterDocument ? mMasterDocument.get()
                           : this;
  }

  virtual void SetMasterDocument(nsIDocument* master) override
  {
    MOZ_ASSERT(master);
    mMasterDocument = master;
    UseRegistryFromDocument(mMasterDocument);
  }

  virtual bool IsMasterDocument() override
  {
    return !mMasterDocument;
  }

  virtual mozilla::dom::ImportManager* ImportManager() override
  {
    if (mImportManager) {
      MOZ_ASSERT(!mMasterDocument, "Only the master document has ImportManager set");
      return mImportManager.get();
    }

    if (mMasterDocument) {
      return mMasterDocument->ImportManager();
    }

    // ImportManager is created lazily.
    // If the manager is not yet set it has to be the
    // master document and this is the first import in it.
    // Let's create a new manager.
    mImportManager = new mozilla::dom::ImportManager();
    return mImportManager.get();
  }

  virtual bool HasSubImportLink(nsINode* aLink) override
  {
    return mSubImportLinks.Contains(aLink);
  }

  virtual uint32_t IndexOfSubImportLink(nsINode* aLink) override
  {
    return mSubImportLinks.IndexOf(aLink);
  }

  virtual void AddSubImportLink(nsINode* aLink) override
  {
    mSubImportLinks.AppendElement(aLink);
  }

  virtual nsINode* GetSubImportLink(uint32_t aIdx) override
  {
    return aIdx < mSubImportLinks.Length() ? mSubImportLinks[aIdx].get()
                                           : nullptr;
  }

  virtual void UnblockDOMContentLoaded() override;

protected:
  friend class nsNodeUtils;
  friend class nsDocumentOnStack;

  void IncreaseStackRefCnt()
  {
    ++mStackRefCnt;
  }

  void DecreaseStackRefCnt()
  {
    if (--mStackRefCnt == 0 && mNeedsReleaseAfterStackRefCntRelease) {
      mNeedsReleaseAfterStackRefCntRelease = false;
      NS_RELEASE_THIS();
    }
  }

  // Returns true if a request for DOM full-screen is currently enabled in
  // this document. This returns true if there are no windowed plugins in this
  // doc tree, and if the document is visible, and if the api is not
  // disabled by pref. aIsCallerChrome must contain the return value of
  // nsContentUtils::IsCallerChrome() from the context we're checking.
  // If aLogFailure is true, an appropriate warning message is logged to the
  // console, and a "mozfullscreenerror" event is dispatched to this document.
  bool IsFullScreenEnabled(bool aIsCallerChrome, bool aLogFailure);

  /**
   * Check that aId is not empty and log a message to the console
   * service if it is.
   * @returns true if aId looks correct, false otherwise.
   */
  inline bool CheckGetElementByIdArg(const nsAString& aId)
  {
    if (aId.IsEmpty()) {
      ReportEmptyGetElementByIdArg();
      return false;
    }
    return true;
  }

  void ReportEmptyGetElementByIdArg();

  void DispatchContentLoadedEvents();

  void RetrieveRelevantHeaders(nsIChannel *aChannel);

  void TryChannelCharset(nsIChannel *aChannel,
                         int32_t& aCharsetSource,
                         nsACString& aCharset,
                         nsHtml5TreeOpExecutor* aExecutor);

  // Call this before the document does something that will unbind all content.
  // That will stop us from doing a lot of work as each element is removed.
  void DestroyElementMaps();

  // Refreshes the hrefs of all the links in the document.
  void RefreshLinkHrefs();

  nsIContent* GetFirstBaseNodeWithHref();
  nsresult SetFirstBaseNodeWithHref(nsIContent *node);

  // Get the first <title> element with the given IsNodeOfType type, or
  // return null if there isn't one
  nsIContent* GetTitleContent(uint32_t aNodeType);
  // Find the first "title" element in the given IsNodeOfType type and
  // append the concatenation of its text node children to aTitle. Do
  // nothing if there is no such element.
  void GetTitleFromElement(uint32_t aNodeType, nsAString& aTitle);
public:
  // Get our title
  virtual void GetTitle(nsString& aTitle) override;
  // Set our title
  virtual void SetTitle(const nsAString& aTitle, mozilla::ErrorResult& rv) override;

  static void XPCOMShutdown();

  bool mIsTopLevelContentDocument:1;

  bool IsTopLevelContentDocument();

  void SetIsTopLevelContentDocument(bool aIsTopLevelContentDocument);

  js::ExpandoAndGeneration mExpandoAndGeneration;

#ifdef MOZ_EME
  bool ContainsEMEContent();
#endif

  bool ContainsMSEContent();

protected:
  already_AddRefed<nsIPresShell> doCreateShell(nsPresContext* aContext,
                                               nsViewManager* aViewManager,
                                               nsStyleSet* aStyleSet,
                                               nsCompatibility aCompatMode);

  void RemoveDocStyleSheetsFromStyleSets();
  void RemoveStyleSheetsFromStyleSets(nsCOMArray<nsIStyleSheet>& aSheets, 
                                      nsStyleSet::sheetType aType);
  void ResetStylesheetsToURI(nsIURI* aURI);
  void FillStyleSet(nsStyleSet* aStyleSet);

  // Return whether all the presshells for this document are safe to flush
  bool IsSafeToFlush() const;

  void DispatchPageTransition(mozilla::dom::EventTarget* aDispatchTarget,
                              const nsAString& aType,
                              bool aPersisted);

  virtual nsPIDOMWindow *GetWindowInternal() const override;
  virtual nsIScriptGlobalObject* GetScriptHandlingObjectInternal() const override;
  virtual bool InternalAllowXULXBL() override;

#define NS_DOCUMENT_NOTIFY_OBSERVERS(func_, params_)                        \
  NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers, nsIDocumentObserver, \
                                           func_, params_);

#ifdef DEBUG
  void VerifyRootContentState();
#endif

  explicit nsDocument(const char* aContentType);
  virtual ~nsDocument();

  void EnsureOnloadBlocker();

  void NotifyStyleSheetApplicableStateChanged();

  nsTArray<nsIObserver*> mCharSetObservers;

  PLDHashTable *mSubDocuments;

  // Array of owning references to all children
  nsAttrAndChildArray mChildren;

  // Pointer to our parser if we're currently in the process of being
  // parsed into.
  nsCOMPtr<nsIParser> mParser;

  // Weak reference to our sink for in case we no longer have a parser.  This
  // will allow us to flush out any pending stuff from the sink even if
  // EndLoad() has already happened.
  nsWeakPtr mWeakSink;

  nsCOMArray<nsIStyleSheet> mStyleSheets;
  nsCOMArray<nsIStyleSheet> mOnDemandBuiltInUASheets;
  nsCOMArray<nsIStyleSheet> mAdditionalSheets[SheetTypeCount];

  // Array of observers
  nsTObserverArray<nsIDocumentObserver*> mObservers;

  // Tracker for animations that are waiting to start.
  // nullptr until GetOrCreatePendingAnimationTracker is called.
  nsRefPtr<mozilla::PendingAnimationTracker> mPendingAnimationTracker;

  // Weak reference to the scope object (aka the script global object)
  // that, unlike mScriptGlobalObject, is never unset once set. This
  // is a weak reference to avoid leaks due to circular references.
  nsWeakPtr mScopeObject;

  // Stack of full-screen elements. When we request full-screen we push the
  // full-screen element onto this stack, and when we cancel full-screen we
  // pop one off this stack, restoring the previous full-screen state
  nsTArray<nsWeakPtr> mFullScreenStack;

  // The root of the doc tree in which this document is in. This is only
  // non-null when this document is in fullscreen mode.
  nsWeakPtr mFullscreenRoot;

private:
  // Array representing the processing stack in the custom elements
  // specification. The processing stack is conceptually a stack of
  // element queues. Each queue is represented by a sequence of
  // CustomElementData in this array, separated by nullptr that
  // represent the boundaries of the items in the stack. The first
  // queue in the stack is the base element queue.
  static mozilla::Maybe<nsTArray<nsRefPtr<mozilla::dom::CustomElementData>>> sProcessingStack;

  // Flag to prevent re-entrance into base element queue as described in the
  // custom elements speicification.
  static bool sProcessingBaseElementQueue;

  static bool CustomElementConstructor(JSContext* aCx, unsigned aArgc, JS::Value* aVp);

public:
  static void ProcessBaseElementQueue();

  // Enqueue created callback or register upgrade candidate for
  // newly created custom elements, possibly extending an existing type.
  // ex. <x-button>, <button is="x-button> (type extension)
  virtual void SetupCustomElement(Element* aElement,
                                  uint32_t aNamespaceID,
                                  const nsAString* aTypeExtension) override;

  static bool IsWebComponentsEnabled(JSContext* aCx, JSObject* aObject);

  // The "registry" from the web components spec.
  nsRefPtr<mozilla::dom::Registry> mRegistry;

  nsRefPtr<mozilla::EventListenerManager> mListenerManager;
  nsRefPtr<mozilla::dom::StyleSheetList> mDOMStyleSheets;
  nsRefPtr<nsDOMStyleSheetSetList> mStyleSheetSetList;
  nsRefPtr<nsScriptLoader> mScriptLoader;
  nsDocHeaderData* mHeaderData;
  /* mIdentifierMap works as follows for IDs:
   * 1) Attribute changes affect the table immediately (removing and adding
   *    entries as needed).
   * 2) Removals from the DOM affect the table immediately
   * 3) Additions to the DOM always update existing entries for names, and add
   *    new ones for IDs.
   */
  nsTHashtable<nsIdentifierMapEntry> mIdentifierMap;

  nsClassHashtable<nsStringHashKey, nsRadioGroupStruct> mRadioGroups;

  // Recorded time of change to 'loading' state.
  mozilla::TimeStamp mLoadingTimeStamp;

  // True if the document has been detached from its content viewer.
  bool mIsGoingAway:1;
  // True if the document is being destroyed.
  bool mInDestructor:1;

  // True if this document has ever had an HTML or SVG <title> element
  // bound to it
  bool mMayHaveTitleElement:1;

  bool mHasWarnedAboutBoxObjects:1;

  bool mDelayFrameLoaderInitialization:1;

  bool mSynchronousDOMContentLoaded:1;

  bool mInXBLUpdate:1;

  // Whether we're currently holding a lock on all of our images.
  bool mLockingImages:1;

  // Whether we currently require our images to animate
  bool mAnimatingImages:1;

  // Whether we're currently under a FlushPendingNotifications call to
  // our presshell.  This is used to handle flush reentry correctly.
  bool mInFlush:1;

  // Parser aborted. True if the parser of this document was forcibly
  // terminated instead of letting it finish at its own pace.
  bool mParserAborted:1;

  // Whether this document has been approved for fullscreen, either by explicit
  // approval via the fullscreen-approval UI, or because it received
  // approval because its document's host already had the "fullscreen"
  // permission granted when the document requested fullscreen.
  // 
  // Note if a document's principal doesn't have a host, the permission manager
  // can't store permissions for it, so we can only manage approval using this
  // flag.
  //
  // Note we must track this separately from the "fullscreen" permission,
  // so that pending pointer lock requests can determine whether documents
  // whose principal doesn't have a host (i.e. those which can't store
  // permissions in the permission manager) have been approved for fullscreen.
  bool mIsApprovedForFullscreen:1;

  // Whether this document has a fullscreen approved observer. Only documents
  // which request fullscreen and which don't have a pre-existing approval for
  // fullscreen will have an observer.
  bool mHasFullscreenApprovedObserver:1;

  friend class nsPointerLockPermissionRequest;
  friend class nsCallRequestFullScreen;
  // When set, trying to lock the pointer doesn't require permission from the
  // user.
  bool mAllowRelocking:1;

  bool mAsyncFullscreenPending:1;

  // Whether we're observing the "app-theme-changed" observer service
  // notification.  We need to keep track of this because we might get multiple
  // OnPageShow notifications in a row without an OnPageHide in between, if
  // we're getting document.open()/close() called on us.
  bool mObservingAppThemeChanged:1;

  // Keeps track of whether we have a pending
  // 'style-sheet-applicable-state-changed' notification.
  bool mSSApplicableStateNotificationPending:1;

  uint32_t mCancelledPointerLockRequests;

  uint8_t mXMLDeclarationBits;

  nsInterfaceHashtable<nsPtrHashKey<nsIContent>, nsPIBoxObject> *mBoxObjectTable;

  // A document "without a browsing context" that owns the content of
  // HTMLTemplateElement.
  nsCOMPtr<nsIDocument> mTemplateContentsOwner;

  // Our update nesting level
  uint32_t mUpdateNestLevel;

  // The application cache that this document is associated with, if
  // any.  This can change during the lifetime of the document.
  nsCOMPtr<nsIApplicationCache> mApplicationCache;

  nsCOMPtr<nsIContent> mFirstBaseNodeWithHref;

  mozilla::EventStates mDocumentState;
  mozilla::EventStates mGotDocumentState;

  nsRefPtr<nsDOMNavigationTiming> mTiming;
private:
  friend class nsUnblockOnloadEvent;
  // Recomputes the visibility state but doesn't set the new value.
  mozilla::dom::VisibilityState GetVisibilityState() const;
  void NotifyStyleSheetAdded(nsIStyleSheet* aSheet, bool aDocumentSheet);
  void NotifyStyleSheetRemoved(nsIStyleSheet* aSheet, bool aDocumentSheet);

  void PostUnblockOnloadEvent();
  void DoUnblockOnload();

  nsresult CheckFrameOptions();
  bool IsLoopDocument(nsIChannel* aChannel);
  nsresult InitCSP(nsIChannel* aChannel);

  void FlushCSPWebConsoleErrorQueue()
  {
    mCSPWebConsoleErrorQueue.Flush(this);
  }

  /**
   * Find the (non-anonymous) content in this document for aFrame. It will
   * be aFrame's content node if that content is in this document and not
   * anonymous. Otherwise, when aFrame is in a subdocument, we use the frame
   * element containing the subdocument containing aFrame, and/or find the
   * nearest non-anonymous ancestor in this document.
   * Returns null if there is no such element.
   */
  nsIContent* GetContentInThisDocument(nsIFrame* aFrame) const;

  // Just like EnableStyleSheetsForSet, but doesn't check whether
  // aSheetSet is null and allows the caller to control whether to set
  // aSheetSet as the preferred set in the CSSLoader.
  void EnableStyleSheetsForSetInternal(const nsAString& aSheetSet,
                                       bool aUpdateCSSLoader);

  // Revoke any pending notifications due to mozRequestAnimationFrame calls
  void RevokeAnimationFrameNotifications();
  // Reschedule any notifications we need to handle
  // mozRequestAnimationFrame, if it's OK to do so.
  void MaybeRescheduleAnimationFrameNotifications();

  // These are not implemented and not supported.
  nsDocument(const nsDocument& aOther);
  nsDocument& operator=(const nsDocument& aOther);

  // The layout history state that should be used by nodes in this
  // document.  We only actually store a pointer to it when:
  // 1)  We have no script global object.
  // 2)  We haven't had Destroy() called on us yet.
  nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;

  // Currently active onload blockers
  uint32_t mOnloadBlockCount;
  // Onload blockers which haven't been activated yet
  uint32_t mAsyncOnloadBlockCount;
  nsCOMPtr<nsIRequest> mOnloadBlocker;

  // A hashtable of styled links keyed by address pointer.
  nsTHashtable<nsPtrHashKey<mozilla::dom::Link> > mStyledLinks;
#ifdef DEBUG
  // Indicates whether mStyledLinks was cleared or not.  This is used to track
  // state so we can provide useful assertions to consumers of ForgetLink and
  // AddStyleRelevantLink.
  bool mStyledLinksCleared;
#endif

  // Member to store out last-selected stylesheet set.
  nsString mLastStyleSheetSet;

  nsTArray<nsRefPtr<nsFrameLoader> > mInitializableFrameLoaders;
  nsTArray<nsCOMPtr<nsIRunnable> > mFrameLoaderFinalizers;
  nsRefPtr<nsRunnableMethod<nsDocument> > mFrameLoaderRunner;

  nsRevocableEventPtr<nsRunnableMethod<nsDocument, void, false> >
    mPendingTitleChangeEvent;

  nsExternalResourceMap mExternalResourceMap;

  // All images in process of being preloaded.  This is a hashtable so
  // we can remove them as the real image loads start; that way we
  // make sure to not keep the image load going when no one cares
  // about it anymore.
  nsRefPtrHashtable<nsURIHashKey, imgIRequest> mPreloadingImages;

  // Current depth of picture elements from parser
  int32_t mPreloadPictureDepth;

  // Set if we've found a URL for the current picture
  nsString mPreloadPictureFoundSource;

  nsRefPtr<mozilla::dom::DOMImplementation> mDOMImplementation;

  nsRefPtr<nsContentList> mImageMaps;

  nsCString mScrollToRef;
  uint8_t mScrolledToRefAlready : 1;
  uint8_t mChangeScrollPosWhenScrollingToRef : 1;

  // Tracking for images in the document.
  nsDataHashtable< nsPtrHashKey<imgIRequest>, uint32_t> mImageTracker;

  // Tracking for plugins in the document.
  nsTHashtable< nsPtrHashKey<nsIObjectLoadingContent> > mPlugins;

  nsRefPtr<mozilla::dom::UndoManager> mUndoManager;

  nsRefPtr<mozilla::dom::DocumentTimeline> mDocumentTimeline;

  enum ViewportType {
    DisplayWidthHeight,
    DisplayWidthHeightNoZoom,
    Specified,
    Unknown
  };

  ViewportType mViewportType;

  // These member variables cache information about the viewport so we don't have to
  // recalculate it each time.
  bool mValidWidth, mValidHeight;
  mozilla::LayoutDeviceToScreenScale mScaleMinFloat;
  mozilla::LayoutDeviceToScreenScale mScaleMaxFloat;
  mozilla::LayoutDeviceToScreenScale mScaleFloat;
  mozilla::CSSToLayoutDeviceScale mPixelRatio;
  bool mAutoSize, mAllowZoom, mAllowDoubleTapZoom, mValidScaleFloat, mValidMaxScale, mScaleStrEmpty, mWidthStrEmpty;
  mozilla::CSSSize mViewportSize;

  nsrefcnt mStackRefCnt;
  bool mNeedsReleaseAfterStackRefCntRelease;

  CSPErrorQueue mCSPWebConsoleErrorQueue;

  nsCOMPtr<nsIDocument> mMasterDocument;
  nsRefPtr<mozilla::dom::ImportManager> mImportManager;
  nsTArray<nsCOMPtr<nsINode> > mSubImportLinks;

  // Set to true when the document is possibly controlled by the ServiceWorker.
  // Used to prevent multiple requests to ServiceWorkerManager.
  bool mMaybeServiceWorkerControlled;

#ifdef DEBUG
public:
  bool mWillReparent;
#endif
};

class nsDocumentOnStack
{
public:
  explicit nsDocumentOnStack(nsDocument* aDoc) : mDoc(aDoc)
  {
    mDoc->IncreaseStackRefCnt();
  }
  ~nsDocumentOnStack()
  {
    mDoc->DecreaseStackRefCnt();
  }
private:
  nsDocument* mDoc;
};

#endif /* nsDocument_h___ */
