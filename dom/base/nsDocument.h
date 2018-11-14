/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all our document implementations.
 */

#ifndef nsDocument_h___
#define nsDocument_h___

#include "nsIDocument.h"

#include "jsfriendapi.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCRT.h"
#include "nsIWeakReferenceUtils.h"
#include "nsTArray.h"
#include "nsIdentifierMapEntry.h"
#include "nsStubDocumentObserver.h"
#include "nsIScriptGlobalObject.h"
#include "nsIContent.h"
#include "nsIPrincipal.h"
#include "nsIParser.h"
#include "nsBindingManager.h"
#include "nsRefPtrHashtable.h"
#include "nsJSThingHashtable.h"
#include "nsIScriptObjectPrincipal.h"
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
#include "PLDHashTable.h"
#include "nsDOMAttributeMap.h"
#include "imgIRequest.h"
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/dom/BoxObject.h"
#include "mozilla/dom/DOMImplementation.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/StyleSheetList.h"
#include "nsDataHashtable.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Attributes.h"
#include "CustomElementRegistry.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/Maybe.h"
#include "nsIURIClassifier.h"

#define XML_DECLARATION_BITS_DECLARATION_EXISTS   (1 << 0)
#define XML_DECLARATION_BITS_ENCODING_EXISTS      (1 << 1)
#define XML_DECLARATION_BITS_STANDALONE_EXISTS    (1 << 2)
#define XML_DECLARATION_BITS_STANDALONE_YES       (1 << 3)


class nsDOMStyleSheetSetList;
class nsDocument;
class nsIFormControl;
class nsOnloadBlocker;
class nsDOMNavigationTiming;
class nsWindowSizes;
class nsHtml5TreeOpExecutor;
class nsDocumentOnStack;
class nsISecurityConsoleMessage;

namespace mozilla {
class EventChainPreVisitor;
namespace dom {
class ImageTracker;
struct LifecycleCallbacks;
class CallbackFunction;
class DOMIntersectionObserver;
class Performance;
} // namespace dom
} // namespace mozilla

class nsOnloadBlocker final : public nsIRequest
{
public:
  nsOnloadBlocker() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST

private:
  ~nsOnloadBlocker() {}
};

// Base class for our document implementations.
class nsDocument : public nsIDocument,
                   public nsSupportsWeakReference,
                   public nsIScriptObjectPrincipal,
                   public nsIRadioGroupContainer,
                   public nsIApplicationCacheContainer,
                   public nsStubMutationObserver
{
  friend class nsIDocument;

public:
  typedef mozilla::dom::Element Element;
  typedef mozilla::net::ReferrerPolicy ReferrerPolicy;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

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

  static bool DocumentSupportsL10n(JSContext* aCx, JSObject* aObject);
  static bool IsWebAnimationsEnabled(JSContext* aCx, JSObject* aObject);
  static bool IsWebAnimationsEnabled(mozilla::dom::CallerType aCallerType);
  static bool IsWebAnimationsGetAnimationsEnabled(JSContext* aCx,
                                                  JSObject* aObject);
  static bool AreWebAnimationsImplicitKeyframesEnabled(JSContext* aCx,
                                                       JSObject* aObject);
  static bool AreWebAnimationsTimelinesEnabled(JSContext* aCx,
                                               JSObject* aObject);

  virtual void EndUpdate() override;
  virtual void BeginLoad() override;
  virtual void EndLoad() override;

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName,
                            nsIRadioVisitor* aVisitor,
                            bool aFlushContent) override
  {
    return DocumentOrShadowRoot::WalkRadioGroup(aName, aVisitor, aFlushContent);
  }
  virtual void
    SetCurrentRadioButton(const nsAString& aName,
                          mozilla::dom::HTMLInputElement* aRadio) override
  {
    DocumentOrShadowRoot::SetCurrentRadioButton(aName, aRadio);
  }
  virtual mozilla::dom::HTMLInputElement*
    GetCurrentRadioButton(const nsAString& aName) override
  {
    return DocumentOrShadowRoot::GetCurrentRadioButton(aName);
  }
  NS_IMETHOD
    GetNextRadioButton(const nsAString& aName,
                       const bool aPrevious,
                       mozilla::dom::HTMLInputElement* aFocusedRadio,
                       mozilla::dom::HTMLInputElement** aRadioOut) override
  {
    return DocumentOrShadowRoot::GetNextRadioButton(aName, aPrevious,
                                                    aFocusedRadio, aRadioOut);
  }
  virtual void AddToRadioGroup(const nsAString& aName,
                               mozilla::dom::HTMLInputElement* aRadio) override
  {
    DocumentOrShadowRoot::AddToRadioGroup(aName, aRadio);
  }
  virtual void RemoveFromRadioGroup(const nsAString& aName,
                                    mozilla::dom::HTMLInputElement* aRadio) override
  {
    DocumentOrShadowRoot::RemoveFromRadioGroup(aName, aRadio);
  }
  virtual uint32_t GetRequiredRadioCount(const nsAString& aName) const override
  {
    return DocumentOrShadowRoot::GetRequiredRadioCount(aName);
  }
  virtual void RadioRequiredWillChange(const nsAString& aName,
                                       bool aRequiredAdded) override
  {
    DocumentOrShadowRoot::RadioRequiredWillChange(aName, aRequiredAdded);
  }
  virtual bool GetValueMissingState(const nsAString& aName) const override
  {
    return DocumentOrShadowRoot::GetValueMissingState(aName);
  }
  virtual void SetValueMissingState(const nsAString& aName, bool aValue) override
  {
    return DocumentOrShadowRoot::SetValueMissingState(aName, aValue);
  }

  // Check whether shadow DOM is enabled for aGlobal.
  static bool IsShadowDOMEnabled(JSContext* aCx, JSObject* aGlobal);
  // Check whether shadow DOM is enabled for the document this node belongs to.
  // Same as above, but also checks that the caller is either chrome or some addon.
  static bool IsShadowDOMEnabledAndCallerIsChromeOrAddon(JSContext* aCx, JSObject* aObject);
  static bool IsShadowDOMEnabled(const nsINode* aNode);

public:
  using mozilla::dom::DocumentOrShadowRoot::GetElementById;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByTagName;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByTagNameNS;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByClassName;

  // EventTarget
  void GetEventTargetParent(mozilla::EventChainPreVisitor& aVisitor) override;
  virtual mozilla::EventListenerManager*
    GetOrCreateListenerManager() override;
  virtual mozilla::EventListenerManager*
    GetExistingListenerManager() const override;

  // nsIScriptObjectPrincipal
  virtual nsIPrincipal* GetPrincipal() override;

  // nsIApplicationCacheContainer
  NS_DECL_NSIAPPLICATIONCACHECONTAINER

  virtual nsresult Init();

  virtual void Destroy() override;
  virtual void RemovedFromDocShell() override;

  virtual void BlockOnload() override;
  virtual void UnblockOnload(bool aFireSync) override;

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDocument,
                                                                   nsIDocument)

  void SetLoadedAsData(bool aLoadedAsData) { mLoadedAsData = aLoadedAsData; }
  void SetLoadedAsInteractiveData(bool aLoadedAsInteractiveData)
  {
    mLoadedAsInteractiveData = aLoadedAsInteractiveData;
  }

  nsresult CloneDocHelper(nsDocument* clone) const;

  // Only BlockOnload should call this!
  void AsyncBlockOnload();

  virtual void DocAddSizeOfExcludingThis(nsWindowSizes& aWindowSizes) const override;
  // DocAddSizeOfIncludingThis is inherited from nsIDocument.

protected:
  friend class nsNodeUtils;

  void RetrieveRelevantHeaders(nsIChannel *aChannel);

  void TryChannelCharset(nsIChannel *aChannel,
                         int32_t& aCharsetSource,
                         NotNull<const Encoding*>& aEncoding,
                         nsHtml5TreeOpExecutor* aExecutor);

  nsIContent* GetFirstBaseNodeWithHref();
  nsresult SetFirstBaseNodeWithHref(nsIContent *node);

#define NS_DOCUMENT_NOTIFY_OBSERVERS(func_, params_) do {                     \
    NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers, nsIDocumentObserver, \
                                             func_, params_);                 \
    /* FIXME(emilio): Apparently we can keep observing from the BFCache? That \
       looks bogus. */                                                        \
    if (nsIPresShell* shell = GetObservingShell()) {                          \
      shell->func_ params_;                                                   \
    }                                                                         \
  } while(0)

#ifdef DEBUG
  void VerifyRootContentState();
#endif

  explicit nsDocument(const char* aContentType);
  virtual ~nsDocument();

public:
  // FIXME(emilio): This needs to be here instead of in nsIDocument because Rust
  // can't represent alignas(8) values on 32-bit architectures, which would
  // cause nsIDocument's layout to be wrong in the Rust side.
  //
  // This can be fixed after updating to rust 1.25 and updating bindgen to
  // include https://github.com/rust-lang-nursery/rust-bindgen/pull/1271.
  js::ExpandoAndGeneration mExpandoAndGeneration;

  friend class nsCallRequestFullscreen;

  // The application cache that this document is associated with, if
  // any.  This can change during the lifetime of the document.
  nsCOMPtr<nsIApplicationCache> mApplicationCache;

  nsCOMPtr<nsIContent> mFirstBaseNodeWithHref;
private:
  friend class nsUnblockOnloadEvent;

  // These are not implemented and not supported.
  nsDocument(const nsDocument& aOther);
  nsDocument& operator=(const nsDocument& aOther);
};

class nsDocumentOnStack
{
public:
  explicit nsDocumentOnStack(nsIDocument* aDoc) : mDoc(aDoc)
  {
    mDoc->IncreaseStackRefCnt();
  }
  ~nsDocumentOnStack()
  {
    mDoc->DecreaseStackRefCnt();
  }
private:
  nsIDocument* mDoc;
};

#endif /* nsDocument_h___ */
