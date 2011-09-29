/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "nsDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIScriptElement.h"
#include "jsapi.h"
#include "nsTArray.h"

#include "pldhash.h"
#include "nsIHttpChannel.h"
#include "nsHTMLStyleSheet.h"

// Document.Write() related
#include "nsIWyciwygChannel.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"

#include "nsICommandManager.h"

class nsIEditor;
class nsIEditorDocShell;
class nsIParser;
class nsIURI;
class nsIMarkupDocumentViewer;
class nsIDocumentCharsetInfo;
class nsICachingChannel;

class nsHTMLDocument : public nsDocument,
                       public nsIHTMLDocument,
                       public nsIDOMHTMLDocument
{
public:
  using nsDocument::SetDocumentURI;

  nsHTMLDocument();
  virtual nsresult Init();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);
  virtual void ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal);

  virtual nsresult CreateShell(nsPresContext* aContext,
                               nsIViewManager* aViewManager,
                               nsStyleSet* aStyleSet,
                               nsIPresShell** aInstancePtrResult);

  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     bool aReset = true,
                                     nsIContentSink* aSink = nsnull);
  virtual void StopDocumentLoad();

  virtual void BeginLoad();

  virtual void EndLoad();

  virtual void SetCompatibilityMode(nsCompatibility aMode);

  virtual bool IsWriting()
  {
    return mWriteLevel != PRUint32(0);
  }

  virtual bool GetIsFrameset() { return mIsFrameset; }
  virtual void SetIsFrameset(bool aFrameset) { mIsFrameset = aFrameset; }

  virtual NS_HIDDEN_(nsContentList*) GetForms();
 
  virtual NS_HIDDEN_(nsContentList*) GetFormControls();
 
  // nsIDOMDocument interface
  NS_FORWARD_NSIDOMDOCUMENT(nsDocument::)

  // nsIDOMNode interface
  NS_FORWARD_NSIDOMNODE(nsDocument::)

  // nsIDOMHTMLDocument interface
  NS_DECL_NSIDOMHTMLDOCUMENT

  /**
   * Returns the result of document.all[aID] which can either be a node
   * or a nodelist depending on if there are multiple nodes with the same
   * id.
   */
  nsISupports *GetDocumentAllResult(const nsAString& aID,
                                    nsWrapperCache **aCache,
                                    nsresult *aResult);

  nsIContent *GetBody(nsresult *aResult);
  already_AddRefed<nsContentList> GetElementsByName(const nsAString & aName)
  {
    return NS_GetFuncStringContentList(this, MatchNameAttribute, nsnull,
                                       UseExistingNameString, aName);
  }


  virtual nsresult ResolveName(const nsAString& aName,
                               nsIContent *aForm,
                               nsISupports **aResult,
                               nsWrapperCache **aCache);

  virtual void ScriptLoading(nsIScriptElement *aScript);
  virtual void ScriptExecuted(nsIScriptElement *aScript);

  virtual void AddedForm();
  virtual void RemovedForm();
  virtual PRInt32 GetNumFormsSynchronous();
  virtual void TearingDownEditor(nsIEditor *aEditor);
  virtual void SetIsXHTML(bool aXHTML) { mIsRegularHTML = !aXHTML; }
  virtual void SetDocWriteDisabled(bool aDisabled)
  {
    mDisableDocWrite = aDisabled;
  }

  nsresult ChangeContentEditableCount(nsIContent *aElement, PRInt32 aChange);
  void DeferredContentEditableCountChange(nsIContent *aElement);

  virtual EditingState GetEditingState()
  {
    return mEditingState;
  }

  virtual void DisableCookieAccess()
  {
    mDisableCookieAccess = PR_TRUE;
  }

  class nsAutoEditingState {
  public:
    nsAutoEditingState(nsHTMLDocument* aDoc, EditingState aState)
      : mDoc(aDoc), mSavedState(aDoc->mEditingState)
    {
      aDoc->mEditingState = aState;
    }
    ~nsAutoEditingState() {
      mDoc->mEditingState = mSavedState;
    }
  private:
    nsHTMLDocument* mDoc;
    EditingState    mSavedState;
  };
  friend class nsAutoEditingState;

  void EndUpdate(nsUpdateType aUpdateType);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLDocument, nsDocument)

  virtual nsresult SetEditingState(EditingState aState);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual NS_HIDDEN_(void) RemovedFromDocShell();

  virtual mozilla::dom::Element *GetElementById(const nsAString& aElementId)
  {
    return nsDocument::GetElementById(aElementId);
  }

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  nsresult GetBodySize(PRInt32* aWidth,
                       PRInt32* aHeight);

  nsIContent *MatchId(nsIContent *aContent, const nsAString& aId);

  static bool MatchLinks(nsIContent *aContent, PRInt32 aNamespaceID,
                           nsIAtom* aAtom, void* aData);
  static bool MatchAnchors(nsIContent *aContent, PRInt32 aNamespaceID,
                             nsIAtom* aAtom, void* aData);
  static bool MatchNameAttribute(nsIContent* aContent, PRInt32 aNamespaceID,
                                   nsIAtom* aAtom, void* aData);
  static void* UseExistingNameString(nsINode* aRootNode, const nsString* aName);

  static void DocumentWriteTerminationFunc(nsISupports *aRef);

  void GetDomainURI(nsIURI **uri);

  nsresult WriteCommon(JSContext *cx, const nsAString& aText,
                       bool aNewlineTerminate);

  nsresult CreateAndAddWyciwygChannel(void);
  nsresult RemoveWyciwygChannel(void);

  /**
   * Like IsEditingOn(), but will flush as needed first.
   */
  bool IsEditingOnAfterFlush();

  void *GenerateParserKey(void);

  virtual PRInt32 GetDefaultNamespaceID() const
  {
    return kNameSpaceID_XHTML;
  }

  nsCOMPtr<nsIDOMHTMLCollection> mImages;
  nsCOMPtr<nsIDOMHTMLCollection> mApplets;
  nsCOMPtr<nsIDOMHTMLCollection> mEmbeds;
  nsCOMPtr<nsIDOMHTMLCollection> mLinks;
  nsCOMPtr<nsIDOMHTMLCollection> mAnchors;
  nsCOMPtr<nsIDOMHTMLCollection> mScripts;
  nsRefPtr<nsContentList> mForms;
  nsRefPtr<nsContentList> mFormControls;

  /** # of forms in the document, synchronously set */
  PRInt32 mNumForms;

  static PRUint32 gWyciwygSessionCnt;

  static bool TryHintCharset(nsIMarkupDocumentViewer* aMarkupDV,
                               PRInt32& aCharsetSource,
                               nsACString& aCharset);
  static bool TryUserForcedCharset(nsIMarkupDocumentViewer* aMarkupDV,
                                     nsIDocumentCharsetInfo*  aDocInfo,
                                     PRInt32& aCharsetSource,
                                     nsACString& aCharset);
  static bool TryCacheCharset(nsICachingChannel* aCachingChannel,
                                PRInt32& aCharsetSource,
                                nsACString& aCharset);
  // aParentDocument could be null.
  bool TryParentCharset(nsIDocumentCharsetInfo*  aDocInfo,
                          nsIDocument* aParentDocument,
                          PRInt32& charsetSource, nsACString& aCharset);
  static bool UseWeakDocTypeDefault(PRInt32& aCharsetSource,
                                      nsACString& aCharset);
  static bool TryDefaultCharset(nsIMarkupDocumentViewer* aMarkupDV,
                                  PRInt32& aCharsetSource,
                                  nsACString& aCharset);

  void StartAutodetection(nsIDocShell *aDocShell, nsACString& aCharset,
                          const char* aCommand);

  // Override so we can munge the charset on our wyciwyg channel as needed.
  virtual void SetDocumentCharacterSet(const nsACString& aCharSetID);

  // mWriteState tracks the status of this document if the document is being
  // entirely created by script. In the normal load case, mWriteState will be
  // eNotWriting. Once document.open has been called (either implicitly or
  // explicitly), mWriteState will be eDocumentOpened. When document.close has
  // been called, mWriteState will become eDocumentClosed if there have been no
  // external script loads in the meantime. If there have been, then mWriteState
  // becomes ePendingClose, indicating that we might still be writing, but that
  // we shouldn't process any further close() calls.
  enum {
    eNotWriting,
    eDocumentOpened,
    ePendingClose,
    eDocumentClosed
  } mWriteState;

  // Tracks if we are currently processing any document.write calls (either
  // implicit or explicit). Note that if a write call writes out something which
  // would block the parser, then mWriteLevel will be incorrect until the parser
  // finishes processing that script.
  PRUint32 mWriteLevel;

  nsAutoTArray<nsIScriptElement*, 1> mPendingScripts;

  // Load flags of the document's channel
  PRUint32 mLoadFlags;

  bool mIsFrameset;

  bool mTooDeepWriteRecursion;

  bool mDisableDocWrite;

  bool mWarnedWidthHeight;

  nsCOMPtr<nsIWyciwygChannel> mWyciwygChannel;

  /* Midas implementation */
  nsresult   GetMidasCommandManager(nsICommandManager** aCommandManager);

  nsCOMPtr<nsICommandManager> mMidasCommandManager;

  nsresult TurnEditingOff();
  nsresult EditingStateChanged();
  void MaybeEditingStateChanged();

  PRUint32 mContentEditableCount;
  EditingState mEditingState;

  nsresult   DoClipboardSecurityCheck(bool aPaste);
  static jsid        sCutCopyInternal_id;
  static jsid        sPasteInternal_id;

  // When false, the .cookies property is completely disabled
  bool mDisableCookieAccess;
};

#define NS_HTML_DOCUMENT_INTERFACE_TABLE_BEGIN(_class)                        \
    NS_DOCUMENT_INTERFACE_TABLE_BEGIN(_class)                                 \
    NS_INTERFACE_TABLE_ENTRY(_class, nsIHTMLDocument)                         \
    NS_INTERFACE_TABLE_ENTRY(_class, nsIDOMHTMLDocument)

#endif /* nsHTMLDocument_h___ */
