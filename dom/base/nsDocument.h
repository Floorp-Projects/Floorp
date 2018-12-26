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

#define XML_DECLARATION_BITS_DECLARATION_EXISTS (1 << 0)
#define XML_DECLARATION_BITS_ENCODING_EXISTS (1 << 1)
#define XML_DECLARATION_BITS_STANDALONE_EXISTS (1 << 2)
#define XML_DECLARATION_BITS_STANDALONE_YES (1 << 3)

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
}  // namespace dom
}  // namespace mozilla

class nsOnloadBlocker final : public nsIRequest {
 public:
  nsOnloadBlocker() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST

 private:
  ~nsOnloadBlocker() {}
};

// Base class for our document implementations.
class nsDocument : public nsIDocument {
  friend class nsIDocument;

 public:
  typedef mozilla::dom::Element Element;
  typedef mozilla::net::ReferrerPolicy ReferrerPolicy;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

 public:
  using mozilla::dom::DocumentOrShadowRoot::GetElementById;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByClassName;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByTagName;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByTagNameNS;

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDocument,
                                                                   nsINode)

 protected:
  friend class nsNodeUtils;

  explicit nsDocument(const char* aContentType);
  virtual ~nsDocument();

 public:

  friend class nsCallRequestFullscreen;

 private:
  friend class nsUnblockOnloadEvent;

  // These are not implemented and not supported.
  nsDocument(const nsDocument& aOther);
  nsDocument& operator=(const nsDocument& aOther);
};

class nsDocumentOnStack {
 public:
  explicit nsDocumentOnStack(nsIDocument* aDoc) : mDoc(aDoc) {
    mDoc->IncreaseStackRefCnt();
  }
  ~nsDocumentOnStack() { mDoc->DecreaseStackRefCnt(); }

 private:
  nsIDocument* mDoc;
};

#endif /* nsDocument_h___ */
