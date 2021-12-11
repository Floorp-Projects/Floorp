/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLContentSink_h__
#define nsXMLContentSink_h__

#include "mozilla/Attributes.h"
#include "nsContentSink.h"
#include "nsIXMLContentSink.h"
#include "nsIExpatSink.h"
#include "nsIDocumentTransformer.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDTD.h"
#include "mozilla/dom/FromParser.h"

class nsIURI;
class nsIContent;
class nsIParser;
class nsTextNode;

namespace mozilla {
namespace dom {
class NodeInfo;
class ProcessingInstruction;
}  // namespace dom
}  // namespace mozilla

enum XMLContentSinkState {
  eXMLContentSinkState_InProlog,
  eXMLContentSinkState_InDocumentElement,
  eXMLContentSinkState_InEpilog
};

class nsXMLContentSink : public nsContentSink,
                         public nsIXMLContentSink,
                         public nsITransformObserver,
                         public nsIExpatSink {
 public:
  struct StackNode {
    nsCOMPtr<nsIContent> mContent;
    uint32_t mNumFlushed;
  };

  nsXMLContentSink();

  nsresult Init(mozilla::dom::Document* aDoc, nsIURI* aURL,
                nsISupports* aContainer, nsIChannel* aChannel);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXMLContentSink, nsContentSink)

  NS_DECL_NSIEXPATSINK

  // nsIContentSink
  NS_IMETHOD WillParse(void) override;
  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) override;
  NS_IMETHOD DidBuildModel(bool aTerminated) override;
  NS_IMETHOD WillInterrupt(void) override;
  NS_IMETHOD WillResume(void) override;
  NS_IMETHOD SetParser(nsParserBase* aParser) override;
  virtual void InitialTranslationCompleted() override;
  virtual void FlushPendingNotifications(mozilla::FlushType aType) override;
  virtual void SetDocumentCharset(NotNull<const Encoding*> aEncoding) override;
  virtual nsISupports* GetTarget() override;
  virtual bool IsScriptExecuting() override;
  virtual void ContinueInterruptedParsingAsync() override;
  bool IsPrettyPrintXML() const override { return mPrettyPrintXML; }
  bool IsPrettyPrintHasSpecialRoot() const override {
    return mPrettyPrintHasSpecialRoot;
  }

  // nsITransformObserver
  NS_IMETHOD OnDocumentCreated(
      mozilla::dom::Document* aResultDocument) override;
  NS_IMETHOD OnTransformDone(nsresult aResult,
                             mozilla::dom::Document* aResultDocument) override;

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(mozilla::StyleSheet* aSheet, bool aWasDeferred,
                              nsresult aStatus) override;
  static bool ParsePIData(const nsString& aData, nsString& aHref,
                          nsString& aTitle, nsString& aMedia,
                          bool& aIsAlternate);

 protected:
  virtual ~nsXMLContentSink();

  nsIParser* GetParser();

  void ContinueInterruptedParsingIfEnabled();

  // Start layout.  If aIgnorePendingSheets is true, this will happen even if
  // we still have stylesheet loads pending.  Otherwise, we'll wait until the
  // stylesheets are all done loading.
  virtual void MaybeStartLayout(bool aIgnorePendingSheets);

  virtual nsresult AddAttributes(const char16_t** aNode,
                                 mozilla::dom::Element* aElement);
  nsresult AddText(const char16_t* aString, int32_t aLength);

  virtual bool OnOpenContainer(const char16_t** aAtts, uint32_t aAttsCount,
                               int32_t aNameSpaceID, nsAtom* aTagName,
                               uint32_t aLineNumber) {
    return true;
  }
  // Set the given content as the root element for the created document
  //  don't set if root element was already set.
  //  return TRUE if this call set the root element
  virtual bool SetDocElement(int32_t aNameSpaceID, nsAtom* aTagName,
                             nsIContent* aContent);
  virtual bool NotifyForDocElement() { return true; }
  virtual nsresult CreateElement(const char16_t** aAtts, uint32_t aAttsCount,
                                 mozilla::dom::NodeInfo* aNodeInfo,
                                 uint32_t aLineNumber, uint32_t aColumnNumber,
                                 nsIContent** aResult, bool* aAppendContent,
                                 mozilla::dom::FromParser aFromParser);

  // aParent is allowed to be null here if this is the root content
  // being closed
  virtual nsresult CloseElement(nsIContent* aContent);

  virtual nsresult FlushText(bool aReleaseTextNode = true);

  nsresult AddContentAsLeaf(nsIContent* aContent);

  nsIContent* GetCurrentContent();
  StackNode* GetCurrentStackNode();
  nsresult PushContent(nsIContent* aContent);
  void PopContent();
  bool HaveNotifiedForCurrentContent() const;

  nsresult FlushTags() override;

  void UpdateChildCounts() override;

  void DidAddContent() {
    if (!mXSLTProcessor && IsTimeToNotify()) {
      FlushTags();
    }
  }

  // nsContentSink override
  virtual nsresult ProcessStyleLinkFromHeader(
      const nsAString& aHref, bool aAlternate, const nsAString& aTitle,
      const nsAString& aIntegrity, const nsAString& aType,
      const nsAString& aMedia, const nsAString& aReferrerPolicy) override;

  // Try to handle an XSLT style link.  If NS_OK is returned and aWasXSLT is not
  // null, *aWasXSLT will be set to whether we processed this link as XSLT.
  //
  // aProcessingInstruction can be null if this information comes from a Link
  // header; otherwise it will be the xml-styleshset XML PI that the loading
  // information comes from.
  virtual nsresult MaybeProcessXSLTLink(
      mozilla::dom::ProcessingInstruction* aProcessingInstruction,
      const nsAString& aHref, bool aAlternate, const nsAString& aTitle,
      const nsAString& aType, const nsAString& aMedia,
      const nsAString& aReferrerPolicy, bool* aWasXSLT = nullptr);

  nsresult LoadXSLStyleSheet(nsIURI* aUrl);

  bool CanStillPrettyPrint();

  nsresult MaybePrettyPrint();

  bool IsMonolithicContainer(mozilla::dom::NodeInfo* aNodeInfo);

  nsresult HandleStartElement(const char16_t* aName, const char16_t** aAtts,
                              uint32_t aAttsCount, uint32_t aLineNumber,
                              uint32_t aColumnNumber, bool aInterruptable);
  nsresult HandleEndElement(const char16_t* aName, bool aInterruptable);
  nsresult HandleCharacterData(const char16_t* aData, uint32_t aLength,
                               bool aInterruptable);

  nsCOMPtr<nsIContent> mDocElement;
  nsCOMPtr<nsIContent> mCurrentHead;  // When set, we're in an XHTML <haed>

  XMLContentSinkState mState;

  // The length of the valid data in mText.
  int32_t mTextLength;

  int32_t mNotifyLevel;
  RefPtr<nsTextNode> mLastTextNode;

  uint8_t mPrettyPrintXML : 1;
  uint8_t mPrettyPrintHasSpecialRoot : 1;
  uint8_t mPrettyPrintHasFactoredElements : 1;
  uint8_t mPrettyPrinting : 1;  // True if we called PrettyPrint() and it
                                // decided we should in fact prettyprint.
  // True to call prevent script execution in the fragment mode.
  uint8_t mPreventScriptExecution : 1;

  nsTArray<StackNode> mContentStack;

  nsCOMPtr<nsIDocumentTransformer> mXSLTProcessor;

  // Holds the children in the prolog until the root element is added, after
  // which they're inserted in the document. However, if we're doing an XSLT
  // transform this will actually hold all the children of the source document,
  // until the transform is finished. After the transform is finished we'll just
  // discard the children.
  nsTArray<nsCOMPtr<nsIContent>> mDocumentChildren;

  static const int NS_ACCUMULATION_BUFFER_SIZE = 4096;
  // Our currently accumulated text that we have not flushed to a textnode yet.
  char16_t mText[NS_ACCUMULATION_BUFFER_SIZE];
};

#endif  // nsXMLContentSink_h__
