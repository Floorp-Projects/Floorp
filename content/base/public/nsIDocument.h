/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIDocument_h___
#define nsIDocument_h___

#include "nsISupports.h"
#include "nsEvent.h"
#include "nsAWritableString.h"
#include "nsString.h"

class nsIAtom;
class nsIArena;
class nsIContent;
class nsIDocumentContainer;
class nsIDocumentObserver;
class nsIPresContext;
class nsIPresShell;

class nsIStreamListener;
class nsIStreamObserver;
class nsIStyleSet;
class nsIStyleSheet;
class nsIStyleRule;
class nsIURI;
class nsILoadGroup;
class nsIViewManager;
class nsIScriptGlobalObject;
class nsIDOMEvent;
class nsIDeviceContext;
class nsIParser;
class nsIDOMNode;
class nsINameSpaceManager;
class nsIDOMDocumentFragment;
class nsILineBreaker;
class nsIWordBreaker;
class nsISelection;
class nsIChannel;
class nsIPrincipal;
class nsINodeInfoManager;
class nsIDOMDocument;
class nsIDOMDocumentType;
class nsIBindingManager;
class nsIObserver;
class nsISupportsArray;
class nsIScriptLoader;
class nsString;
class nsIFocusController;

// IID for the nsIDocument interface
#define NS_IDOCUMENT_IID      \
{ 0x94c6ceb0, 0x9447, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// The base value for the content ID counter.
// This counter is used by the document to 
// assign a monotonically increasing ID to each content
// object it creates
#define NS_CONTENT_ID_COUNTER_BASE 10000

//----------------------------------------------------------------------

// Document interface
class nsIDocument : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_IID)

  // All documents have a memory arena associated with them which is
  // used for memory allocation during document creation. This call
  // returns the arena associated with this document.
  NS_IMETHOD GetArena(nsIArena** aArena) = 0;

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               PRBool aReset) = 0;

  NS_IMETHOD StopDocumentLoad() = 0;

  /**
   * Return the title of the document. May return null.
   */
  virtual const nsString* GetDocumentTitle() const = 0;

  /**
   * Return the URL for the document. May return null.
   */
  NS_IMETHOD GetDocumentURL(nsIURI** aURL) const = 0;
  NS_IMETHOD SetDocumentURL(nsIURI* aURL) = 0;

  /**
   * Return the principal responsible for this document.
   */
  NS_IMETHOD GetPrincipal(nsIPrincipal **aPrincipal) = 0;

  /**
   * Update principal responsible for this document to the intersection
   * of its previous value and aPrincipal.
   */
  NS_IMETHOD AddPrincipal(nsIPrincipal *aPrincipal) = 0;
  
  /**
   * Return the LoadGroup for the document. May return null.
   */
  NS_IMETHOD GetDocumentLoadGroup(nsILoadGroup** aGroup) const = 0;

  /**
   * Return the base URL for realtive URLs in the document. May return null (or the document URL).
   */
  NS_IMETHOD GetBaseURL(nsIURI*& aURL) const = 0;
  NS_IMETHOD SetBaseURL(nsIURI* aURL) = 0;

  /**
   * Get/Set the base target of a link in a document.
   */
  NS_IMETHOD GetBaseTarget(nsAWritableString &aBaseTarget)=0;
  NS_IMETHOD SetBaseTarget(const nsAReadableString &aBaseTarget)=0;

  /**
   * Return a standard name for the document's character set. This will
   * trigger a startDocumentLoad if necessary to answer the question.
   */
  NS_IMETHOD GetDocumentCharacterSet(nsAWritableString& oCharSetID) = 0;
  NS_IMETHOD SetDocumentCharacterSet(const nsAReadableString& aCharSetID) = 0;

  /**
   * Add an observer that gets notified whenever the charset changes.
   */
  NS_IMETHOD AddCharSetObserver(nsIObserver* aObserver) = 0;

  /**
   * Remove a charset observer.
   */
  NS_IMETHOD RemoveCharSetObserver(nsIObserver* aObserver) = 0;

  /**
   * Return the language of this document.
   */
  NS_IMETHOD GetContentLanguage(nsAWritableString& aContentLanguage) const = 0;

#ifdef IBMBIDI
  // The state BidiEnabled should persist across multiple views (screen, print)
  // of the same document.

  /**
   * Check if the document contains bidi data.
   * If so, we have to apply the Unicode Bidi Algorithm.
   */
  NS_IMETHOD GetBidiEnabled(PRBool* aBidiEnabled) const = 0;

  /**
   * Indicate the document contains bidi data.
   * Currently, we cannot disable bidi, because once bidi is enabled,
   * it affects a frame model irreversibly, and plays even though
   * the document no longer contains bidi data.
   */
  NS_IMETHOD SetBidiEnabled(PRBool aBidiEnabled) = 0;
#endif // IBMBIDI

  /**
   * Return the Line Breaker for the document
   */
  NS_IMETHOD GetLineBreaker(nsILineBreaker** aResult) = 0;
  NS_IMETHOD SetLineBreaker(nsILineBreaker* aLineBreaker) = 0;
  NS_IMETHOD GetWordBreaker(nsIWordBreaker** aResult) = 0;
  NS_IMETHOD SetWordBreaker(nsIWordBreaker* aWordBreaker) = 0;

  /**
   * Access HTTP header data (this may also get set from other sources, like
   * HTML META tags).
   */
  NS_IMETHOD GetHeaderData(nsIAtom* aHeaderField, nsAWritableString& aData) const = 0;
  NS_IMETHOD SetHeaderData(nsIAtom* aheaderField, const nsAReadableString& aData) = 0;

  /**
   * Create a new presentation shell that will use aContext for
   * it's presentation context (presentation context's <b>must not</b> be
   * shared among multiple presentation shell's).
   */
  NS_IMETHOD CreateShell(nsIPresContext* aContext,
                         nsIViewManager* aViewManager,
                         nsIStyleSet* aStyleSet,
                         nsIPresShell** aInstancePtrResult) = 0;
  virtual PRBool DeleteShell(nsIPresShell* aShell) = 0;
  virtual PRInt32 GetNumberOfShells() = 0;
  NS_IMETHOD GetShellAt(PRInt32 aIndex, nsIPresShell** aShell) = 0;

  /**
   * Return the parent document of this document. Will return null
   * unless this document is within a compound document and has a parent.
   */
  NS_IMETHOD GetParentDocument(nsIDocument** aParent) = 0;
  NS_IMETHOD SetParentDocument(nsIDocument* aParent) = 0;
  NS_IMETHOD AddSubDocument(nsIDocument* aSubDoc) = 0;
  NS_IMETHOD GetNumberOfSubDocuments(PRInt32* aCount) = 0;
  NS_IMETHOD GetSubDocumentAt(PRInt32 aIndex, nsIDocument** aSubDoc) = 0;

  /**
   * Return the root content object for this document.
   */
  NS_IMETHOD GetRootContent(nsIContent** aRoot) = 0;
  NS_IMETHOD SetRootContent(nsIContent* aRoot) = 0;

  /** 
   * Get the direct children of the document - content in
   * the prolog, the root content and content in the epilog.
   */
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const = 0;
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const = 0;
  NS_IMETHOD GetChildCount(PRInt32& aCount) = 0;

  /**
   * Get the style sheets owned by this document.
   * Style sheets are ordered, most significant last.
   */
  NS_IMETHOD GetNumberOfStyleSheets(PRInt32* aCount) = 0;
  NS_IMETHOD GetStyleSheetAt(PRInt32 aIndex, nsIStyleSheet** aSheet) = 0;
  NS_IMETHOD GetIndexOfStyleSheet(nsIStyleSheet* aSheet, PRInt32* aIndex) = 0;
  virtual void AddStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual void RemoveStyleSheet(nsIStyleSheet* aSheet) = 0;
  NS_IMETHOD UpdateStyleSheets(nsISupportsArray* aOldSheets, nsISupportsArray* aNewSheets) = 0;

  NS_IMETHOD InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex, PRBool aNotify) = 0;
  virtual void SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                          PRBool mDisabled) = 0;

  /**
   * Set the object from which a document can get a script context.
   * This is the context within which all scripts (during document 
   * creation and during event handling) will run.
   */
  NS_IMETHOD GetScriptGlobalObject(nsIScriptGlobalObject** aGlobalObject) = 0;
  NS_IMETHOD SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject) = 0;

  /**
   * Get the name space manager for this document
   */
  NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager) = 0;

  /**
   * Get the script loader for this document
   */ 
  NS_IMETHOD GetScriptLoader(nsIScriptLoader** aScriptLoader) = 0;

  /**
   * Get the focus controller for this document
   * This can usually be gotten through the ScriptGlobalObject, but
   * it is set to null during document destruction, when we still might
   * need to fire focus events.
   */
  NS_IMETHOD GetFocusController(nsIFocusController** aFocusController) = 0;

  //----------------------------------------------------------------------

  // Document notification API's

  /**
   * Add a new observer of document change notifications. Whenever
   * content is changed, appended, inserted or removed the observers are
   * informed.
   */
  virtual void AddObserver(nsIDocumentObserver* aObserver) = 0;

  /**
   * Remove an observer of document change notifications. This will
   * return false if the observer cannot be found.
   */
  virtual PRBool RemoveObserver(nsIDocumentObserver* aObserver) = 0;

  // Observation hooks used by content nodes to propagate
  // notifications to document observers.
  NS_IMETHOD BeginUpdate() = 0;
  NS_IMETHOD EndUpdate() = 0;
  NS_IMETHOD BeginLoad() = 0;
  NS_IMETHOD EndLoad() = 0;
  NS_IMETHOD ContentChanged(nsIContent* aContent,
                            nsISupports* aSubContent) = 0;
  // notify that one or two content nodes changed state
  // either may be nsnull, but not both
  NS_IMETHOD ContentStatesChanged(nsIContent* aContent1,
                                  nsIContent* aContent2) = 0;
  NS_IMETHOD AttributeWillChange(nsIContent* aChild,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aAttribute) = 0;
  NS_IMETHOD AttributeChanged(nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType,
                              PRInt32 aHint) = 0; // See nsStyleConsts fot hint values
  NS_IMETHOD ContentAppended(nsIContent* aContainer,
                             PRInt32 aNewIndexInContainer) = 0;
  NS_IMETHOD ContentInserted(nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer) = 0;
  NS_IMETHOD ContentReplaced(nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer) = 0;
  NS_IMETHOD ContentRemoved(nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer) = 0;

  // Observation hooks for style data to propogate notifications
  // to document observers
  NS_IMETHOD StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) = 0; // See nsStyleConsts fot hint values
  NS_IMETHOD StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) = 0;
  NS_IMETHOD StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) = 0;

  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, 
                            nsEvent* aEvent, 
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus) = 0;

  NS_IMETHOD_(PRBool) EventCaptureRegistration(PRInt32 aCapturerIncrement) = 0;
  
  NS_IMETHOD FlushPendingNotifications(PRBool aFlushReflows=PR_TRUE,
                                       PRBool aUpdateViews=PR_FALSE) = 0;

  NS_IMETHOD GetAndIncrementContentID(PRInt32* aID) = 0;

  NS_IMETHOD GetBindingManager(nsIBindingManager** aResult) = 0;

  NS_IMETHOD GetNodeInfoManager(nsINodeInfoManager*& aNodeInfoManager) = 0;

  NS_IMETHOD Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) = 0;

  NS_IMETHOD AddReference(void *aKey, nsISupports *aReference) = 0;
  NS_IMETHOD RemoveReference(void *aKey, nsISupports **aOldReference) = 0;
};


// XXX These belong somewhere else
extern NS_EXPORT nsresult
   NS_NewHTMLDocument(nsIDocument** aInstancePtrResult);

extern NS_EXPORT nsresult
   NS_NewXMLDocument(nsIDocument** aInstancePtrResult);

extern NS_EXPORT nsresult
   NS_NewImageDocument(nsIDocument** aInstancePtrResult);

extern NS_EXPORT nsresult
   NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
                          nsIDocument* aOwnerDocument);
extern NS_EXPORT nsresult
   NS_NewDOMDocument(nsIDOMDocument** aInstancePtrResult,
                     const nsAReadableString& aNamespaceURI, 
                     const nsAReadableString& aQualifiedName, 
                     nsIDOMDocumentType* aDoctype,
                     nsIURI* aBaseURI);

// Note: The buffer passed into NewPostData(...) becomes owned by the IPostData
//       instance and is freed when the instance is destroyed...
//
#if 0
extern NS_EXPORT nsresult
   NS_NewPostData(PRBool aIsFile, char *aData, nsIPostData** aInstancePtrResult);
#endif

#endif /* nsIDocument_h___ */
