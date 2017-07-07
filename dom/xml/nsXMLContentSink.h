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

class nsIDocument;
class nsIURI;
class nsIContent;
class nsIParser;

namespace mozilla {
namespace dom {
class NodeInfo;
} // namespace dom
} // namespace mozilla

typedef enum {
  eXMLContentSinkState_InProlog,
  eXMLContentSinkState_InDocumentElement,
  eXMLContentSinkState_InEpilog
} XMLContentSinkState;

struct StackNode {
  nsCOMPtr<nsIContent> mContent;
  uint32_t mNumFlushed;
};

class nsXMLContentSink : public nsContentSink,
                         public nsIXMLContentSink,
                         public nsITransformObserver,
                         public nsIExpatSink
{
public:
  nsXMLContentSink();

  nsresult Init(nsIDocument* aDoc,
                nsIURI* aURL,
                nsISupports* aContainer,
                nsIChannel* aChannel);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsXMLContentSink,
                                                     nsContentSink)

  NS_DECL_NSIEXPATSINK

  // nsIContentSink
  NS_IMETHOD WillParse(void) override;
  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) override;
  NS_IMETHOD DidBuildModel(bool aTerminated) override;
  NS_IMETHOD WillInterrupt(void) override;
  NS_IMETHOD WillResume(void) override;
  NS_IMETHOD SetParser(nsParserBase* aParser) override;
  virtual void FlushPendingNotifications(mozilla::FlushType aType) override;
  virtual void SetDocumentCharset(NotNull<const Encoding*> aEncoding) override;
  virtual nsISupports *GetTarget() override;
  virtual bool IsScriptExecuting() override;
  virtual void ContinueInterruptedParsingAsync() override;

  // nsITransformObserver
  NS_IMETHOD OnDocumentCreated(nsIDocument *aResultDocument) override;
  NS_IMETHOD OnTransformDone(nsresult aResult, nsIDocument *aResultDocument) override;

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(mozilla::StyleSheet* aSheet,
                              bool aWasAlternate,
                              nsresult aStatus) override;
  static bool ParsePIData(const nsString &aData, nsString &aHref,
                          nsString &aTitle, nsString &aMedia,
                          bool &aIsAlternate);

protected:
  virtual ~nsXMLContentSink();

  nsIParser* GetParser();

  void ContinueInterruptedParsingIfEnabled();

  // Start layout.  If aIgnorePendingSheets is true, this will happen even if
  // we still have stylesheet loads pending.  Otherwise, we'll wait until the
  // stylesheets are all done loading.
  virtual void MaybeStartLayout(bool aIgnorePendingSheets);

  virtual nsresult AddAttributes(const char16_t** aNode, nsIContent* aContent);
  nsresult AddText(const char16_t* aString, int32_t aLength);

  virtual bool OnOpenContainer(const char16_t **aAtts,
                                 uint32_t aAttsCount,
                                 int32_t aNameSpaceID,
                                 nsIAtom* aTagName,
                                 uint32_t aLineNumber) { return true; }
  // Set the given content as the root element for the created document
  //  don't set if root element was already set.
  //  return TRUE if this call set the root element
  virtual bool SetDocElement(int32_t aNameSpaceID,
                               nsIAtom *aTagName,
                               nsIContent *aContent);
  virtual bool NotifyForDocElement() { return true; }
  virtual nsresult CreateElement(const char16_t** aAtts, uint32_t aAttsCount,
                                 mozilla::dom::NodeInfo* aNodeInfo, uint32_t aLineNumber,
                                 nsIContent** aResult, bool* aAppendContent,
                                 mozilla::dom::FromParser aFromParser);

  // aParent is allowed to be null here if this is the root content
  // being closed
  virtual nsresult CloseElement(nsIContent* aContent);

  virtual nsresult FlushText(bool aReleaseTextNode = true);

  nsresult AddContentAsLeaf(nsIContent *aContent);

  nsIContent* GetCurrentContent();
  StackNode* GetCurrentStackNode();
  nsresult PushContent(nsIContent *aContent);
  void PopContent();
  bool HaveNotifiedForCurrentContent() const;

  nsresult FlushTags() override;

  void UpdateChildCounts() override;

  void DidAddContent()
  {
    if (IsTimeToNotify()) {
      FlushTags();
    }
  }

  // nsContentSink override
  virtual nsresult ProcessStyleLink(nsIContent* aElement,
                                    const nsAString& aHref,
                                    bool aAlternate,
                                    const nsAString& aTitle,
                                    const nsAString& aType,
                                    const nsAString& aMedia) override;

  nsresult LoadXSLStyleSheet(nsIURI* aUrl);

  bool CanStillPrettyPrint();

  nsresult MaybePrettyPrint();

  bool IsMonolithicContainer(mozilla::dom::NodeInfo* aNodeInfo);

  nsresult HandleStartElement(const char16_t *aName, const char16_t **aAtts,
                              uint32_t aAttsCount, uint32_t aLineNumber,
                              bool aInterruptable);
  nsresult HandleEndElement(const char16_t *aName, bool aInterruptable);
  nsresult HandleCharacterData(const char16_t *aData, uint32_t aLength,
                               bool aInterruptable);

  nsCOMPtr<nsIContent> mDocElement;
  nsCOMPtr<nsIContent> mCurrentHead;  // When set, we're in an XHTML <haed>

  XMLContentSinkState mState;

  // The length of the valid data in mText.
  int32_t mTextLength;

  int32_t mNotifyLevel;
  nsCOMPtr<nsIContent> mLastTextNode;

  uint8_t mPrettyPrintXML : 1;
  uint8_t mPrettyPrintHasSpecialRoot : 1;
  uint8_t mPrettyPrintHasFactoredElements : 1;
  uint8_t mPrettyPrinting : 1;  // True if we called PrettyPrint() and it
                                // decided we should in fact prettyprint.
  // True to call prevent script execution in the fragment mode.
  uint8_t mPreventScriptExecution : 1;

  nsTArray<StackNode>              mContentStack;

  nsCOMPtr<nsIDocumentTransformer> mXSLTProcessor;

  static const int NS_ACCUMULATION_BUFFER_SIZE = 4096;
  // Our currently accumulated text that we have not flushed to a textnode yet.
  char16_t mText[NS_ACCUMULATION_BUFFER_SIZE];
};

#endif // nsXMLContentSink_h__
