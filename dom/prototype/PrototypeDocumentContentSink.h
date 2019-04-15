/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PrototypeDocumentContentSink_h__
#define mozilla_dom_PrototypeDocumentContentSink_h__

#include "mozilla/Attributes.h"
#include "nsIContentSink.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDTD.h"
#include "mozilla/dom/FromParser.h"
#include "nsXULPrototypeDocument.h"
#include "nsIStreamLoader.h"
#include "nsIScriptContext.h"
#include "nsICSSLoaderObserver.h"
#include "mozilla/Logging.h"

class nsIURI;
class nsIChannel;
class nsIContent;
class nsIParser;
class nsTextNode;
class nsINode;
class nsXULPrototypeElement;
class nsXULPrototypePI;
class nsXULPrototypeScript;

namespace mozilla {
namespace dom {
class Element;
class ScriptLoader;
class Document;
}  // namespace dom
}  // namespace mozilla

nsresult NS_NewPrototypeDocumentContentSink(nsIContentSink** aResult,
                                            mozilla::dom::Document* aDoc,
                                            nsIURI* aURI,
                                            nsISupports* aContainer,
                                            nsIChannel* aChannel);

namespace mozilla {
namespace dom {

class PrototypeDocumentContentSink final : public nsIStreamLoaderObserver,
                                           public nsIContentSink,
                                           public nsICSSLoaderObserver,
                                           public nsIOffThreadScriptReceiver {
 public:
  PrototypeDocumentContentSink();

  nsresult Init(Document* aDoc, nsIURI* aURL, nsISupports* aContainer,
                nsIChannel* aChannel);

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(PrototypeDocumentContentSink,
                                           nsIContentSink)

  // nsIContentSink
  NS_IMETHOD WillParse(void) override { return NS_OK; };
  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) override { return NS_OK; };
  NS_IMETHOD DidBuildModel(bool aTerminated) override { return NS_OK; };
  NS_IMETHOD WillInterrupt(void) override { return NS_OK; };
  NS_IMETHOD WillResume(void) override { return NS_OK; };
  NS_IMETHOD SetParser(nsParserBase* aParser) override;
  virtual void InitialDocumentTranslationCompleted() override;
  virtual void FlushPendingNotifications(FlushType aType) override{};
  virtual void SetDocumentCharset(NotNull<const Encoding*> aEncoding) override;
  virtual nsISupports* GetTarget() override;
  virtual bool IsScriptExecuting() override;
  virtual void ContinueInterruptedParsingAsync() override;

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet, bool aWasAlternate,
                              nsresult aStatus) override;

  // nsIOffThreadScriptReceiver
  NS_IMETHOD OnScriptCompileComplete(JSScript* aScript,
                                     nsresult aStatus) override;

  nsresult OnPrototypeLoadDone(nsXULPrototypeDocument* aPrototype);

 protected:
  virtual ~PrototypeDocumentContentSink();

  static LazyLogModule gLog;

  nsIParser* GetParser();

  void ContinueInterruptedParsingIfEnabled();
  void StartLayout();

  virtual nsresult AddAttributes(nsXULPrototypeElement* aPrototype,
                                 Element* aElement);

  RefPtr<nsParserBase> mParser;
  nsCOMPtr<nsIURI> mDocumentURI;
  RefPtr<Document> mDocument;
  RefPtr<nsNodeInfoManager> mNodeInfoManager;
  RefPtr<ScriptLoader> mScriptLoader;

  PrototypeDocumentContentSink* mNextSrcLoadWaiter;  // [OWNER] but not COMPtr

  /**
   * The prototype-script of the current transcluded script that is being
   * loaded.  For document.write('<script src="nestedwrite.js"><\/script>')
   * to work, these need to be in a stack element type, and we need to hold
   * the top of stack here.
   */
  nsXULPrototypeScript* mCurrentScriptProto;

  /**
   * Whether the current transcluded script is being compiled off thread.
   * The load event is blocked while this is in progress.
   */
  bool mOffThreadCompiling;

  /**
   * If the current transcluded script is being compiled off thread, the
   * source for that script.
   */
  char16_t* mOffThreadCompileStringBuf;
  size_t mOffThreadCompileStringLength;

  /**
   * Wether the prototype document is still be traversed to create the DOM.
   * Layout will not be started until false.
   */
  bool mStillWalking;

  /**
   * Number of style sheets still loading. Layout will not start until zero.
   */
  uint32_t mPendingSheets;

  /**
   * Context stack, which maintains the state of the Builder and allows
   * it to be interrupted.
   */
  class ContextStack {
   protected:
    struct Entry {
      nsXULPrototypeElement* mPrototype;
      nsIContent* mElement;
      int32_t mIndex;
      Entry* mNext;
    };

    Entry* mTop;
    int32_t mDepth;

   public:
    ContextStack();
    ~ContextStack();

    int32_t Depth() { return mDepth; }

    nsresult Push(nsXULPrototypeElement* aPrototype, nsIContent* aElement);
    nsresult Pop();
    nsresult Peek(nsXULPrototypeElement** aPrototype, nsIContent** aElement,
                  int32_t* aIndex);

    nsresult SetTopIndex(int32_t aIndex);
  };

  friend class ContextStack;
  ContextStack mContextStack;

  /**
   * The current prototype that we are walking to construct the
   * content model.
   */
  RefPtr<nsXULPrototypeDocument> mCurrentPrototype;
  nsresult CreateAndInsertPI(const nsXULPrototypePI* aProtoPI);
  nsresult ExecuteScript(nsXULPrototypeScript* aScript);
  nsresult LoadScript(nsXULPrototypeScript* aScriptProto, bool* aBlock);

  /**
   * A wrapper around ResumeWalkInternal to report walking errors.
   */
  nsresult ResumeWalk();

  /**
   * Resume (or initiate) an interrupted (or newly prepared)
   * prototype walk.
   */
  nsresult ResumeWalkInternal();

  /**
   * Called at the end of ResumeWalk(), from StyleSheetLoaded(),
   * and from DocumentL10n.
   * If walking, stylesheets and l10n are not blocking, it
   * will trigger `DoneWalking()`.
   */
  nsresult MaybeDoneWalking();

  /**
   * Called from `MaybeDoneWalking()`.
   * Expects that both the prototype document walk is complete and
   * all referenced stylesheets finished loading.
   */
  nsresult DoneWalking();

  /**
   * Create a delegate content model element from a prototype.
   * Note that the resulting content node is not bound to any tree
   */
  nsresult CreateElementFromPrototype(nsXULPrototypeElement* aPrototype,
                                      Element** aResult, bool aIsRoot);
  /**
   * Prepare to walk the current prototype.
   */
  nsresult PrepareToWalk();
  /**
   * Creates a processing instruction based on aProtoPI and inserts
   * it to the DOM.
   */
  nsresult CreateAndInsertPI(const nsXULPrototypePI* aProtoPI, nsINode* aParent,
                             nsINode* aBeforeThis);

  /**
   * Inserts the passed <?xml-stylesheet ?> PI at the specified
   * index. Loads and applies the associated stylesheet
   * asynchronously.
   * The prototype document walk can happen before the stylesheets
   * are loaded, but the final steps in the load process (see
   * DoneWalking()) are not run before all the stylesheets are done
   * loading.
   */
  nsresult InsertXMLStylesheetPI(const nsXULPrototypePI* aProtoPI,
                                 nsINode* aParent, nsINode* aBeforeThis,
                                 nsIContent* aPINode);
  void CloseElement(Element* aElement);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PrototypeDocumentContentSink_h__
