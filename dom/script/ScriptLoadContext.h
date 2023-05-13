/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScriptLoadContext_h
#define mozilla_dom_ScriptLoadContext_h

#include "js/AllocPolicy.h"
#include "js/RootingAPI.h"
#include "js/SourceText.h"
#include "js/TypeDecls.h"
#include "js/loader/LoadContextBase.h"
#include "js/loader/ScriptKind.h"
#include "mozilla/Atomics.h"
#include "mozilla/Assertions.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/MaybeOneOf.h"
#include "mozilla/PreloaderBase.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIScriptElement.h"

class nsICacheInfoChannel;

namespace JS {
class OffThreadToken;
}  // namespace JS

namespace mozilla::dom {

class Element;

/*
 * DOM specific ScriptLoadContext.
 *
 * ScriptLoadContexts augment the loading of a ScriptLoadRequest. They
 * describe how a ScriptLoadRequests loading and evaluation needs to be
 * augmented, based on the information provided by the loading context. In
 * the case of the DOM, the ScriptLoadContext is used to identify how a script
 * should be loaded according to information found in the HTML document into
 * which it will be loaded. The following fields describe how the
 * ScriptLoadRequest will be loaded.
 *
 *    * mScriptMode
 *        stores the mode (Async, Sync, Deferred), and preload, which
 *        allows the ScriptLoader to decide if the script should be pushed
 *        offThread, or if the preloaded request should be used.
 *    * mScriptFromHead
 *        Set when the script tag is in the head, and should be treated as
 *        a blocking script
 *    * mIsInline
 *        Set for scripts whose bodies are inline in the html. In this case,
 *        the script does not need to be fetched first.
 *    * mIsXSLT
 *        Set if we are in an XSLT request.
 *    * mIsPreload
 *        Set for scripts that are preloaded in a
 *        <link rel="preload" as="script"> or <link rel="modulepreload">
 *        element.
 *
 * In addition to describing how the ScriptLoadRequest will be loaded by the
 * DOM ScriptLoader, the ScriptLoadContext contains fields that facilitate
 * those custom behaviors, including support for offthread parsing, pointers
 * to runnables (for cancellation and cleanup if a script is parsed offthread)
 * and preload element specific controls.
 *
 */

class ScriptLoadContext : public JS::loader::LoadContextBase,
                          public PreloaderBase {
 protected:
  virtual ~ScriptLoadContext();

 public:
  explicit ScriptLoadContext();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ScriptLoadContext,
                                           JS::loader::LoadContextBase)

  // PreloaderBase
  static void PrioritizeAsPreload(nsIChannel* aChannel);
  virtual void PrioritizeAsPreload() override;

  bool IsPreload() const;

  bool CompileStarted() const;

  JS::OffThreadToken** OffThreadTokenPtr() {
    return mOffThreadToken ? &mOffThreadToken : nullptr;
  }

  bool IsTracking() const { return mIsTracking; }
  void SetIsTracking() {
    MOZ_ASSERT(!mIsTracking);
    mIsTracking = true;
  }

  void BlockOnload(Document* aDocument);

  void MaybeUnblockOnload();

  enum class ScriptMode : uint8_t {
    eBlocking,
    eDeferred,
    eAsync,
    eLinkPreload  // this is a load initiated by <link rel="preload"
                  // as="script"> or <link rel="modulepreload"> tag
  };

  void SetScriptMode(bool aDeferAttr, bool aAsyncAttr, bool aLinkPreload);

  bool IsLinkPreloadScript() const {
    return mScriptMode == ScriptMode::eLinkPreload;
  }

  bool IsBlockingScript() const { return mScriptMode == ScriptMode::eBlocking; }

  bool IsDeferredScript() const { return mScriptMode == ScriptMode::eDeferred; }

  bool IsAsyncScript() const { return mScriptMode == ScriptMode::eAsync; }

  nsIScriptElement* GetScriptElement() const;

  // Make this request a preload (speculative) request.
  void SetIsPreloadRequest() {
    MOZ_ASSERT(!GetScriptElement());
    MOZ_ASSERT(!IsPreload());
    mIsPreload = true;
  }

  // Make a preload request into an actual load request for the given element.
  void SetIsLoadRequest(nsIScriptElement* aElement);

  FromParser GetParserCreated() const {
    nsIScriptElement* element = GetScriptElement();
    if (!element) {
      return NOT_FROM_PARSER;
    }
    return element->GetParserCreated();
  }

  // Used to output a string for the Gecko Profiler.
  void GetProfilerLabel(nsACString& aOutString) override;

  void MaybeCancelOffThreadScript();

  ScriptMode mScriptMode;  // Whether this is a blocking, defer or async script.
  bool mScriptFromHead;    // Synchronous head script block loading of other non
                           // js/css content.
  bool mIsInline;          // Is the script inline or loaded?
  bool mInDeferList;       // True if we live in mDeferRequests.
  bool mInAsyncList;       // True if we live in mLoadingAsyncRequests or
                           // mLoadedAsyncRequests.
  bool mIsNonAsyncScriptInserted;  // True if we live in
                                   // mNonAsyncExternalScriptInsertedRequests
  bool mIsXSLT;                    // True if we live in mXSLTRequests.
  bool mInCompilingList;  // True if we are in mOffThreadCompilingRequests.
  bool mIsTracking;       // True if the script comes from a source on our
                          // tracking protection list.
  bool mWasCompiledOMT;   // True if the script has been compiled off main
                          // thread.

  // Off-thread parsing token. Set at the start of off-thread parsing and
  // cleared when the result of the parse is used.
  JS::OffThreadToken* mOffThreadToken;

  // Runnable that is dispatched to the main thread when off-thread compilation
  // completes.
  RefPtr<Runnable> mRunnable;

  uint32_t mLineNo;
  uint32_t mColumnNo;

  // Set on scripts and top level modules.
  bool mIsPreload;

  // Non-null if there is a document that this request is blocking from loading.
  RefPtr<Document> mLoadBlockedDocument;

  // For preload requests, we defer reporting errors to the console until the
  // request is used.
  nsresult mUnreportedPreloadError;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ScriptLoadContext_h
