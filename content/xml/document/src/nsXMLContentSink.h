/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
class nsINodeInfo;
class nsIParser;
class nsViewManager;

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
  virtual ~nsXMLContentSink();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

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
  NS_IMETHOD WillParse(void) MOZ_OVERRIDE;
  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) MOZ_OVERRIDE;
  NS_IMETHOD DidBuildModel(bool aTerminated) MOZ_OVERRIDE;
  NS_IMETHOD WillInterrupt(void) MOZ_OVERRIDE;
  NS_IMETHOD WillResume(void) MOZ_OVERRIDE;
  NS_IMETHOD SetParser(nsParserBase* aParser) MOZ_OVERRIDE;
  virtual void FlushPendingNotifications(mozFlushType aType) MOZ_OVERRIDE;
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset) MOZ_OVERRIDE;
  virtual nsISupports *GetTarget() MOZ_OVERRIDE;
  virtual bool IsScriptExecuting() MOZ_OVERRIDE;
  virtual void ContinueInterruptedParsingAsync() MOZ_OVERRIDE;

  // nsITransformObserver
  NS_IMETHOD OnDocumentCreated(nsIDocument *aResultDocument) MOZ_OVERRIDE;
  NS_IMETHOD OnTransformDone(nsresult aResult, nsIDocument *aResultDocument) MOZ_OVERRIDE;

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsCSSStyleSheet* aSheet, bool aWasAlternate,
                              nsresult aStatus) MOZ_OVERRIDE;
  static bool ParsePIData(const nsString &aData, nsString &aHref,
                          nsString &aTitle, nsString &aMedia,
                          bool &aIsAlternate);

protected:

  nsIParser* GetParser();

  void ContinueInterruptedParsingIfEnabled();

  // Start layout.  If aIgnorePendingSheets is true, this will happen even if
  // we still have stylesheet loads pending.  Otherwise, we'll wait until the
  // stylesheets are all done loading.
  virtual void MaybeStartLayout(bool aIgnorePendingSheets);

  virtual nsresult AddAttributes(const PRUnichar** aNode, nsIContent* aContent);
  nsresult AddText(const PRUnichar* aString, int32_t aLength);

  virtual bool OnOpenContainer(const PRUnichar **aAtts, 
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
  virtual nsresult CreateElement(const PRUnichar** aAtts, uint32_t aAttsCount,
                                 nsINodeInfo* aNodeInfo, uint32_t aLineNumber,
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

  nsresult FlushTags() MOZ_OVERRIDE;

  void UpdateChildCounts() MOZ_OVERRIDE;

  void DidAddContent()
  {
    if (IsTimeToNotify()) {
      FlushTags();	
    }
  }
  
  // nsContentSink override
  virtual nsresult ProcessStyleLink(nsIContent* aElement,
                                    const nsSubstring& aHref,
                                    bool aAlternate,
                                    const nsSubstring& aTitle,
                                    const nsSubstring& aType,
                                    const nsSubstring& aMedia) MOZ_OVERRIDE;

  nsresult LoadXSLStyleSheet(nsIURI* aUrl);

  bool CanStillPrettyPrint();

  nsresult MaybePrettyPrint();
  
  bool IsMonolithicContainer(nsINodeInfo* aNodeInfo);

  nsresult HandleStartElement(const PRUnichar *aName, const PRUnichar **aAtts, 
                              uint32_t aAttsCount, int32_t aIndex, 
                              uint32_t aLineNumber,
                              bool aInterruptable);
  nsresult HandleEndElement(const PRUnichar *aName, bool aInterruptable);
  nsresult HandleCharacterData(const PRUnichar *aData, uint32_t aLength,
                               bool aInterruptable);

  nsCOMPtr<nsIContent> mDocElement;
  nsCOMPtr<nsIContent> mCurrentHead;  // When set, we're in an XHTML <haed>
  PRUnichar*       mText;

  XMLContentSinkState mState;

  int32_t mTextLength;
  int32_t mTextSize;
  
  int32_t mNotifyLevel;
  nsCOMPtr<nsIContent> mLastTextNode;
  int32_t mLastTextNodeSize;

  uint8_t mConstrainSize : 1;
  uint8_t mPrettyPrintXML : 1;
  uint8_t mPrettyPrintHasSpecialRoot : 1;
  uint8_t mPrettyPrintHasFactoredElements : 1;
  uint8_t mPrettyPrinting : 1;  // True if we called PrettyPrint() and it
                                // decided we should in fact prettyprint.
  // True to call prevent script execution in the fragment mode.
  uint8_t mPreventScriptExecution : 1;
  
  nsTArray<StackNode>              mContentStack;

  nsCOMPtr<nsIDocumentTransformer> mXSLTProcessor;
};

#endif // nsXMLContentSink_h__
