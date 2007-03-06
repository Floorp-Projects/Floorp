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

/*
 * Base class for all our document implementations.
 */

#ifndef nsDocument_h___
#define nsDocument_h___

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCRT.h"
#include "nsIDocument.h"
#include "nsWeakReference.h"
#include "nsWeakPtr.h"
#include "nsVoidArray.h"
#include "nsHashSets.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOM3Document.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMNSDocumentStyle.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsStubDocumentObserver.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMNSEventTarget.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMEventTarget.h"
#include "nsIContent.h"
#include "nsIEventListenerManager.h"
#include "nsIDOM3Node.h"
#include "nsIPrincipal.h"
#include "nsIParser.h"
#include "nsBindingManager.h"
#include "nsINodeInfo.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOM3DocumentEvent.h"
#include "nsCOMArray.h"
#include "nsHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsScriptLoader.h"
#include "nsICSSLoader.h"
#include "nsIRadioGroupContainer.h"
#include "nsIScriptEventManager.h"
#include "nsILayoutHistoryState.h"
#include "nsIRequest.h"
#include "nsILoadGroup.h"
#include "nsTObserverArray.h"
#include "nsStubMutationObserver.h"
#include "nsIChannel.h"
#include "nsCycleCollectionParticipant.h"

// Put these here so all document impls get them automatically
#include "nsHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"

#include "nsStyleSet.h"
#include "nsXMLEventsManager.h"
#include "pldhash.h"
#include "nsAttrAndChildArray.h"
#include "nsDOMAttributeMap.h"

#define XML_DECLARATION_BITS_DECLARATION_EXISTS   (1 << 0)
#define XML_DECLARATION_BITS_ENCODING_EXISTS      (1 << 1)
#define XML_DECLARATION_BITS_STANDALONE_EXISTS    (1 << 2)
#define XML_DECLARATION_BITS_STANDALONE_YES       (1 << 3)


class nsIEventListenerManager;
class nsDOMStyleSheetList;
class nsDOMStyleSheetSetList;
class nsIOutputStream;
class nsDocument;
class nsIDTD;
class nsIRadioVisitor;
class nsIFormControl;
struct nsRadioGroupStruct;
class nsOnloadBlocker;
class nsUnblockOnloadEvent;
struct PLEvent;
class nsChildContentList;

PR_BEGIN_EXTERN_C
/* Note that these typedefs declare functions, not pointer to
   functions.  That's the only way in which they differ from
   PLHandleEventProc and PLDestroyEventProc. */
typedef void*
(PR_CALLBACK EventHandlerFunc)(PLEvent* self);
typedef void
(PR_CALLBACK EventDestructorFunc)(PLEvent* self);
PR_END_EXTERN_C

/**
 * Hashentry using a PRUint32 key and a cheap set of nsIContent* owning
 * pointers for the value.
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsUint32ToContentHashEntry : public PLDHashEntryHdr
{
  public:
    typedef const PRUint32& KeyType;
    typedef const PRUint32* KeyTypePointer;

    nsUint32ToContentHashEntry(const KeyTypePointer key) :
      mValue(*key), mValOrHash(nsnull) { }
    nsUint32ToContentHashEntry(const nsUint32ToContentHashEntry& toCopy) :
      mValue(toCopy.mValue), mValOrHash(toCopy.mValOrHash)
    {
      // Pathetic attempt to not die: clear out the other mValOrHash so we're
      // effectively stealing it. If toCopy is destroyed right after this,
      // we'll be OK.
      NS_CONST_CAST(nsUint32ToContentHashEntry&, toCopy).mValOrHash = nsnull;
      NS_ERROR("Copying not supported. Fasten your seat belt.");
    }
    ~nsUint32ToContentHashEntry() { Destroy(); }

    KeyType GetKey() const { return mValue; }
    KeyTypePointer GetKeyPointer() const { return &mValue; }

    PRBool KeyEquals(KeyTypePointer aKey) const { return mValue == *aKey; }

    static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
    static PLDHashNumber HashKey(KeyTypePointer aKey) { return *aKey; }
    enum { ALLOW_MEMMOVE = PR_TRUE };

    // Content set methods
    nsresult PutContent(nsIContent* aContent);

    void RemoveContent(nsIContent* aContent);

    struct Visitor {
      virtual void Visit(nsIContent* aContent) = 0;
    };
    void VisitContent(Visitor* aVisitor);

    PRBool IsEmpty() { return mValOrHash == nsnull; }

  private:
    typedef unsigned long PtrBits;
    typedef nsTHashtable<nsISupportsHashKey> HashSet;
    /** Get the hash pointer (or null if we're not a hash) */
    HashSet* GetHashSet()
    {
      return (PtrBits(mValOrHash) & 0x1) ? nsnull : (HashSet*)mValOrHash;
    }
    /** Find out whether it is an nsIContent (returns weak) */
    nsIContent* GetContent()
    {
      return (PtrBits(mValOrHash) & 0x1)
             ? (nsIContent*)(PtrBits(mValOrHash) & ~0x1)
             : nsnull;
    }
    /** Set the single element, adding a reference */
    nsresult SetContent(nsIContent* aVal)
    {
      NS_IF_ADDREF(aVal);
      mValOrHash = (void*)(PtrBits(aVal) | 0x1);
      return NS_OK;
    }
    /** Initialize the hash */
    nsresult InitHashSet(HashSet** aSet);

    void Destroy();

  private:
    const PRUint32 mValue;
    /** A hash or nsIContent ptr, depending on the lower bit (0=hash, 1=ptr) */
    void* mValOrHash;
};


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

class nsDOMStyleSheetList : public nsIDOMStyleSheetList,
                            public nsStubDocumentObserver
{
public:
  nsDOMStyleSheetList(nsIDocument *aDocument);
  virtual ~nsDOMStyleSheetList();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMSTYLESHEETLIST

  // nsIDocumentObserver
  virtual void NodeWillBeDestroyed(const nsINode *aNode);
  virtual void StyleSheetAdded(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet,
                               PRBool aDocumentSheet);
  virtual void StyleSheetRemoved(nsIDocument *aDocument,
                                 nsIStyleSheet* aStyleSheet,
                                 PRBool aDocumentSheet);

protected:
  PRInt32       mLength;
  nsIDocument*  mDocument;
};

class nsOnloadBlocker : public nsIRequest
{
public:
  nsOnloadBlocker() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST

private:
  ~nsOnloadBlocker() {}
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
                   public nsIDOMNSDocumentStyle,
                   public nsIDOMDocumentView,
                   public nsIDOMDocumentRange,
                   public nsIDOMDocumentTraversal,
                   public nsIDOMDocumentXBL,
                   public nsIDOM3Document,
                   public nsSupportsWeakReference,
                   public nsIDOMEventReceiver,
                   public nsIDOM3EventTarget,
                   public nsIDOMNSEventTarget,
                   public nsIScriptObjectPrincipal,
                   public nsIRadioGroupContainer,
                   public nsStubMutationObserver
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  virtual void Reset(nsIChannel *aChannel, nsILoadGroup *aLoadGroup);
  virtual void ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup,
                          nsIPrincipal* aPrincipal);

  // StartDocumentLoad is pure virtual so that subclasses must override it.
  // The nsDocument StartDocumentLoad does some setup, but does NOT set
  // *aDocListener; this is the job of subclasses.
  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     PRBool aReset = PR_TRUE,
                                     nsIContentSink* aContentSink = nsnull) = 0;

  virtual void StopDocumentLoad();

  virtual void SetDocumentURI(nsIURI* aURI);
  
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
  virtual nsresult CreateShell(nsPresContext* aContext,
                               nsIViewManager* aViewManager,
                               nsStyleSet* aStyleSet,
                               nsIPresShell** aInstancePtrResult);
  virtual PRBool DeleteShell(nsIPresShell* aShell);
  virtual PRUint32 GetNumberOfShells() const;
  virtual nsIPresShell *GetShellAt(PRUint32 aIndex) const;
  virtual void SetShellsHidden(PRBool aHide);

  virtual nsresult SetSubDocumentFor(nsIContent *aContent,
                                     nsIDocument* aSubDoc);
  virtual nsIDocument* GetSubDocumentFor(nsIContent *aContent) const;
  virtual nsIContent* FindContentForSubDocument(nsIDocument *aDocument) const;

  /**
   * Get the style sheets owned by this document.
   * These are ordered, highest priority last
   */
  virtual PRInt32 GetNumberOfStyleSheets() const;
  virtual nsIStyleSheet* GetStyleSheetAt(PRInt32 aIndex) const;
  virtual PRInt32 GetIndexOfStyleSheet(nsIStyleSheet* aSheet) const;
  virtual void AddStyleSheet(nsIStyleSheet* aSheet);
  virtual void RemoveStyleSheet(nsIStyleSheet* aSheet);

  virtual void UpdateStyleSheets(nsCOMArray<nsIStyleSheet>& aOldSheets,
                                 nsCOMArray<nsIStyleSheet>& aNewSheets);
  virtual void AddStyleSheetToStyleSets(nsIStyleSheet* aSheet);
  virtual void RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet);

  virtual void InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex);
  virtual void SetStyleSheetApplicableState(nsIStyleSheet* aSheet,
                                            PRBool aApplicable);

  virtual PRInt32 GetNumberOfCatalogStyleSheets() const;
  virtual nsIStyleSheet* GetCatalogStyleSheetAt(PRInt32 aIndex) const;
  virtual void AddCatalogStyleSheet(nsIStyleSheet* aSheet);
  virtual void EnsureCatalogStyleSheet(const char *aStyleSheetURI);

  virtual nsIChannel* GetChannel() const {
    return mChannel;
  }

  /**
   * Get this document's attribute stylesheet.  May return null if
   * there isn't one.
   */
  virtual nsHTMLStyleSheet* GetAttributeStyleSheet() const {
    return mAttrStyleSheet;
  }

  /**
   * Get this document's inline style sheet.  May return null if there
   * isn't one
   */
  virtual nsIHTMLCSSStyleSheet* GetInlineStyleSheet() const {
    return mStyleAttrStyleSheet;
  }
  
  /**
   * Set the object from which a document can get a script context.
   * This is the context within which all scripts (during document
   * creation and during event handling) will run.
   */
  virtual nsIScriptGlobalObject* GetScriptGlobalObject() const;
  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject);

  virtual nsIScriptGlobalObject* GetScopeObject();

  /**
   * Return the window containing the document (the outer window).
   */
  virtual nsPIDOMWindow *GetWindow();

  /**
   * Return the inner window used as the script compilation scope for
   * this document. If you're not absolutely sure you need this, use
   * GetWindow().
   */
  virtual nsPIDOMWindow *GetInnerWindow();

  /**
   * Get the script loader for this document
   */
  virtual nsScriptLoader* GetScriptLoader();

  virtual void AddMutationObserver(nsIMutationObserver* aObserver);
  virtual void RemoveMutationObserver(nsIMutationObserver* aMutationObserver);

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
  virtual void ContentStatesChanged(nsIContent* aContent1,
                                    nsIContent* aContent2,
                                    PRInt32 aStateMask);

  virtual void AttributeWillChange(nsIContent* aChild,
                                   PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute);

  virtual void StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aOldStyleRule,
                                nsIStyleRule* aNewStyleRule);
  virtual void StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);
  virtual void StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule);

  virtual void FlushPendingNotifications(mozFlushType aType);
  virtual void AddReference(void *aKey, nsISupports *aReference);
  virtual nsISupports *GetReference(void *aKey);
  virtual already_AddRefed<nsISupports> RemoveReference(void *aKey);
  virtual nsIScriptEventManager* GetScriptEventManager();
  virtual void SetXMLDeclaration(const PRUnichar *aVersion,
                                 const PRUnichar *aEncoding,
                                 const PRInt32 aStandalone);
  virtual void GetXMLDeclaration(nsAString& aVersion,
                                 nsAString& aEncoding,
                                 nsAString& Standalone);
  virtual PRBool IsScriptEnabled();

  virtual void OnPageShow(PRBool aPersisted);
  virtual void OnPageHide(PRBool aPersisted);

  // nsINode
  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const;
  virtual PRInt32 IndexOf(nsINode* aPossibleChild) const;
  virtual PRUint32 GetChildCount() const;
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual nsresult DispatchDOMEvent(nsEvent* aEvent, nsIDOMEvent* aDOMEvent,
                                    nsPresContext* aPresContext,
                                    nsEventStatus* aEventStatus);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName,
                            nsIRadioVisitor* aVisitor,
                            PRBool aFlushContent);
  NS_IMETHOD SetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement* aRadio);
  NS_IMETHOD GetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement** aRadio);
  NS_IMETHOD GetPositionInGroup(nsIDOMHTMLInputElement *aRadio,
                                PRInt32 *aPositionIndex,
                                PRInt32 *aItemsInGroup);
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

  // nsIDOMNSDocumentStyle
  NS_DECL_NSIDOMNSDOCUMENTSTYLE

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
  NS_IMETHOD GetListenerManager(PRBool aCreateIfNotFound,
                                nsIEventListenerManager** aResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);
  NS_IMETHOD GetSystemEventGroup(nsIDOMEventGroup** aGroup);

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOM3EventTarget
  NS_DECL_NSIDOM3EVENTTARGET

  // nsIDOMNSEventTarget
  NS_DECL_NSIDOMNSEVENTTARGET

  // nsIScriptObjectPrincipal
  virtual nsIPrincipal* GetPrincipal();

  virtual nsresult Init();
  
  virtual nsresult AddXMLEventsContent(nsIContent * aXMLEventsElement);

  virtual nsresult CreateElem(nsIAtom *aName, nsIAtom *aPrefix,
                              PRInt32 aNamespaceID,
                              PRBool aDocumentDefaultType,
                              nsIContent **aResult);

  virtual NS_HIDDEN_(nsresult) Sanitize();

  virtual NS_HIDDEN_(void) EnumerateSubDocuments(nsSubDocEnumFunc aCallback,
                                                 void *aData);

  virtual NS_HIDDEN_(PRBool) CanSavePresentation(nsIRequest *aNewRequest);
  virtual NS_HIDDEN_(void) Destroy();
  virtual NS_HIDDEN_(already_AddRefed<nsILayoutHistoryState>) GetLayoutHistoryState() const;

  virtual NS_HIDDEN_(void) BlockOnload();
  virtual NS_HIDDEN_(void) UnblockOnload(PRBool aFireSync);

  virtual NS_HIDDEN_(void) AddStyleRelevantLink(nsIContent* aContent, nsIURI* aURI);
  virtual NS_HIDDEN_(void) ForgetLink(nsIContent* aContent);
  virtual NS_HIDDEN_(void) NotifyURIVisitednessChanged(nsIURI* aURI);

  NS_HIDDEN_(void) ClearBoxObjectFor(nsIContent* aContent);

  virtual NS_HIDDEN_(nsresult) GetXBLChildNodesFor(nsIContent* aContent,
                                                   nsIDOMNodeList** aResult);
  virtual NS_HIDDEN_(nsresult) GetContentListFor(nsIContent* aContent,
                                                 nsIDOMNodeList** aResult);
  virtual NS_HIDDEN_(void) FlushSkinBindings();

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDocument, nsIDocument)

  /**
   * Utility method for getElementsByClassName
   */
  static nsresult GetElementsByClassNameHelper(nsIContent* aContent,
                                               const nsAString& aClasses,
                                               nsIDOMNodeList** aReturn);
protected:

  /**
   * Check that aId is not empty and log a message to the console
   * service if it is.
   * @returns PR_TRUE if aId looks correct, PR_FALSE otherwise.
   */
  static PRBool CheckGetElementByIdArg(const nsAString& aId);

  void DispatchContentLoadedEvents();

  void RetrieveRelevantHeaders(nsIChannel *aChannel);

  static PRBool TryChannelCharset(nsIChannel *aChannel,
                                  PRInt32& aCharsetSource,
                                  nsACString& aCharset);
  
  void UpdateLinkMap();
  // Call this before the document does something that will unbind all content.
  // That will stop us from resolving URIs for all links as they are removed.
  void DestroyLinkMap();

  nsresult doCreateShell(nsPresContext* aContext,
                         nsIViewManager* aViewManager, nsStyleSet* aStyleSet,
                         nsCompatibility aCompatMode,
                         nsIPresShell** aInstancePtrResult);

  nsresult ResetStylesheetsToURI(nsIURI* aURI);
  virtual nsStyleSet::sheetType GetAttrSheetType();
  void FillStyleSet(nsStyleSet* aStyleSet);

  // Return whether all the presshells for this document are safe to flush
  PRBool IsSafeToFlush() const;
  
  virtual PRInt32 GetDefaultNamespaceID() const
  {
    return kNameSpaceID_None;
  };

  // Dispatch an event to the ScriptGlobalObject for this document
  void DispatchEventToWindow(nsEvent *aEvent);

  // nsContentList match functions for GetElementsByClassName
  static PRBool MatchClassNames(nsIContent* aContent, PRInt32 aNamespaceID,
                                nsIAtom* aAtom, void* aData);

  static void DestroyClassNameArray(void* aData);

#define NS_DOCUMENT_NOTIFY_OBSERVERS(func_, params_)                  \
  NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(mObservers, nsIDocumentObserver, \
                                     func_, params_);
  
#ifdef DEBUG
  void VerifyRootContentState();
#endif

  nsDocument(const char* aContentType);
  virtual ~nsDocument();

  void LastRelease();

  nsCString mReferrer;
  nsString mLastModified;

  nsVoidArray mCharSetObservers;

  PLDHashTable *mSubDocuments;

  // Array of owning references to all children
  nsAttrAndChildArray mChildren;

  // Pointer to our parser if we're currently in the process of being
  // parsed into.
  nsCOMPtr<nsIParser> mParser;

  nsCOMArray<nsIStyleSheet> mStyleSheets;
  nsCOMArray<nsIStyleSheet> mCatalogSheets;

  // Array of observers
  nsTObserverArray<nsIDocumentObserver> mObservers;

  // The document's script global object, the object from which the
  // document can get its script context and scope. This is the
  // *inner* window object.
  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobalObject;

  // Weak reference to the scope object (aka the script global object)
  // that, unlike mScriptGlobalObject, is never unset once set. This
  // is a weak reference to avoid leaks due to circular references.
  nsWeakPtr mScopeObject;

  nsCOMPtr<nsIEventListenerManager> mListenerManager;
  nsCOMPtr<nsIDOMStyleSheetList> mDOMStyleSheets;
  nsRefPtr<nsDOMStyleSheetSetList> mStyleSheetSetList;
  nsRefPtr<nsScriptLoader> mScriptLoader;
  nsDocHeaderData* mHeaderData;

  nsHashtable mRadioGroups;

  // True if the document has been detached from its content viewer.
  PRPackedBool mIsGoingAway:1;
  // True if the document is being destroyed.
  PRPackedBool mInDestructor:1;
  // True if the document "page" is not hidden
  PRPackedBool mVisible:1;

  PRPackedBool mShellsAreHidden:1;

  PRUint8 mXMLDeclarationBits;

  PRUint8 mDefaultElementType;

  nsInterfaceHashtable<nsISupportsHashKey, nsPIBoxObject> *mBoxObjectTable;
  nsInterfaceHashtable<nsVoidPtrHashKey, nsISupports> *mContentWrapperHash;

  // The channel that got passed to StartDocumentLoad(), if any
  nsCOMPtr<nsIChannel> mChannel;
  nsRefPtr<nsHTMLStyleSheet> mAttrStyleSheet;
  nsCOMPtr<nsIHTMLCSSStyleSheet> mStyleAttrStyleSheet;
  nsRefPtr<nsXMLEventsManager> mXMLEventsManager;

  nsCOMPtr<nsIScriptEventManager> mScriptEventManager;

  nsString mBaseTarget;

private:
  friend class nsUnblockOnloadEvent;

  void PostUnblockOnloadEvent();
  void DoUnblockOnload();

  /**
   * See if aDocument is a child of this.  If so, return the frame element in
   * this document that holds currentDoc (or an ancestor).
   */
  already_AddRefed<nsIDOMElement>
    CheckAncestryAndGetFrame(nsIDocument* aDocument) const;

  // Just like EnableStyleSheetsForSet, but doesn't check whether
  // aSheetSet is null and allows the caller to control whether to set
  // aSheetSet as the preferred set in the CSSLoader.
  void EnableStyleSheetsForSetInternal(const nsAString& aSheetSet,
                                       PRBool aUpdateCSSLoader);

  // These are not implemented and not supported.
  nsDocument(const nsDocument& aOther);
  nsDocument& operator=(const nsDocument& aOther);

  nsSmallVoidArray mPresShells;

  nsCOMPtr<nsISupports> mXPathEvaluatorTearoff;

  // The layout history state that should be used by nodes in this
  // document.  We only actually store a pointer to it when:
  // 1)  We have no script global object.
  // 2)  We haven't had Destroy() called on us yet.
  nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;

  PRUint32 mOnloadBlockCount;
  nsCOMPtr<nsIRequest> mOnloadBlocker;
  
  // A map from unvisited URI hashes to content elements
  nsTHashtable<nsUint32ToContentHashEntry> mLinkMap;
  // URIs whose visitedness has changed while we were hidden
  nsCOMArray<nsIURI> mVisitednessChangedURIs;

  // Member to store out last-selected stylesheet set.
  nsString mLastStyleSheetSet;

  // Our update nesting level
  PRUint32 mUpdateNestLevel;
};


#endif /* nsDocument_h___ */
