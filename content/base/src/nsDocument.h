/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsDocument_h___
#define nsDocument_h___

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCRT.h"
#include "nsIDocument.h"
#include "nsWeakReference.h"
#include "nsWeakPtr.h"
#include "nsVoidArray.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOM3Document.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentStyle.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsStubDocumentObserver.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMEventTarget.h"
#include "nsIContent.h"
#include "nsIEventListenerManager.h"
#include "nsGenericDOMNodeList.h"
#include "nsIDOM3Node.h"
#include "nsIPrincipal.h"
#include "nsIBindingManager.h"
#include "nsINodeInfo.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOM3DocumentEvent.h"
#include "nsCOMArray.h"
#include "nsHashtable.h"
#include "nsIWordBreakerFactory.h"
#include "nsILineBreakerFactory.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsScriptLoader.h"
#include "nsICSSLoader.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIRadioGroupContainer.h"
#include "nsIScriptEventManager.h"

// Put these here so all document impls get them automatically
#include "nsHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"

#include "nsStyleSet.h"

#include "pldhash.h"


#define XML_DECLARATION_BITS_DECLARATION_EXISTS   (1 << 0)
#define XML_DECLARATION_BITS_ENCODING_EXISTS      (1 << 1)
#define XML_DECLARATION_BITS_STANDALONE_EXISTS    (1 << 2)
#define XML_DECLARATION_BITS_STANDALONE_YES       (1 << 3)


class nsIEventListenerManager;
class nsDOMStyleSheetList;
class nsIOutputStream;
class nsDocument;
class nsIDTD;
class nsXPathDocumentTearoff;
class nsIRadioVisitor;
class nsIFormControl;
struct nsRadioGroupStruct;


class nsDocHeaderData
{
public:
  nsDocHeaderData(nsIAtom* aField, const nsAString& aData)
    : mField(aField), mData(aData), mNext(nsnull)
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

// Represents the children of a document (prolog, epilog and
// document element)
class nsDocumentChildNodes : public nsGenericDOMNodeList
{
public:
  nsDocumentChildNodes(nsIDocument* aDocument);
  ~nsDocumentChildNodes();

  NS_IMETHOD    GetLength(PRUint32* aLength);
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);

  void DropReference();

protected:
  nsDocumentChildNodes(); // Not implemented

  nsIDocument* mDocument;
};


class nsDOMStyleSheetList : public nsIDOMStyleSheetList,
                            public nsStubDocumentObserver
{
public:
  nsDOMStyleSheetList(nsIDocument *aDocument);
  virtual ~nsDOMStyleSheetList();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMSTYLESHEETLIST

  // nsIDocumentObserver
  virtual void DocumentWillBeDestroyed(nsIDocument *aDocument);
  virtual void StyleSheetAdded(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet);
  virtual void StyleSheetRemoved(nsIDocument *aDocument,
                                 nsIStyleSheet* aStyleSheet);

protected:
  PRInt32       mLength;
  nsIDocument*  mDocument;
  void*         mScriptObject;
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
                   public nsIDOMNSDocument,
                   public nsIDOMDocumentEvent,
                   public nsIDOM3DocumentEvent,
                   public nsIDOMDocumentStyle,
                   public nsIDOMDocumentView,
                   public nsIDOMDocumentRange,
                   public nsIDOMDocumentTraversal,
                   public nsIDOMDocumentXBL,
                   public nsIDOM3Document,
                   public nsSupportsWeakReference,
                   public nsIDOMEventReceiver,
                   public nsIDOM3EventTarget,
                   public nsIScriptObjectPrincipal,
                   public nsIRadioGroupContainer
{
public:
  NS_DECL_ISUPPORTS

  virtual void Reset(nsIChannel *aChannel, nsILoadGroup *aLoadGroup);
  virtual void ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup);

  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     PRBool aReset = PR_TRUE,
                                     nsIContentSink* aContentSink = nsnull);

  virtual void StopDocumentLoad();

  /**
   * Return the principal responsible for this document.
   */
  virtual nsIPrincipal* GetPrincipal();

  /**
   * Set the principal responsible for this document.
   */
  virtual void SetPrincipal(nsIPrincipal *aPrincipal);

  /**
   * Get the Content-Type of this document.
   */
  // NS_IMETHOD GetContentType(nsAString& aContentType);
  // Already declared in nsIDOMNSDocument

  /**
   * Set the Content-Type of this document.
   */
  virtual void SetContentType(const nsAString& aContentType);

  virtual nsresult SetBaseURI(nsIURI* aURI);

  /**
   * Get/Set the base target of a link in a document.
   */
  virtual void GetBaseTarget(nsAString &aBaseTarget) const;
  virtual void SetBaseTarget(const nsAString &aBaseTarget);

  /**
   * Return a standard name for the document's character set. This will
   * trigger a startDocumentLoad if necessary to answer the question.
   */
  virtual void SetDocumentCharacterSet(const nsACString& aCharSetID);

  /**
   * Add an observer that gets notified whenever the charset changes.
   */
  virtual nsresult AddCharSetObserver(nsIObserver* aObserver);

  /**
   * Remove a charset observer.
   */
  virtual void RemoveCharSetObserver(nsIObserver* aObserver);

  /**
   * Return the Line Breaker for the document
   */
  virtual nsILineBreaker* GetLineBreaker();
  virtual void SetLineBreaker(nsILineBreaker* aLineBreaker);
  virtual nsIWordBreaker* GetWordBreaker();
  virtual void SetWordBreaker(nsIWordBreaker* aWordBreaker);

  /**
   * Access HTTP header data (this may also get set from other sources, like
   * HTML META tags).
   */
  virtual void GetHeaderData(nsIAtom* aHeaderField, nsAString& aData) const;
  virtual void SetHeaderData(nsIAtom* aheaderField,
                             const nsAString& aData);

  /**
   * Create a new presentation shell that will use aContext for
   * it's presentation context (presentation context's <b>must not</b> be
   * shared among multiple presentation shell's).
   */
  virtual nsresult CreateShell(nsIPresContext* aContext,
                               nsIViewManager* aViewManager,
                               nsStyleSet* aStyleSet,
                               nsIPresShell** aInstancePtrResult);
  virtual PRBool DeleteShell(nsIPresShell* aShell);
  virtual PRUint32 GetNumberOfShells() const;
  virtual nsIPresShell *GetShellAt(PRUint32 aIndex) const;

  virtual nsresult SetSubDocumentFor(nsIContent *aContent,
                                     nsIDocument* aSubDoc);
  virtual nsIDocument* GetSubDocumentFor(nsIContent *aContent) const;
  virtual nsIContent* FindContentForSubDocument(nsIDocument *aDocument) const;

  virtual void SetRootContent(nsIContent* aRoot);

  /**
   * Get the direct children of the document - content in
   * the prolog, the root content and content in the epilog.
   */
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const;
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const;
  virtual PRUint32 GetChildCount() const;

  /**
   * Get the style sheets owned by this document.
   * These are ordered, highest priority last
   */
  virtual PRInt32 GetNumberOfStyleSheets(PRBool aIncludeSpecialSheets) const;
  virtual nsIStyleSheet* GetStyleSheetAt(PRInt32 aIndex,
                                         PRBool aIncludeSpecialSheets) const;
  virtual PRInt32 GetIndexOfStyleSheet(nsIStyleSheet* aSheet) const;
  virtual void AddStyleSheet(nsIStyleSheet* aSheet, PRUint32 aFlags);
  virtual void RemoveStyleSheet(nsIStyleSheet* aSheet);

  virtual void UpdateStyleSheets(nsCOMArray<nsIStyleSheet>& aOldSheets,
                                 nsCOMArray<nsIStyleSheet>& aNewSheets);
  virtual void AddStyleSheetToStyleSets(nsIStyleSheet* aSheet);
  virtual void RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet);

  virtual void InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex);
  virtual void SetStyleSheetApplicableState(nsIStyleSheet* aSheet,
                                            PRBool aApplicable);

  /**
   * Get this document's attribute stylesheet.  May return null if
   * there isn't one.
   */
  nsHTMLStyleSheet* GetAttributeStyleSheet() const {
    return mAttrStyleSheet;
  }

  /**
   * Get this document's inline style sheet.  May return null if there
   * isn't one
   */
  nsIHTMLCSSStyleSheet* GetInlineStyleSheet() const {
    return mStyleAttrStyleSheet;
  }
  
  /**
   * Set the object from which a document can get a script context.
   * This is the context within which all scripts (during document
   * creation and during event handling) will run.
   */
  virtual nsIScriptGlobalObject* GetScriptGlobalObject() const;
  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject);

  /**
   * Get the script loader for this document
   */
  virtual nsIScriptLoader* GetScriptLoader();

  /**
   * Add a new observer of document change notifications. Whenever
   * content is changed, appended, inserted or removed the observers are
   * informed.
   */
  virtual void AddObserver(nsIDocumentObserver* aObserver);

  /**
   * Remove an observer of document change notifications. This will
   * return false if the observer cannot be found.
   */
  virtual PRBool RemoveObserver(nsIDocumentObserver* aObserver);

  // Observation hooks used to propagate notifications to document
  // observers.
  virtual void BeginUpdate(nsUpdateType aUpdateType);
  virtual void EndUpdate(nsUpdateType aUpdateType);
  virtual void BeginLoad();
  virtual void EndLoad();
  virtual void CharacterDataChanged(nsIContent* aContent,
                                    PRBool aAppend);
  virtual void ContentStatesChanged(nsIContent* aContent1,
                                    nsIContent* aContent2,
                                    PRInt32 aStateMask);

  virtual void AttributeWillChange(nsIContent* aChild,
                                   PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute);
  virtual void AttributeChanged(nsIContent* aChild,
                                PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute,
                                PRInt32 aModType);
  virtual void ContentAppended(nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer);
  virtual void ContentInserted(nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer);
  virtual void ContentRemoved(nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer);

  virtual void StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aOldStyleRule,
                                nsIStyleRule* aNewStyleRule);
  virtual void StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);
  virtual void StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule);

  virtual void FlushPendingNotifications(mozFlushType aType);
  virtual void AddReference(void *aKey, nsISupports *aReference);
  virtual already_AddRefed<nsISupports> RemoveReference(void *aKey);
  virtual nsIScriptEventManager* GetScriptEventManager();
  virtual void SetXMLDeclaration(const nsAString& aVersion,
                               const nsAString& aEncoding,
                               const nsAString& Standalone);
  virtual void GetXMLDeclaration(nsAString& aVersion,
                                 nsAString& aEncoding,
                                 nsAString& Standalone);
  virtual PRBool IsScriptEnabled();

  virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName,
                            nsIRadioVisitor* aVisitor);
  NS_IMETHOD SetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement* aRadio);
  NS_IMETHOD GetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement** aRadio);
  NS_IMETHOD GetNextRadioButton(const nsAString& aName,
                                const PRBool aPrevious,
                                nsIDOMHTMLInputElement*  aFocusedRadio,
                                nsIDOMHTMLInputElement** aRadioOut);
  NS_IMETHOD AddToRadioGroup(const nsAString& aName,
                             nsIFormControl* aRadio);
  NS_IMETHOD RemoveFromRadioGroup(const nsAString& aName,
                                  nsIFormControl* aRadio);

  // for radio group
  nsresult GetRadioGroup(const nsAString& aName,
                         nsRadioGroupStruct **aRadioGroup);

  // nsIDOMNode
  NS_DECL_NSIDOMNODE

  // nsIDOM3Node
  NS_DECL_NSIDOM3NODE

  // nsIDOMDocument
  NS_DECL_NSIDOMDOCUMENT

  // nsIDOM3Document
  NS_DECL_NSIDOM3DOCUMENT

  // nsIDOMXMLDocument
  NS_DECL_NSIDOMXMLDOCUMENT

  // nsIDOMNSDocument
  NS_DECL_NSIDOMNSDOCUMENT

  // nsIDOMDocumentEvent
  NS_DECL_NSIDOMDOCUMENTEVENT

  // nsIDOM3DocumentEvent
  NS_DECL_NSIDOM3DOCUMENTEVENT

  // nsIDOMDocumentStyle
  NS_DECL_NSIDOMDOCUMENTSTYLE

  // nsIDOMDocumentView
  NS_DECL_NSIDOMDOCUMENTVIEW

  // nsIDOMDocumentRange
  NS_DECL_NSIDOMDOCUMENTRANGE

  // nsIDOMDocumentTraversal
  NS_DECL_NSIDOMDOCUMENTTRAVERSAL

  // nsIDOMDocumentXBL
  NS_DECL_NSIDOMDOCUMENTXBL

  // nsIDOMEventReceiver interface
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);
  NS_IMETHOD GetSystemEventGroup(nsIDOMEventGroup** aGroup);

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOM3EventTarget
  NS_DECL_NSIDOM3EVENTTARGET

  // nsIScriptObjectPrincipal
  // virtual nsIPrincipal* GetPrincipal();
  // Already declared in nsIDocument

  virtual nsresult Init();

  virtual nsresult CreateElem(nsIAtom *aName, nsIAtom *aPrefix,
                              PRInt32 aNamespaceID,
                              PRBool aDocumentDefaultType,
                              nsIContent **aResult);

protected:
  // subclass hooks for sheet ordering
  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet,
                                     PRUint32 aFlags);
  virtual void InternalInsertStyleSheetAt(nsIStyleSheet* aSheet,
                                          PRInt32 aIndex);
  virtual nsIStyleSheet* InternalGetStyleSheetAt(PRInt32 aIndex) const;
  virtual PRInt32 InternalGetNumberOfStyleSheets() const;

  void RetrieveRelevantHeaders(nsIChannel *aChannel);

  static PRBool TryChannelCharset(nsIChannel *aChannel,
                                  PRInt32& aCharsetSource,
                                  nsACString& aCharset);
  
  nsresult doCreateShell(nsIPresContext* aContext,
                         nsIViewManager* aViewManager, nsStyleSet* aStyleSet,
                         nsCompatibility aCompatMode,
                         nsIPresShell** aInstancePtrResult);

  nsresult ResetStylesheetsToURI(nsIURI* aURI);
  virtual nsStyleSet::sheetType GetAttrSheetType();

  nsresult CreateElement(nsIAtom *aName, nsIAtom *aPrefix,
                         PRInt32 aNamespaceID, PRInt32 aElementType,
                         nsIContent** aResult);
  nsresult CreateElement(nsINodeInfo *aNodeInfo, PRInt32 aElementType,
                         nsIContent** aResult);

  virtual PRInt32 GetDefaultNamespaceID() const
  {
    return kNameSpaceID_None;
  };

  nsDocument();
  virtual ~nsDocument();

  nsCString mReferrer;
  nsCString mLastModified;
  nsCOMPtr<nsIPrincipal> mPrincipal;

  nsVoidArray mCharSetObservers;

  PLDHashTable *mSubDocuments;

  nsSmallVoidArray mPresShells;

  // Array of owning references to all children
  nsCOMArray<nsIContent> mChildren;

  nsCOMArray<nsIStyleSheet> mStyleSheets;

  // Basically always has at least 1 entry
  nsAutoVoidArray mObservers;
  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobalObject;
  nsCOMPtr<nsIEventListenerManager> mListenerManager;
  nsCOMPtr<nsIDOMStyleSheetList> mDOMStyleSheets;
  nsCOMPtr<nsIScriptLoader> mScriptLoader;
  nsDocHeaderData* mHeaderData;
  nsCOMPtr<nsILineBreaker> mLineBreaker;
  nsCOMPtr<nsIWordBreaker> mWordBreaker;

  nsRefPtr<nsDocumentChildNodes> mChildNodes;

  nsHashtable mRadioGroups;

  // True if the document is being destroyed.
  PRPackedBool mIsGoingAway;

  // True if the document is being destroyed.
  PRPackedBool mInDestructor;

  PRUint8 mXMLDeclarationBits;

  PRUint8 mDefaultElementType;

  nsSupportsHashtable* mBoxObjectTable;

  nsSupportsHashtable mContentWrapperHash;

  nsCOMPtr<nsICSSLoader> mCSSLoader;
  nsRefPtr<nsHTMLStyleSheet> mAttrStyleSheet;
  nsCOMPtr<nsIHTMLCSSStyleSheet> mStyleAttrStyleSheet;

  nsCOMPtr<nsIScriptEventManager> mScriptEventManager;

  nsString mBaseTarget;

private:
  nsresult IsAllowedAsChild(PRUint16 aNodeType, nsIContent* aRefContent);

  // These are not implemented and not supported.
  nsDocument(const nsDocument& aOther);
  nsDocument& operator=(const nsDocument& aOther);

  nsXPathDocumentTearoff* mXPathDocument;
};



#endif /* nsDocument_h___ */
