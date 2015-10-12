/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEditor.h"

#include "mozilla/DebugOnly.h"          // for DebugOnly

#include <stdio.h>                      // for nullptr, stdout
#include <string.h>                     // for strcmp

#include "ChangeAttributeTxn.h"         // for ChangeAttributeTxn
#include "CreateElementTxn.h"           // for CreateElementTxn
#include "DeleteNodeTxn.h"              // for DeleteNodeTxn
#include "DeleteRangeTxn.h"             // for DeleteRangeTxn
#include "DeleteTextTxn.h"              // for DeleteTextTxn
#include "EditAggregateTxn.h"           // for EditAggregateTxn
#include "EditTxn.h"                    // for EditTxn
#include "IMETextTxn.h"                 // for IMETextTxn
#include "InsertNodeTxn.h"              // for InsertNodeTxn
#include "InsertTextTxn.h"              // for InsertTextTxn
#include "JoinNodeTxn.h"                // for JoinNodeTxn
#include "PlaceholderTxn.h"             // for PlaceholderTxn
#include "SplitNodeTxn.h"               // for SplitNodeTxn
#include "mozFlushType.h"               // for mozFlushType::Flush_Frames
#include "mozInlineSpellChecker.h"      // for mozInlineSpellChecker
#include "mozilla/CheckedInt.h"         // for CheckedInt
#include "mozilla/IMEStateManager.h"    // for IMEStateManager
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/dom/Selection.h"      // for Selection, etc
#include "mozilla/Services.h"           // for GetObserverService
#include "mozilla/TextComposition.h"    // for TextComposition
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"        // for Element, nsINode::AsElement
#include "mozilla/dom/Text.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAString.h"                  // for nsAString_internal::Length, etc
#include "nsCCUncollectableMarker.h"    // for nsCCUncollectableMarker
#include "nsCaret.h"                    // for nsCaret
#include "nsCaseTreatment.h"
#include "nsCharTraits.h"               // for NS_IS_HIGH_SURROGATE, etc
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsComputedDOMStyle.h"         // for nsComputedDOMStyle
#include "nsContentUtils.h"             // for nsContentUtils
#include "nsDOMString.h"                // for DOMStringIsNull
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsEditorEventListener.h"      // for nsEditorEventListener
#include "nsEditorUtils.h"              // for nsAutoRules, etc
#include "nsError.h"                    // for NS_OK, etc
#include "nsFocusManager.h"             // for nsFocusManager
#include "nsFrameSelection.h"           // for nsFrameSelection
#include "nsGkAtoms.h"                  // for nsGkAtoms, nsGkAtoms::dir
#include "nsIAbsorbingTransaction.h"    // for nsIAbsorbingTransaction
#include "nsIAtom.h"                    // for nsIAtom
#include "nsIContent.h"                 // for nsIContent
#include "nsIDOMAttr.h"                 // for nsIDOMAttr
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsIDOMDocument.h"             // for nsIDOMDocument
#include "nsIDOMElement.h"              // for nsIDOMElement
#include "nsIDOMEvent.h"                // for nsIDOMEvent
#include "nsIDOMEventListener.h"        // for nsIDOMEventListener
#include "nsIDOMEventTarget.h"          // for nsIDOMEventTarget
#include "nsIDOMHTMLElement.h"          // for nsIDOMHTMLElement
#include "nsIDOMKeyEvent.h"             // for nsIDOMKeyEvent, etc
#include "nsIDOMMozNamedAttrMap.h"      // for nsIDOMMozNamedAttrMap
#include "nsIDOMMouseEvent.h"           // for nsIDOMMouseEvent
#include "nsIDOMNode.h"                 // for nsIDOMNode, etc
#include "nsIDOMNodeList.h"             // for nsIDOMNodeList
#include "nsIDOMText.h"                 // for nsIDOMText
#include "nsIDocument.h"                // for nsIDocument
#include "nsIDocumentStateListener.h"   // for nsIDocumentStateListener
#include "nsIEditActionListener.h"      // for nsIEditActionListener
#include "nsIEditorObserver.h"          // for nsIEditorObserver
#include "nsIEditorSpellCheck.h"        // for nsIEditorSpellCheck
#include "nsIFrame.h"                   // for nsIFrame
#include "nsIHTMLDocument.h"            // for nsIHTMLDocument
#include "nsIInlineSpellChecker.h"      // for nsIInlineSpellChecker, etc
#include "nsNameSpaceManager.h"        // for kNameSpaceID_None, etc
#include "nsINode.h"                    // for nsINode, etc
#include "nsIPlaintextEditor.h"         // for nsIPlaintextEditor, etc
#include "nsIPresShell.h"               // for nsIPresShell
#include "nsISelectionController.h"     // for nsISelectionController, etc
#include "nsISelectionDisplay.h"        // for nsISelectionDisplay, etc
#include "nsISupportsBase.h"            // for nsISupports
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_IF_ADDREF
#include "nsITransaction.h"             // for nsITransaction
#include "nsITransactionManager.h"
#include "nsIWeakReference.h"           // for nsISupportsWeakReference
#include "nsIWidget.h"                  // for nsIWidget, IMEState, etc
#include "nsPIDOMWindow.h"              // for nsPIDOMWindow
#include "nsPresContext.h"              // for nsPresContext
#include "nsRange.h"                    // for nsRange
#include "nsReadableUtils.h"            // for EmptyString, ToNewCString
#include "nsString.h"                   // for nsAutoString, nsString, etc
#include "nsStringFwd.h"                // for nsAFlatString
#include "nsStyleConsts.h"              // for NS_STYLE_DIRECTION_RTL, etc
#include "nsStyleContext.h"             // for nsStyleContext
#include "nsStyleSheetTxns.h"           // for AddStyleSheetTxn, etc
#include "nsStyleStruct.h"              // for nsStyleDisplay, nsStyleText, etc
#include "nsStyleStructFwd.h"           // for nsIFrame::StyleUIReset, etc
#include "nsTextEditUtils.h"            // for nsTextEditUtils
#include "nsTextNode.h"                 // for nsTextNode
#include "nsThreadUtils.h"              // for nsRunnable
#include "nsTransactionManager.h"       // for nsTransactionManager
#include "prtime.h"                     // for PR_Now

class nsIOutputStream;
class nsIParserService;
class nsITransferable;

#ifdef DEBUG
#include "nsIDOMHTMLDocument.h"         // for nsIDOMHTMLDocument
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::widget;

// Defined in nsEditorRegistration.cpp
extern nsIParserService *sParserService;

//---------------------------------------------------------------------------
//
// nsEditor: base editor class implementation
//
//---------------------------------------------------------------------------

nsEditor::nsEditor()
:  mPlaceHolderName(nullptr)
,  mSelState(nullptr)
,  mPhonetic(nullptr)
,  mModCount(0)
,  mFlags(0)
,  mUpdateCount(0)
,  mPlaceHolderBatch(0)
,  mAction(EditAction::none)
,  mIMETextOffset(0)
,  mIMETextLength(0)
,  mDirection(eNone)
,  mDocDirtyState(-1)
,  mSpellcheckCheckboxState(eTriUnset)
,  mShouldTxnSetSelection(true)
,  mDidPreDestroy(false)
,  mDidPostCreate(false)
,  mDispatchInputEvent(true)
,  mIsInEditAction(false)
,  mHidingCaret(false)
{
}

nsEditor::~nsEditor()
{
  NS_ASSERTION(!mDocWeak || mDidPreDestroy, "Why PreDestroy hasn't been called?");

  if (mComposition) {
    mComposition->OnEditorDestroyed();
    mComposition = nullptr;
  }
  // If this editor is still hiding the caret, we need to restore it.
  HideCaret(false);
  mTxnMgr = nullptr;

  delete mPhonetic;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsEditor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsEditor)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mRootElement)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mInlineSpellChecker)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mTxnMgr)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mIMETextNode)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mActionListeners)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mEditorObservers)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocStateListeners)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mEventTarget)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mEventListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsEditor)
 nsIDocument* currentDoc =
   tmp->mRootElement ? tmp->mRootElement->GetCurrentDoc() : nullptr;
 if (currentDoc &&
     nsCCUncollectableMarker::InGeneration(cb, currentDoc->GetMarkedCCGeneration())) {
   return NS_SUCCESS_INTERRUPTED_TRAVERSE;
 }
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRootElement)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInlineSpellChecker)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTxnMgr)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIMETextNode)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mActionListeners)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEditorObservers)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocStateListeners)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventTarget)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsEditor)
 NS_INTERFACE_MAP_ENTRY(nsIPhonetic)
 NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
 NS_INTERFACE_MAP_ENTRY(nsIEditorIMESupport)
 NS_INTERFACE_MAP_ENTRY(nsIEditor)
 NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsEditor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsEditor)


NS_IMETHODIMP
nsEditor::Init(nsIDOMDocument *aDoc, nsIContent *aRoot,
               nsISelectionController *aSelCon, uint32_t aFlags,
               const nsAString& aValue)
{
  NS_PRECONDITION(aDoc, "bad arg");
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;

  // First only set flags, but other stuff shouldn't be initialized now.
  // Don't move this call after initializing mDocWeak.
  // SetFlags() can check whether it's called during initialization or not by
  // them.  Note that SetFlags() will be called by PostCreate().
#ifdef DEBUG
  nsresult rv =
#endif
  SetFlags(aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "SetFlags() failed");

  mDocWeak = do_GetWeakReference(aDoc);  // weak reference to doc
  // HTML editors currently don't have their own selection controller,
  // so they'll pass null as aSelCon, and we'll get the selection controller
  // off of the presshell.
  nsCOMPtr<nsISelectionController> selCon;
  if (aSelCon) {
    mSelConWeak = do_GetWeakReference(aSelCon);   // weak reference to selectioncontroller
    selCon = aSelCon;
  } else {
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    selCon = do_QueryInterface(presShell);
  }
  NS_ASSERTION(selCon, "Selection controller should be available at this point");

  //set up root element if we are passed one.
  if (aRoot)
    mRootElement = do_QueryInterface(aRoot);

  mUpdateCount=0;

  // If this is an editor for <input> or <textarea>, mIMETextNode is always
  // recreated with same content. Therefore, we need to forget mIMETextNode,
  // but we need to keep storing mIMETextOffset and mIMETextLength becuase
  // they are necessary to restore IME selection and replacing composing string
  // when this receives eCompositionChange event next time.
  if (mIMETextNode && !mIMETextNode->IsInComposedDoc()) {
    mIMETextNode = nullptr;
  }

  /* Show the caret */
  selCon->SetCaretReadOnly(false);
  selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);

  selCon->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);//we want to see all the selection reflected to user

  NS_POSTCONDITION(mDocWeak, "bad state");

  // Make sure that the editor will be destroyed properly
  mDidPreDestroy = false;
  // Make sure that the ediotr will be created properly
  mDidPostCreate = false;

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::PostCreate()
{
  // Synchronize some stuff for the flags.  SetFlags() will initialize
  // something by the flag difference.  This is first time of that, so, all
  // initializations must be run.  For such reason, we need to invert mFlags
  // value first.
  mFlags = ~mFlags;
  nsresult rv = SetFlags(~mFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // These operations only need to happen on the first PostCreate call
  if (!mDidPostCreate) {
    mDidPostCreate = true;

    // Set up listeners
    CreateEventListeners();
    rv = InstallEventListeners();
    NS_ENSURE_SUCCESS(rv, rv);

    // nuke the modification count, so the doc appears unmodified
    // do this before we notify listeners
    ResetModificationCount();

    // update the UI with our state
    NotifyDocumentListeners(eDocumentCreated);
    NotifyDocumentListeners(eDocumentStateChanged);
  }

  // update nsTextStateManager and caret if we have focus
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (focusedContent) {
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(focusedContent);
    if (target) {
      InitializeSelection(target);
    }

    // If the text control gets reframed during focus, Focus() would not be
    // called, so take a chance here to see if we need to spell check the text
    // control.
    nsEditorEventListener* listener =
      reinterpret_cast<nsEditorEventListener*> (mEventListener.get());
    listener->SpellCheckIfNeeded();

    IMEState newState;
    rv = GetPreferredIMEState(&newState);
    NS_ENSURE_SUCCESS(rv, NS_OK);
    nsCOMPtr<nsIContent> content = GetFocusedContentForIME();
    IMEStateManager::UpdateIMEState(newState, content, this);
  }

  // FYI: This call might cause destroying this editor.
  IMEStateManager::OnEditorInitialized(this);

  return NS_OK;
}

/* virtual */
void
nsEditor::CreateEventListeners()
{
  // Don't create the handler twice
  if (!mEventListener) {
    mEventListener = new nsEditorEventListener();
  }
}

nsresult
nsEditor::InstallEventListeners()
{
  NS_ENSURE_TRUE(mDocWeak && mEventListener,
                 NS_ERROR_NOT_INITIALIZED);

  // Initialize the event target.
  nsCOMPtr<nsIContent> rootContent = GetRoot();
  NS_ENSURE_TRUE(rootContent, NS_ERROR_NOT_AVAILABLE);
  mEventTarget = do_QueryInterface(rootContent->GetParent());
  NS_ENSURE_TRUE(mEventTarget, NS_ERROR_NOT_AVAILABLE);

  nsEditorEventListener* listener =
    reinterpret_cast<nsEditorEventListener*>(mEventListener.get());
  nsresult rv = listener->Connect(this);
  if (mComposition) {
    // Restart to handle composition with new editor contents.
    mComposition->StartHandlingComposition(this);
  }
  return rv;
}

void
nsEditor::RemoveEventListeners()
{
  if (!mDocWeak || !mEventListener) {
    return;
  }
  reinterpret_cast<nsEditorEventListener*>(mEventListener.get())->Disconnect();
  if (mComposition) {
    // Even if this is called, don't release mComposition because this is
    // may be reused after reframing.
    mComposition->EndHandlingComposition(this);
  }
  mEventTarget = nullptr;
}

bool
nsEditor::GetDesiredSpellCheckState()
{
  // Check user override on this element
  if (mSpellcheckCheckboxState != eTriUnset) {
    return (mSpellcheckCheckboxState == eTriTrue);
  }

  // Check user preferences
  int32_t spellcheckLevel = Preferences::GetInt("layout.spellcheckDefault", 1);

  if (spellcheckLevel == 0) {
    return false;                    // Spellchecking forced off globally
  }

  if (!CanEnableSpellCheck()) {
    return false;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (presShell) {
    nsPresContext* context = presShell->GetPresContext();
    if (context && !context->IsDynamic()) {
      return false;
    }
  }

  // Check DOM state
  nsCOMPtr<nsIContent> content = GetExposedRoot();
  if (!content) {
    return false;
  }

  nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(content);
  if (!element) {
    return false;
  }

  if (!IsPlaintextEditor()) {
    // Some of the page content might be editable and some not, if spellcheck=
    // is explicitly set anywhere, so if there's anything editable on the page,
    // return true and let the spellchecker figure it out.
    nsCOMPtr<nsIHTMLDocument> doc = do_QueryInterface(content->GetCurrentDoc());
    return doc && doc->IsEditingOn();
  }

  bool enable;
  element->GetSpellcheck(&enable);

  return enable;
}

NS_IMETHODIMP
nsEditor::PreDestroy(bool aDestroyingFrames)
{
  if (mDidPreDestroy)
    return NS_OK;

  IMEStateManager::OnEditorDestroying(this);

  // Let spellchecker clean up its observers etc. It is important not to
  // actually free the spellchecker here, since the spellchecker could have
  // caused flush notifications, which could have gotten here if a textbox
  // is being removed. Setting the spellchecker to nullptr could free the
  // object that is still in use! It will be freed when the editor is
  // destroyed.
  if (mInlineSpellChecker)
    mInlineSpellChecker->Cleanup(aDestroyingFrames);

  // tell our listeners that the doc is going away
  NotifyDocumentListeners(eDocumentToBeDestroyed);

  // Unregister event listeners
  RemoveEventListeners();
  // If this editor is still hiding the caret, we need to restore it.
  HideCaret(false);
  mActionListeners.Clear();
  mEditorObservers.Clear();
  mDocStateListeners.Clear();
  mInlineSpellChecker = nullptr;
  mSpellcheckCheckboxState = eTriUnset;
  mRootElement = nullptr;

  mDidPreDestroy = true;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetFlags(uint32_t *aFlags)
{
  *aFlags = mFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetFlags(uint32_t aFlags)
{
  if (mFlags == aFlags) {
    return NS_OK;
  }

  bool spellcheckerWasEnabled = CanEnableSpellCheck();
  mFlags = aFlags;

  if (!mDocWeak) {
    // If we're initializing, we shouldn't do anything now.
    // SetFlags() will be called by PostCreate(),
    // we should synchronize some stuff for the flags at that time.
    return NS_OK;
  }

  // The flag change may cause the spellchecker state change
  if (CanEnableSpellCheck() != spellcheckerWasEnabled) {
    nsresult rv = SyncRealTimeSpell();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If this is called from PostCreate(), it will update the IME state if it's
  // necessary.
  if (!mDidPostCreate) {
    return NS_OK;
  }

  // Might be changing editable state, so, we need to reset current IME state
  // if we're focused and the flag change causes IME state change.
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (focusedContent) {
    IMEState newState;
    nsresult rv = GetPreferredIMEState(&newState);
    if (NS_SUCCEEDED(rv)) {
      // NOTE: When the enabled state isn't going to be modified, this method
      // is going to do nothing.
      nsCOMPtr<nsIContent> content = GetFocusedContentForIME();
      IMEStateManager::UpdateIMEState(newState, content, this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetIsSelectionEditable(bool *aIsSelectionEditable)
{
  NS_ENSURE_ARG_POINTER(aIsSelectionEditable);

  // get current selection
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // XXX we just check that the anchor node is editable at the moment
  //     we should check that all nodes in the selection are editable
  nsCOMPtr<nsINode> anchorNode = selection->GetAnchorNode();
  *aIsSelectionEditable = anchorNode && IsEditable(anchorNode);

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetIsDocumentEditable(bool *aIsDocumentEditable)
{
  NS_ENSURE_ARG_POINTER(aIsDocumentEditable);
  nsCOMPtr<nsIDocument> doc = GetDocument();
  *aIsDocumentEditable = !!doc;

  return NS_OK;
}

already_AddRefed<nsIDocument>
nsEditor::GetDocument()
{
  NS_PRECONDITION(mDocWeak, "bad state, mDocWeak weak pointer not initialized");
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  return doc.forget();
}

already_AddRefed<nsIDOMDocument>
nsEditor::GetDOMDocument()
{
  NS_PRECONDITION(mDocWeak, "bad state, mDocWeak weak pointer not initialized");
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  return doc.forget();
}

NS_IMETHODIMP
nsEditor::GetDocument(nsIDOMDocument **aDoc)
{
  *aDoc = GetDOMDocument().take();
  return *aDoc ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

already_AddRefed<nsIPresShell>
nsEditor::GetPresShell()
{
  NS_PRECONDITION(mDocWeak, "bad state, null mDocWeak");
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, nullptr);
  nsCOMPtr<nsIPresShell> ps = doc->GetShell();
  return ps.forget();
}

already_AddRefed<nsIWidget>
nsEditor::GetWidget()
{
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, nullptr);
  nsPresContext* pc = ps->GetPresContext();
  NS_ENSURE_TRUE(pc, nullptr);
  nsCOMPtr<nsIWidget> widget = pc->GetRootWidget();
  NS_ENSURE_TRUE(widget.get(), nullptr);
  return widget.forget();
}

NS_IMETHODIMP
nsEditor::GetContentsMIMEType(char * *aContentsMIMEType)
{
  NS_ENSURE_ARG_POINTER(aContentsMIMEType);
  *aContentsMIMEType = ToNewCString(mContentMIMEType);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetContentsMIMEType(const char * aContentsMIMEType)
{
  mContentMIMEType.Assign(aContentsMIMEType ? aContentsMIMEType : "");
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetSelectionController(nsISelectionController **aSel)
{
  NS_ENSURE_TRUE(aSel, NS_ERROR_NULL_POINTER);
  *aSel = nullptr; // init out param
  nsCOMPtr<nsISelectionController> selCon;
  if (mSelConWeak) {
    selCon = do_QueryReferent(mSelConWeak);
  } else {
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    selCon = do_QueryInterface(presShell);
  }
  if (!selCon) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ADDREF(*aSel = selCon);
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DeleteSelection(EDirection aAction, EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);
  return DeleteSelectionImpl(aAction, aStripWrappers);
}


NS_IMETHODIMP
nsEditor::GetSelection(nsISelection** aSelection)
{
  return GetSelection(nsISelectionController::SELECTION_NORMAL, aSelection);
}

nsresult
nsEditor::GetSelection(int16_t aSelectionType, nsISelection** aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  *aSelection = nullptr;
  nsCOMPtr<nsISelectionController> selcon;
  GetSelectionController(getter_AddRefs(selcon));
  if (!selcon) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return selcon->GetSelection(aSelectionType, aSelection);  // does an addref
}

Selection*
nsEditor::GetSelection(int16_t aSelectionType)
{
  nsCOMPtr<nsISelection> sel;
  nsresult res = GetSelection(aSelectionType, getter_AddRefs(sel));
  if (NS_FAILED(res)) {
    return nullptr;
  }

  return static_cast<Selection*>(sel.get());
}

NS_IMETHODIMP
nsEditor::DoTransaction(nsITransaction* aTxn)
{
  if (mPlaceHolderBatch && !mPlaceHolderTxn) {
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = new PlaceholderTxn();

    // save off weak reference to placeholder txn
    mPlaceHolderTxn = do_GetWeakReference(plcTxn);
    plcTxn->Init(mPlaceHolderName, mSelState, this);
    // placeholder txn took ownership of this pointer
    mSelState = nullptr;

    // QI to an nsITransaction since that's what DoTransaction() expects
    nsCOMPtr<nsITransaction> theTxn = do_QueryInterface(plcTxn);
    // we will recurse, but will not hit this case in the nested call
    DoTransaction(theTxn);

    if (mTxnMgr) {
      nsCOMPtr<nsITransaction> topTxn = mTxnMgr->PeekUndoStack();
      if (topTxn) {
        plcTxn = do_QueryInterface(topTxn);
        if (plcTxn) {
          // there is a placeholder transaction on top of the undo stack.  It
          // is either the one we just created, or an earlier one that we are
          // now merging into.  From here on out remember this placeholder
          // instead of the one we just created.
          mPlaceHolderTxn = do_GetWeakReference(plcTxn);
        }
      }
    }
  }

  if (aTxn) {
    // XXX: Why are we doing selection specific batching stuff here?
    // XXX: Most entry points into the editor have auto variables that
    // XXX: should trigger Begin/EndUpdateViewBatch() calls that will make
    // XXX: these selection batch calls no-ops.
    // XXX:
    // XXX: I suspect that this was placed here to avoid multiple
    // XXX: selection changed notifications from happening until after
    // XXX: the transaction was done. I suppose that can still happen
    // XXX: if an embedding application called DoTransaction() directly
    // XXX: to pump its own transactions through the system, but in that
    // XXX: case, wouldn't we want to use Begin/EndUpdateViewBatch() or
    // XXX: its auto equivalent nsAutoUpdateViewBatch to ensure that
    // XXX: selection listeners have access to accurate frame data?
    // XXX:
    // XXX: Note that if we did add Begin/EndUpdateViewBatch() calls
    // XXX: we will need to make sure that they are disabled during
    // XXX: the init of the editor for text widgets to avoid layout
    // XXX: re-entry during initial reflow. - kin

    // get the selection and start a batch change
    nsRefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

    selection->StartBatchChanges();

    nsresult res;
    if (mTxnMgr) {
      res = mTxnMgr->DoTransaction(aTxn);
    } else {
      res = aTxn->DoTransaction();
    }
    if (NS_SUCCEEDED(res)) {
      DoAfterDoTransaction(aTxn);
    }

    // no need to check res here, don't lose result of operation
    selection->EndBatchChanges();

    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::EnableUndo(bool aEnable)
{
  if (aEnable) {
    if (!mTxnMgr) {
      mTxnMgr = new nsTransactionManager();
    }
    mTxnMgr->SetMaxTransactionCount(-1);
  } else if (mTxnMgr) {
    // disable the transaction manager if it is enabled
    mTxnMgr->Clear();
    mTxnMgr->SetMaxTransactionCount(0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetNumberOfUndoItems(int32_t* aNumItems)
{
  *aNumItems = 0;
  return mTxnMgr ? mTxnMgr->GetNumberOfUndoItems(aNumItems) : NS_OK;
}

NS_IMETHODIMP
nsEditor::GetNumberOfRedoItems(int32_t* aNumItems)
{
  *aNumItems = 0;
  return mTxnMgr ? mTxnMgr->GetNumberOfRedoItems(aNumItems) : NS_OK;
}

NS_IMETHODIMP
nsEditor::GetTransactionManager(nsITransactionManager* *aTxnManager)
{
  NS_ENSURE_ARG_POINTER(aTxnManager);

  *aTxnManager = nullptr;
  NS_ENSURE_TRUE(mTxnMgr, NS_ERROR_FAILURE);

  NS_ADDREF(*aTxnManager = mTxnMgr);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetTransactionManager(nsITransactionManager *aTxnManager)
{
  NS_ENSURE_TRUE(aTxnManager, NS_ERROR_FAILURE);

  // nsITransactionManager is builtinclass, so this is safe
  mTxnMgr = static_cast<nsTransactionManager*>(aTxnManager);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::Undo(uint32_t aCount)
{
  ForceCompositionEnd();

  bool hasTxnMgr, hasTransaction = false;
  CanUndo(&hasTxnMgr, &hasTransaction);
  NS_ENSURE_TRUE(hasTransaction, NS_OK);

  nsAutoRules beginRulesSniffing(this, EditAction::undo, nsIEditor::eNone);

  if (!mTxnMgr) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < aCount; ++i) {
    nsresult rv = mTxnMgr->UndoTransaction();
    NS_ENSURE_SUCCESS(rv, rv);

    DoAfterUndoTransaction();
  }

  return NS_OK;
}


NS_IMETHODIMP nsEditor::CanUndo(bool *aIsEnabled, bool *aCanUndo)
{
  NS_ENSURE_TRUE(aIsEnabled && aCanUndo, NS_ERROR_NULL_POINTER);
  *aIsEnabled = !!mTxnMgr;
  if (*aIsEnabled) {
    int32_t numTxns = 0;
    mTxnMgr->GetNumberOfUndoItems(&numTxns);
    *aCanUndo = !!numTxns;
  } else {
    *aCanUndo = false;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::Redo(uint32_t aCount)
{
  bool hasTxnMgr, hasTransaction = false;
  CanRedo(&hasTxnMgr, &hasTransaction);
  NS_ENSURE_TRUE(hasTransaction, NS_OK);

  nsAutoRules beginRulesSniffing(this, EditAction::redo, nsIEditor::eNone);

  if (!mTxnMgr) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < aCount; ++i) {
    nsresult rv = mTxnMgr->RedoTransaction();
    NS_ENSURE_SUCCESS(rv, rv);

    DoAfterRedoTransaction();
  }

  return NS_OK;
}


NS_IMETHODIMP nsEditor::CanRedo(bool *aIsEnabled, bool *aCanRedo)
{
  NS_ENSURE_TRUE(aIsEnabled && aCanRedo, NS_ERROR_NULL_POINTER);

  *aIsEnabled = !!mTxnMgr;
  if (*aIsEnabled) {
    int32_t numTxns = 0;
    mTxnMgr->GetNumberOfRedoItems(&numTxns);
    *aCanRedo = !!numTxns;
  } else {
    *aCanRedo = false;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::BeginTransaction()
{
  BeginUpdateViewBatch();

  if (mTxnMgr) {
    mTxnMgr->BeginBatch(nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::EndTransaction()
{
  if (mTxnMgr) {
    mTxnMgr->EndBatch(false);
  }

  EndUpdateViewBatch();

  return NS_OK;
}


// These two routines are similar to the above, but do not use
// the transaction managers batching feature.  Instead we use
// a placeholder transaction to wrap up any further transaction
// while the batch is open.  The advantage of this is that
// placeholder transactions can later merge, if needed.  Merging
// is unavailable between transaction manager batches.

NS_IMETHODIMP
nsEditor::BeginPlaceHolderTransaction(nsIAtom *aName)
{
  NS_PRECONDITION(mPlaceHolderBatch >= 0, "negative placeholder batch count!");
  if (!mPlaceHolderBatch)
  {
    NotifyEditorObservers(eNotifyEditorObserversOfBefore);
    // time to turn on the batch
    BeginUpdateViewBatch();
    mPlaceHolderTxn = nullptr;
    mPlaceHolderName = aName;
    nsRefPtr<Selection> selection = GetSelection();
    if (selection) {
      mSelState = new nsSelectionState();
      mSelState->SaveSelection(selection);
    }
  }
  mPlaceHolderBatch++;

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::EndPlaceHolderTransaction()
{
  NS_PRECONDITION(mPlaceHolderBatch > 0, "zero or negative placeholder batch count when ending batch!");
  if (mPlaceHolderBatch == 1)
  {
    nsRefPtr<Selection> selection = GetSelection();

    // By making the assumption that no reflow happens during the calls
    // to EndUpdateViewBatch and ScrollSelectionIntoView, we are able to
    // allow the selection to cache a frame offset which is used by the
    // caret drawing code. We only enable this cache here; at other times,
    // we have no way to know whether reflow invalidates it
    // See bugs 35296 and 199412.
    if (selection) {
      selection->SetCanCacheFrameOffset(true);
    }

    {
      // Hide the caret here to avoid hiding it twice, once in EndUpdateViewBatch
      // and once in ScrollSelectionIntoView.
      nsRefPtr<nsCaret> caret;
      nsCOMPtr<nsIPresShell> presShell = GetPresShell();

      if (presShell)
        caret = presShell->GetCaret();

      // time to turn off the batch
      EndUpdateViewBatch();
      // make sure selection is in view

      // After ScrollSelectionIntoView(), the pending notifications might be
      // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
      ScrollSelectionIntoView(false);
    }

    // cached for frame offset are Not available now
    if (selection) {
      selection->SetCanCacheFrameOffset(false);
    }

    if (mSelState)
    {
      // we saved the selection state, but never got to hand it to placeholder
      // (else we ould have nulled out this pointer), so destroy it to prevent leaks.
      delete mSelState;
      mSelState = nullptr;
    }
    if (mPlaceHolderTxn)  // we might have never made a placeholder if no action took place
    {
      nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryReferent(mPlaceHolderTxn);
      if (plcTxn)
      {
        plcTxn->EndPlaceHolderBatch();
      }
      else
      {
        // in the future we will check to make sure undo is off here,
        // since that is the only known case where the placeholdertxn would disappear on us.
        // For now just removing the assert.
      }
      // notify editor observers of action but if composing, it's done by
      // compositionchange event handler.
      if (!mComposition) {
        NotifyEditorObservers(eNotifyEditorObserversOfEnd);
      }
    } else {
      NotifyEditorObservers(eNotifyEditorObserversOfCancel);
    }
  }
  mPlaceHolderBatch--;

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::ShouldTxnSetSelection(bool *aResult)
{
  NS_ENSURE_TRUE(aResult, NS_ERROR_NULL_POINTER);
  *aResult = mShouldTxnSetSelection;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetShouldTxnSetSelection(bool aShould)
{
  mShouldTxnSetSelection = aShould;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetDocumentIsEmpty(bool *aDocumentIsEmpty)
{
  *aDocumentIsEmpty = true;

  dom::Element* root = GetRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_NULL_POINTER);

  *aDocumentIsEmpty = !root->HasChildren();
  return NS_OK;
}


// XXX: the rule system should tell us which node to select all on (ie, the root, or the body)
NS_IMETHODIMP nsEditor::SelectAll()
{
  if (!mDocWeak) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);
  return SelectEntireDocument(selection);
}

NS_IMETHODIMP nsEditor::BeginningOfDocument()
{
  if (!mDocWeak) { return NS_ERROR_NOT_INITIALIZED; }

  // get the selection
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);

  // get the root element
  dom::Element* rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER);

  // find first editable thingy
  nsCOMPtr<nsINode> firstNode = GetFirstEditableNode(rootElement);
  if (!firstNode) {
    // just the root node, set selection to inside the root
    return selection->CollapseNative(rootElement, 0);
  }

  if (firstNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    // If firstNode is text, set selection to beginning of the text node.
    return selection->CollapseNative(firstNode, 0);
  }

  // Otherwise, it's a leaf node and we set the selection just in front of it.
  nsCOMPtr<nsIContent> parent = firstNode->GetParent();
  if (!parent) {
    return NS_ERROR_NULL_POINTER;
  }

  int32_t offsetInParent = parent->IndexOf(firstNode);
  return selection->CollapseNative(parent, offsetInParent);
}

NS_IMETHODIMP
nsEditor::EndOfDocument()
{
  NS_ENSURE_TRUE(mDocWeak, NS_ERROR_NOT_INITIALIZED);

  // get selection
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // get the root element
  nsINode* node = GetRoot();
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  nsINode* child = node->GetLastChild();

  while (child && IsContainer(child->AsDOMNode())) {
    node = child;
    child = node->GetLastChild();
  }

  uint32_t length = node->Length();
  return selection->CollapseNative(node, int32_t(length));
}

NS_IMETHODIMP
nsEditor::GetDocumentModified(bool *outDocModified)
{
  NS_ENSURE_TRUE(outDocModified, NS_ERROR_NULL_POINTER);

  int32_t  modCount = 0;
  GetModificationCount(&modCount);

  *outDocModified = (modCount != 0);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetDocumentCharacterSet(nsACString &characterSet)
{
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  characterSet = doc->GetDocumentCharacterSet();
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetDocumentCharacterSet(const nsACString& characterSet)
{
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  doc->SetDocumentCharacterSet(characterSet);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::Cut()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::CanCut(bool *aCanCut)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::Copy()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::CanCopy(bool *aCanCut)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::CanDelete(bool *aCanDelete)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::Paste(int32_t aSelectionType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::PasteTransferable(nsITransferable *aTransferable)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::CanPaste(int32_t aSelectionType, bool *aCanPaste)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::CanPasteTransferable(nsITransferable *aTransferable, bool *aCanPaste)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::SetAttribute(nsIDOMElement* aElement, const nsAString& aAttribute,
                       const nsAString& aValue)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIAtom> attribute = do_GetAtom(aAttribute);

  nsRefPtr<ChangeAttributeTxn> txn =
    CreateTxnForSetAttribute(*element, *attribute, aValue);
  return DoTransaction(txn);
}

NS_IMETHODIMP
nsEditor::GetAttributeValue(nsIDOMElement *aElement,
                            const nsAString & aAttribute,
                            nsAString & aResultValue,
                            bool *aResultIsSet)
{
  NS_ENSURE_TRUE(aResultIsSet, NS_ERROR_NULL_POINTER);
  *aResultIsSet = false;
  if (!aElement) {
    return NS_OK;
  }
  nsAutoString value;
  nsresult rv = aElement->GetAttribute(aAttribute, value);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!DOMStringIsNull(value)) {
    *aResultIsSet = true;
    aResultValue = value;
  }
  return rv;
}

NS_IMETHODIMP
nsEditor::RemoveAttribute(nsIDOMElement* aElement, const nsAString& aAttribute)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIAtom> attribute = do_GetAtom(aAttribute);

  nsRefPtr<ChangeAttributeTxn> txn =
    CreateTxnForRemoveAttribute(*element, *attribute);
  return DoTransaction(txn);
}


bool
nsEditor::OutputsMozDirty()
{
  // Return true for Composer (!eEditorAllowInteraction) or mail
  // (eEditorMailMask), but false for webpages.
  return !(mFlags & nsIPlaintextEditor::eEditorAllowInteraction) ||
          (mFlags & nsIPlaintextEditor::eEditorMailMask);
}


NS_IMETHODIMP
nsEditor::MarkNodeDirty(nsIDOMNode* aNode)
{
  // Mark the node dirty, but not for webpages (bug 599983)
  if (!OutputsMozDirty()) {
    return NS_OK;
  }
  nsCOMPtr<dom::Element> element = do_QueryInterface(aNode);
  if (element) {
    element->SetAttr(kNameSpaceID_None, nsGkAtoms::mozdirty,
                     EmptyString(), false);
  }
  return NS_OK;
}

NS_IMETHODIMP nsEditor::GetInlineSpellChecker(bool autoCreate,
                                              nsIInlineSpellChecker ** aInlineSpellChecker)
{
  NS_ENSURE_ARG_POINTER(aInlineSpellChecker);

  if (mDidPreDestroy) {
    // Don't allow people to get or create the spell checker once the editor
    // is going away.
    *aInlineSpellChecker = nullptr;
    return autoCreate ? NS_ERROR_NOT_AVAILABLE : NS_OK;
  }

  // We don't want to show the spell checking UI if there are no spell check dictionaries available.
  bool canSpell = mozInlineSpellChecker::CanEnableInlineSpellChecking();
  if (!canSpell) {
    *aInlineSpellChecker = nullptr;
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (!mInlineSpellChecker && autoCreate) {
    mInlineSpellChecker = do_CreateInstance(MOZ_INLINESPELLCHECKER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mInlineSpellChecker) {
    rv = mInlineSpellChecker->Init(this);
    if (NS_FAILED(rv))
      mInlineSpellChecker = nullptr;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_IF_ADDREF(*aInlineSpellChecker = mInlineSpellChecker);

  return NS_OK;
}

NS_IMETHODIMP nsEditor::SyncRealTimeSpell()
{
  bool enable = GetDesiredSpellCheckState();

  // Initializes mInlineSpellChecker
  nsCOMPtr<nsIInlineSpellChecker> spellChecker;
  GetInlineSpellChecker(enable, getter_AddRefs(spellChecker));

  if (mInlineSpellChecker) {
    // We might have a mInlineSpellChecker even if there are no dictionaries
    // available since we don't destroy the mInlineSpellChecker when the last
    // dictionariy is removed, but in that case spellChecker is null
    mInlineSpellChecker->SetEnableRealTimeSpell(enable && spellChecker);
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditor::SetSpellcheckUserOverride(bool enable)
{
  mSpellcheckCheckboxState = enable ? eTriTrue : eTriFalse;

  return SyncRealTimeSpell();
}

NS_IMETHODIMP
nsEditor::CreateNode(const nsAString& aTag,
                     nsIDOMNode* aParent,
                     int32_t aPosition,
                     nsIDOMNode** aNewNode)
{
  nsCOMPtr<nsIAtom> tag = do_GetAtom(aTag);
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  NS_ENSURE_STATE(parent);
  *aNewNode = GetAsDOMNode(CreateNode(tag, parent, aPosition).take());
  NS_ENSURE_STATE(*aNewNode);
  return NS_OK;
}

already_AddRefed<Element>
nsEditor::CreateNode(nsIAtom* aTag,
                     nsINode* aParent,
                     int32_t aPosition)
{
  MOZ_ASSERT(aTag && aParent);

  nsAutoRules beginRulesSniffing(this, EditAction::createNode, nsIEditor::eNext);

  for (auto& listener : mActionListeners) {
    listener->WillCreateNode(nsDependentAtomString(aTag),
                             GetAsDOMNode(aParent), aPosition);
  }

  nsCOMPtr<Element> ret;

  nsRefPtr<CreateElementTxn> txn =
    CreateTxnForCreateElement(*aTag, *aParent, aPosition);
  nsresult res = DoTransaction(txn);
  if (NS_SUCCEEDED(res)) {
    ret = txn->GetNewNode();
    MOZ_ASSERT(ret);
  }

  mRangeUpdater.SelAdjCreateNode(aParent, aPosition);

  for (auto& listener : mActionListeners) {
    listener->DidCreateNode(nsDependentAtomString(aTag), GetAsDOMNode(ret),
                            GetAsDOMNode(aParent), aPosition, res);
  }

  return ret.forget();
}


NS_IMETHODIMP
nsEditor::InsertNode(nsIDOMNode* aNode, nsIDOMNode* aParent, int32_t aPosition)
{
  nsCOMPtr<nsIContent> node = do_QueryInterface(aNode);
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(node && parent, NS_ERROR_NULL_POINTER);

  return InsertNode(*node, *parent, aPosition);
}

nsresult
nsEditor::InsertNode(nsIContent& aNode, nsINode& aParent, int32_t aPosition)
{
  nsAutoRules beginRulesSniffing(this, EditAction::insertNode, nsIEditor::eNext);

  for (auto& listener : mActionListeners) {
    listener->WillInsertNode(aNode.AsDOMNode(), aParent.AsDOMNode(),
                             aPosition);
  }

  nsRefPtr<InsertNodeTxn> txn = CreateTxnForInsertNode(aNode, aParent,
                                                       aPosition);
  nsresult res = DoTransaction(txn);

  mRangeUpdater.SelAdjInsertNode(aParent.AsDOMNode(), aPosition);

  for (auto& listener : mActionListeners) {
    listener->DidInsertNode(aNode.AsDOMNode(), aParent.AsDOMNode(), aPosition,
                            res);
  }

  return res;
}


NS_IMETHODIMP
nsEditor::SplitNode(nsIDOMNode* aNode,
                    int32_t aOffset,
                    nsIDOMNode** aNewLeftNode)
{
  nsCOMPtr<nsIContent> node = do_QueryInterface(aNode);
  NS_ENSURE_STATE(node);
  ErrorResult rv;
  nsCOMPtr<nsIContent> newNode = SplitNode(*node, aOffset, rv);
  *aNewLeftNode = GetAsDOMNode(newNode.forget().take());
  return rv.StealNSResult();
}

nsIContent*
nsEditor::SplitNode(nsIContent& aNode, int32_t aOffset, ErrorResult& aResult)
{
  nsAutoRules beginRulesSniffing(this, EditAction::splitNode,
                                 nsIEditor::eNext);

  for (auto& listener : mActionListeners) {
    listener->WillSplitNode(aNode.AsDOMNode(), aOffset);
  }

  nsRefPtr<SplitNodeTxn> txn = CreateTxnForSplitNode(aNode, aOffset);
  aResult = DoTransaction(txn);

  nsCOMPtr<nsIContent> newNode = aResult.Failed() ? nullptr
                                                  : txn->GetNewNode();

  mRangeUpdater.SelAdjSplitNode(aNode, aOffset, newNode);

  nsresult result = aResult.StealNSResult();
  for (auto& listener : mActionListeners) {
    listener->DidSplitNode(aNode.AsDOMNode(), aOffset, GetAsDOMNode(newNode),
                           result);
  }
  // Note: result might be a success code, so we can't use Throw() to
  // set it on aResult.
  aResult = result;

  return newNode;
}


NS_IMETHODIMP
nsEditor::JoinNodes(nsIDOMNode* aLeftNode,
                    nsIDOMNode* aRightNode,
                    nsIDOMNode*)
{
  nsCOMPtr<nsINode> leftNode = do_QueryInterface(aLeftNode);
  nsCOMPtr<nsINode> rightNode = do_QueryInterface(aRightNode);
  NS_ENSURE_STATE(leftNode && rightNode && leftNode->GetParentNode());
  return JoinNodes(*leftNode, *rightNode);
}

nsresult
nsEditor::JoinNodes(nsINode& aLeftNode, nsINode& aRightNode)
{
  nsCOMPtr<nsINode> parent = aLeftNode.GetParentNode();
  MOZ_ASSERT(parent);

  nsAutoRules beginRulesSniffing(this, EditAction::joinNode,
                                 nsIEditor::ePrevious);

  // Remember some values; later used for saved selection updating.
  // Find the offset between the nodes to be joined.
  int32_t offset = parent->IndexOf(&aRightNode);
  // Find the number of children of the lefthand node
  uint32_t oldLeftNodeLen = aLeftNode.Length();

  for (auto& listener : mActionListeners) {
    listener->WillJoinNodes(aLeftNode.AsDOMNode(), aRightNode.AsDOMNode(),
                            parent->AsDOMNode());
  }

  nsresult result;
  nsRefPtr<JoinNodeTxn> txn = CreateTxnForJoinNode(aLeftNode, aRightNode);
  if (txn)  {
    result = DoTransaction(txn);
  }

  mRangeUpdater.SelAdjJoinNodes(aLeftNode, aRightNode, *parent, offset,
                                (int32_t)oldLeftNodeLen);

  for (auto& listener : mActionListeners) {
    listener->DidJoinNodes(aLeftNode.AsDOMNode(), aRightNode.AsDOMNode(),
                           parent->AsDOMNode(), result);
  }

  return result;
}


NS_IMETHODIMP
nsEditor::DeleteNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_STATE(node);
  return DeleteNode(node);
}

nsresult
nsEditor::DeleteNode(nsINode* aNode)
{
  nsAutoRules beginRulesSniffing(this, EditAction::createNode, nsIEditor::ePrevious);

  // save node location for selection updating code.
  for (auto& listener : mActionListeners) {
    listener->WillDeleteNode(aNode->AsDOMNode());
  }

  nsRefPtr<DeleteNodeTxn> txn;
  nsresult res = CreateTxnForDeleteNode(aNode, getter_AddRefs(txn));
  if (NS_SUCCEEDED(res))  {
    res = DoTransaction(txn);
  }

  for (auto& listener : mActionListeners) {
    listener->DidDeleteNode(aNode->AsDOMNode(), res);
  }

  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////
// ReplaceContainer: replace inNode with a new node (outNode) which is contructed
//                   to be of type aNodeType.  Put inNodes children into outNode.
//                   Callers responsibility to make sure inNode's children can
//                   go in outNode.
already_AddRefed<Element>
nsEditor::ReplaceContainer(Element* aOldContainer,
                           nsIAtom* aNodeType,
                           nsIAtom* aAttribute,
                           const nsAString* aValue,
                           ECloneAttributes aCloneAttributes)
{
  MOZ_ASSERT(aOldContainer && aNodeType);

  nsCOMPtr<nsIContent> parent = aOldContainer->GetParent();
  NS_ENSURE_TRUE(parent, nullptr);

  int32_t offset = parent->IndexOf(aOldContainer);

  // create new container
  nsCOMPtr<Element> ret = CreateHTMLContent(aNodeType);
  NS_ENSURE_TRUE(ret, nullptr);

  // set attribute if needed
  nsresult res;
  if (aAttribute && aValue && aAttribute != nsGkAtoms::_empty) {
    res = ret->SetAttr(kNameSpaceID_None, aAttribute, *aValue, true);
    NS_ENSURE_SUCCESS(res, nullptr);
  }
  if (aCloneAttributes == eCloneAttributes) {
    CloneAttributes(ret, aOldContainer);
  }

  // notify our internal selection state listener
  // (Note: A nsAutoSelectionReset object must be created
  //  before calling this to initialize mRangeUpdater)
  AutoReplaceContainerSelNotify selStateNotify(mRangeUpdater, aOldContainer,
                                               ret);
  {
    nsAutoTxnsConserveSelection conserveSelection(this);
    while (aOldContainer->HasChildren()) {
      nsCOMPtr<nsIContent> child = aOldContainer->GetFirstChild();

      res = DeleteNode(child);
      NS_ENSURE_SUCCESS(res, nullptr);

      res = InsertNode(*child, *ret, -1);
      NS_ENSURE_SUCCESS(res, nullptr);
    }
  }

  // insert new container into tree
  res = InsertNode(*ret, *parent, offset);
  NS_ENSURE_SUCCESS(res, nullptr);

  // delete old container
  res = DeleteNode(aOldContainer);
  NS_ENSURE_SUCCESS(res, nullptr);

  return ret.forget();
}

///////////////////////////////////////////////////////////////////////////////
// RemoveContainer: remove inNode, reparenting its children (if any) into the
//                  parent of inNode
//
nsresult
nsEditor::RemoveContainer(nsIContent* aNode)
{
  MOZ_ASSERT(aNode);

  nsCOMPtr<nsINode> parent = aNode->GetParentNode();
  NS_ENSURE_STATE(parent);

  int32_t offset = parent->IndexOf(aNode);

  // Loop through the children of inNode and promote them into inNode's parent
  uint32_t nodeOrigLen = aNode->GetChildCount();

  // notify our internal selection state listener
  nsAutoRemoveContainerSelNotify selNotify(mRangeUpdater, aNode, parent,
                                           offset, nodeOrigLen);

  while (aNode->HasChildren()) {
    nsCOMPtr<nsIContent> child = aNode->GetLastChild();
    nsresult rv = DeleteNode(child);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = InsertNode(*child, *parent, offset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return DeleteNode(aNode);
}


///////////////////////////////////////////////////////////////////////////////
// InsertContainerAbove: Insert a new parent for inNode, which is contructed to
//                       be of type aNodeType.  outNode becomes a child of
//                       inNode's earlier parent.  Caller's responsibility to
//                       make sure inNode's can be child of outNode, and
//                       outNode can be child of old parent.
already_AddRefed<Element>
nsEditor::InsertContainerAbove(nsIContent* aNode,
                               nsIAtom* aNodeType,
                               nsIAtom* aAttribute,
                               const nsAString* aValue)
{
  MOZ_ASSERT(aNode && aNodeType);

  nsCOMPtr<nsIContent> parent = aNode->GetParent();
  NS_ENSURE_TRUE(parent, nullptr);
  int32_t offset = parent->IndexOf(aNode);

  // Create new container
  nsCOMPtr<Element> newContent = CreateHTMLContent(aNodeType);
  NS_ENSURE_TRUE(newContent, nullptr);

  // Set attribute if needed
  nsresult res;
  if (aAttribute && aValue && aAttribute != nsGkAtoms::_empty) {
    res = newContent->SetAttr(kNameSpaceID_None, aAttribute, *aValue, true);
    NS_ENSURE_SUCCESS(res, nullptr);
  }

  // Notify our internal selection state listener
  nsAutoInsertContainerSelNotify selNotify(mRangeUpdater);

  // Put inNode in new parent, outNode
  res = DeleteNode(aNode);
  NS_ENSURE_SUCCESS(res, nullptr);

  {
    nsAutoTxnsConserveSelection conserveSelection(this);
    res = InsertNode(*aNode, *newContent, 0);
    NS_ENSURE_SUCCESS(res, nullptr);
  }

  // Put new parent in doc
  res = InsertNode(*newContent, *parent, offset);
  NS_ENSURE_SUCCESS(res, nullptr);

  return newContent.forget();
}

///////////////////////////////////////////////////////////////////////////
// MoveNode:  move aNode to {aParent,aOffset}
nsresult
nsEditor::MoveNode(nsIContent* aNode, nsINode* aParent, int32_t aOffset)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aOffset == -1 ||
             (0 <= aOffset &&
              AssertedCast<uint32_t>(aOffset) <= aParent->Length()));

  nsCOMPtr<nsINode> oldParent = aNode->GetParentNode();
  int32_t oldOffset = oldParent ? oldParent->IndexOf(aNode) : -1;

  if (aOffset == -1) {
    // Magic value meaning "move to end of aParent"
    aOffset = AssertedCast<int32_t>(aParent->Length());
  }

  // Don't do anything if it's already in right place
  if (aParent == oldParent && aOffset == oldOffset) {
    return NS_OK;
  }

  // Notify our internal selection state listener
  nsAutoMoveNodeSelNotify selNotify(mRangeUpdater, oldParent, oldOffset,
                                    aParent, aOffset);

  // Need to adjust aOffset if we're moving aNode later in its current parent
  if (aParent == oldParent && oldOffset < aOffset) {
    // When we delete aNode, it will make the offsets after it off by one
    aOffset--;
  }

  // Hold a reference so aNode doesn't go away when we remove it (bug 772282)
  nsCOMPtr<nsINode> kungFuDeathGrip = aNode;

  nsresult rv = DeleteNode(aNode);
  NS_ENSURE_SUCCESS(rv, rv);

  return InsertNode(*aNode, *aParent, aOffset);
}


NS_IMETHODIMP
nsEditor::AddEditorObserver(nsIEditorObserver *aObserver)
{
  // we don't keep ownership of the observers.  They must
  // remove themselves as observers before they are destroyed.

  NS_ENSURE_TRUE(aObserver, NS_ERROR_NULL_POINTER);

  // Make sure the listener isn't already on the list
  if (!mEditorObservers.Contains(aObserver)) {
    mEditorObservers.AppendElement(*aObserver);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveEditorObserver(nsIEditorObserver *aObserver)
{
  NS_ENSURE_TRUE(aObserver, NS_ERROR_FAILURE);

  mEditorObservers.RemoveElement(aObserver);

  return NS_OK;
}

class EditorInputEventDispatcher : public nsRunnable
{
public:
  EditorInputEventDispatcher(nsEditor* aEditor,
                             nsIContent* aTarget,
                             bool aIsComposing)
    : mEditor(aEditor)
    , mTarget(aTarget)
    , mIsComposing(aIsComposing)
  {
  }

  NS_IMETHOD Run()
  {
    // Note that we don't need to check mDispatchInputEvent here.  We need
    // to check it only when the editor requests to dispatch the input event.

    if (!mTarget->IsInComposedDoc()) {
      return NS_OK;
    }

    nsCOMPtr<nsIPresShell> ps = mEditor->GetPresShell();
    if (!ps) {
      return NS_OK;
    }

    nsCOMPtr<nsIWidget> widget = mEditor->GetWidget();
    if (!widget) {
      return NS_OK;
    }

    // Even if the change is caused by untrusted event, we need to dispatch
    // trusted input event since it's a fact.
    InternalEditorInputEvent inputEvent(true, eEditorInput, widget);
    inputEvent.time = static_cast<uint64_t>(PR_Now() / 1000);
    inputEvent.mIsComposing = mIsComposing;
    nsEventStatus status = nsEventStatus_eIgnore;
    nsresult rv =
      ps->HandleEventWithTarget(&inputEvent, nullptr, mTarget, &status);
    NS_ENSURE_SUCCESS(rv, NS_OK); // print the warning if error
    return NS_OK;
  }

private:
  nsRefPtr<nsEditor> mEditor;
  nsCOMPtr<nsIContent> mTarget;
  bool mIsComposing;
};

void
nsEditor::NotifyEditorObservers(NotificationForEditorObservers aNotification)
{
  // Copy the observers since EditAction()s can modify mEditorObservers.
  nsTArray<mozilla::OwningNonNull<nsIEditorObserver>> observers(mEditorObservers);
  switch (aNotification) {
    case eNotifyEditorObserversOfEnd:
      mIsInEditAction = false;
      for (auto& observer : observers) {
        observer->EditAction();
      }

      if (!mDispatchInputEvent) {
        return;
      }

      FireInputEvent();
      break;
    case eNotifyEditorObserversOfBefore:
      if (NS_WARN_IF(mIsInEditAction)) {
        break;
      }
      mIsInEditAction = true;
      for (auto& observer : observers) {
        observer->BeforeEditAction();
      }
      break;
    case eNotifyEditorObserversOfCancel:
      mIsInEditAction = false;
      for (auto& observer : observers) {
        observer->CancelEditAction();
      }
      break;
    default:
      MOZ_CRASH("Handle all notifications here");
      break;
  }
}

void
nsEditor::FireInputEvent()
{
  // We don't need to dispatch multiple input events if there is a pending
  // input event.  However, it may have different event target.  If we resolved
  // this issue, we need to manage the pending events in an array.  But it's
  // overwork.  We don't need to do it for the very rare case.

  nsCOMPtr<nsIContent> target = GetInputEventTargetContent();
  NS_ENSURE_TRUE_VOID(target);

  // NOTE: Don't refer IsIMEComposing() because it returns false even before
  //       compositionend.  However, DOM Level 3 Events defines it should be
  //       true after compositionstart and before compositionend.
  nsContentUtils::AddScriptRunner(
    new EditorInputEventDispatcher(this, target, !!GetComposition()));
}

NS_IMETHODIMP
nsEditor::AddEditActionListener(nsIEditActionListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  // Make sure the listener isn't already on the list
  if (!mActionListeners.Contains(aListener)) {
    mActionListeners.AppendElement(*aListener);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveEditActionListener(nsIEditActionListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_FAILURE);

  mActionListeners.RemoveElement(aListener);

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::AddDocumentStateListener(nsIDocumentStateListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  if (!mDocStateListeners.Contains(aListener)) {
    mDocStateListeners.AppendElement(*aListener);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveDocumentStateListener(nsIDocumentStateListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  mDocStateListeners.RemoveElement(aListener);

  return NS_OK;
}


NS_IMETHODIMP nsEditor::OutputToString(const nsAString& aFormatType,
                                       uint32_t aFlags,
                                       nsAString& aOutputString)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::OutputToStream(nsIOutputStream* aOutputStream,
                         const nsAString& aFormatType,
                         const nsACString& aCharsetOverride,
                         uint32_t aFlags)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::DumpContentTree()
{
#ifdef DEBUG
  if (mRootElement) {
    mRootElement->List(stdout);
  }
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugDumpContent()
{
#ifdef DEBUG
  nsCOMPtr<nsIDOMHTMLDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMHTMLElement>bodyElem;
  doc->GetBody(getter_AddRefs(bodyElem));
  nsCOMPtr<nsIContent> content = do_QueryInterface(bodyElem);
  if (content)
    content->List();
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugUnitTests(int32_t *outNumTests, int32_t *outNumTestsFailed)
{
#ifdef DEBUG
  NS_NOTREACHED("This should never get called. Overridden by subclasses");
#endif
  return NS_OK;
}


bool
nsEditor::ArePreservingSelection()
{
  return !(mSavedSel.IsEmpty());
}

void
nsEditor::PreserveSelectionAcrossActions(Selection* aSel)
{
  mSavedSel.SaveSelection(aSel);
  mRangeUpdater.RegisterSelectionState(mSavedSel);
}

nsresult
nsEditor::RestorePreservedSelection(Selection* aSel)
{
  if (mSavedSel.IsEmpty()) return NS_ERROR_FAILURE;
  mSavedSel.RestoreSelection(aSel);
  StopPreservingSelection();
  return NS_OK;
}

void
nsEditor::StopPreservingSelection()
{
  mRangeUpdater.DropSelectionState(mSavedSel);
  mSavedSel.MakeEmpty();
}

void
nsEditor::EnsureComposition(mozilla::WidgetGUIEvent* aEvent)
{
  if (mComposition) {
    return;
  }
  // The compositionstart event must cause creating new TextComposition
  // instance at being dispatched by IMEStateManager.
  mComposition = IMEStateManager::GetTextCompositionFor(aEvent);
  if (!mComposition) {
    MOZ_CRASH("IMEStateManager doesn't return proper composition");
  }
  mComposition->StartHandlingComposition(this);
}

nsresult
nsEditor::BeginIMEComposition(WidgetCompositionEvent* aCompositionEvent)
{
  MOZ_ASSERT(!mComposition, "There is composition already");
  EnsureComposition(aCompositionEvent);
  if (mPhonetic) {
    mPhonetic->Truncate(0);
  }
  return NS_OK;
}

void
nsEditor::EndIMEComposition()
{
  NS_ENSURE_TRUE_VOID(mComposition); // nothing to do

  // commit the IME transaction..we can get at it via the transaction mgr.
  // Note that this means IME won't work without an undo stack!
  if (mTxnMgr) {
    nsCOMPtr<nsITransaction> txn = mTxnMgr->PeekUndoStack();
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryInterface(txn);
    if (plcTxn) {
      DebugOnly<nsresult> rv = plcTxn->Commit();
      NS_ASSERTION(NS_SUCCEEDED(rv),
                   "nsIAbsorbingTransaction::Commit() failed");
    }
  }

  // Composition string may have hidden the caret.  Therefore, we need to
  // cancel it here.
  HideCaret(false);

  /* reset the data we need to construct a transaction */
  mIMETextNode = nullptr;
  mIMETextOffset = 0;
  mIMETextLength = 0;
  mComposition->EndHandlingComposition(this);
  mComposition = nullptr;

  // notify editor observers of action
  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
}


NS_IMETHODIMP
nsEditor::GetPhonetic(nsAString& aPhonetic)
{
  if (mPhonetic)
    aPhonetic = *mPhonetic;
  else
    aPhonetic.Truncate(0);

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::ForceCompositionEnd()
{
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  if (!ps) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsPresContext* pc = ps->GetPresContext();
  if (!pc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mComposition ?
    IMEStateManager::NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, pc) : NS_OK;
}

NS_IMETHODIMP
nsEditor::GetPreferredIMEState(IMEState *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  aState->mEnabled = IMEState::ENABLED;
  aState->mOpen = IMEState::DONT_CHANGE_OPEN_STATE;

  if (IsReadonly() || IsDisabled()) {
    aState->mEnabled = IMEState::DISABLED;
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content = GetRoot();
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsIFrame* frame = content->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  switch (frame->StyleUIReset()->mIMEMode) {
    case NS_STYLE_IME_MODE_AUTO:
      if (IsPasswordEditor())
        aState->mEnabled = IMEState::PASSWORD;
      break;
    case NS_STYLE_IME_MODE_DISABLED:
      // we should use password state for |ime-mode: disabled;|.
      aState->mEnabled = IMEState::PASSWORD;
      break;
    case NS_STYLE_IME_MODE_ACTIVE:
      aState->mOpen = IMEState::OPEN;
      break;
    case NS_STYLE_IME_MODE_INACTIVE:
      aState->mOpen = IMEState::CLOSED;
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetComposing(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = IsIMEComposing();
  return NS_OK;
}


/* Non-interface, public methods */

NS_IMETHODIMP
nsEditor::GetRootElement(nsIDOMElement **aRootElement)
{
  NS_ENSURE_ARG_POINTER(aRootElement);
  NS_ENSURE_TRUE(mRootElement, NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(mRootElement);
  rootElement.forget(aRootElement);
  return NS_OK;
}


/** All editor operations which alter the doc should be prefaced
 *  with a call to StartOperation, naming the action and direction */
NS_IMETHODIMP
nsEditor::StartOperation(EditAction opID, nsIEditor::EDirection aDirection)
{
  mAction = opID;
  mDirection = aDirection;
  return NS_OK;
}


/** All editor operations which alter the doc should be followed
 *  with a call to EndOperation */
NS_IMETHODIMP
nsEditor::EndOperation()
{
  mAction = EditAction::none;
  mDirection = eNone;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::CloneAttribute(const nsAString & aAttribute,
                         nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)
{
  NS_ENSURE_TRUE(aDestNode && aSourceNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMElement> destElement = do_QueryInterface(aDestNode);
  nsCOMPtr<nsIDOMElement> sourceElement = do_QueryInterface(aSourceNode);
  NS_ENSURE_TRUE(destElement && sourceElement, NS_ERROR_NO_INTERFACE);

  nsAutoString attrValue;
  bool isAttrSet;
  nsresult rv = GetAttributeValue(sourceElement,
                                  aAttribute,
                                  attrValue,
                                  &isAttrSet);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isAttrSet)
    rv = SetAttribute(destElement, aAttribute, attrValue);
  else
    rv = RemoveAttribute(destElement, aAttribute);

  return rv;
}

// Objects must be DOM elements
NS_IMETHODIMP
nsEditor::CloneAttributes(nsIDOMNode* aDest, nsIDOMNode* aSource)
{
  NS_ENSURE_TRUE(aDest && aSource, NS_ERROR_NULL_POINTER);

  nsCOMPtr<Element> dest = do_QueryInterface(aDest);
  nsCOMPtr<Element> source = do_QueryInterface(aSource);
  NS_ENSURE_TRUE(dest && source, NS_ERROR_NO_INTERFACE);

  CloneAttributes(dest, source);

  return NS_OK;
}

void
nsEditor::CloneAttributes(Element* aDest, Element* aSource)
{
  MOZ_ASSERT(aDest && aSource);

  nsAutoEditBatch beginBatching(this);

  // Use transaction system for undo only if destination is already in the
  // document
  NS_ENSURE_TRUE(GetRoot(), );
  bool destInBody = GetRoot()->Contains(aDest);

  // Clear existing attributes
  nsRefPtr<nsDOMAttributeMap> destAttributes = aDest->Attributes();
  while (nsRefPtr<Attr> attr = destAttributes->Item(0)) {
    if (destInBody) {
      RemoveAttribute(static_cast<nsIDOMElement*>(GetAsDOMNode(aDest)),
                      attr->NodeName());
    } else {
      ErrorResult ignored;
      aDest->RemoveAttribute(attr->NodeName(), ignored);
    }
  }

  // Set just the attributes that the source element has
  nsRefPtr<nsDOMAttributeMap> sourceAttributes = aSource->Attributes();
  uint32_t sourceCount = sourceAttributes->Length();
  for (uint32_t i = 0; i < sourceCount; i++) {
    nsRefPtr<Attr> attr = sourceAttributes->Item(i);
    nsAutoString value;
    attr->GetValue(value);
    if (destInBody) {
      SetAttributeOrEquivalent(static_cast<nsIDOMElement*>(GetAsDOMNode(aDest)),
                               attr->NodeName(), value, false);
    } else {
      // The element is not inserted in the document yet, we don't want to put
      // a transaction on the UndoStack
      SetAttributeOrEquivalent(static_cast<nsIDOMElement*>(GetAsDOMNode(aDest)),
                               attr->NodeName(), value, true);
    }
  }
}


NS_IMETHODIMP nsEditor::ScrollSelectionIntoView(bool aScrollToAnchor)
{
  nsCOMPtr<nsISelectionController> selCon;
  if (NS_SUCCEEDED(GetSelectionController(getter_AddRefs(selCon))) && selCon)
  {
    int16_t region = nsISelectionController::SELECTION_FOCUS_REGION;

    if (aScrollToAnchor)
      region = nsISelectionController::SELECTION_ANCHOR_REGION;

    selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
      region, nsISelectionController::SCROLL_OVERFLOW_HIDDEN);
  }

  return NS_OK;
}

void
nsEditor::FindBetterInsertionPoint(nsCOMPtr<nsINode>& aNode,
                                   int32_t& aOffset)
{
  if (aNode->IsNodeOfType(nsINode::eTEXT)) {
    // There is no "better" insertion point.
    return;
  }

  if (!IsPlaintextEditor()) {
    // We cannot find "better" insertion point in HTML editor.
    // WARNING: When you add some code to find better node in HTML editor,
    //          you need to call this before calling InsertTextImpl() in
    //          nsHTMLEditRules.
    return;
  }

  nsCOMPtr<nsINode> node = aNode;
  int32_t offset = aOffset;

  nsCOMPtr<nsINode> root = GetRoot();
  if (aNode == root) {
    // In some cases, aNode is the anonymous DIV, and offset is 0.  To avoid
    // injecting unneeded text nodes, we first look to see if we have one
    // available.  In that case, we'll just adjust node and offset accordingly.
    if (offset == 0 && node->HasChildren() &&
        node->GetFirstChild()->IsNodeOfType(nsINode::eTEXT)) {
      aNode = node->GetFirstChild();
      aOffset = 0;
      return;
    }

    // In some other cases, aNode is the anonymous DIV, and offset points to the
    // terminating mozBR.  In that case, we'll adjust aInOutNode and
    // aInOutOffset to the preceding text node, if any.
    if (offset > 0 && node->GetChildAt(offset - 1) &&
        node->GetChildAt(offset - 1)->IsNodeOfType(nsINode::eTEXT)) {
      NS_ENSURE_TRUE_VOID(node->Length() <= INT32_MAX);
      aNode = node->GetChildAt(offset - 1);
      aOffset = static_cast<int32_t>(aNode->Length());
      return;
    }
  }

  // Sometimes, aNode is the mozBR element itself.  In that case, we'll adjust
  // the insertion point to the previous text node, if one exists, or to the
  // parent anonymous DIV.
  if (nsTextEditUtils::IsMozBR(node) && !offset) {
    if (node->GetPreviousSibling() &&
        node->GetPreviousSibling()->IsNodeOfType(nsINode::eTEXT)) {
      NS_ENSURE_TRUE_VOID(node->Length() <= INT32_MAX);
      aNode = node->GetPreviousSibling();
      aOffset = static_cast<int32_t>(aNode->Length());
      return;
    }

    if (node->GetParentNode() && node->GetParentNode() == root) {
      aNode = node->GetParentNode();
      aOffset = 0;
      return;
    }
  }
}

nsresult
nsEditor::InsertTextImpl(const nsAString& aStringToInsert,
                         nsCOMPtr<nsINode>* aInOutNode,
                         int32_t* aInOutOffset,
                         nsIDocument* aDoc)
{
  // NOTE: caller *must* have already used nsAutoTxnsConserveSelection
  // stack-based class to turn off txn selection updating.  Caller also turned
  // on rules sniffing if desired.

  NS_ENSURE_TRUE(aInOutNode && *aInOutNode && aInOutOffset && aDoc,
                 NS_ERROR_NULL_POINTER);

  if (!ShouldHandleIMEComposition() && aStringToInsert.IsEmpty()) {
    return NS_OK;
  }

  // This method doesn't support over INT32_MAX length text since aInOutOffset
  // is int32_t*.
  CheckedInt<int32_t> lengthToInsert(aStringToInsert.Length());
  NS_ENSURE_TRUE(lengthToInsert.isValid(), NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsINode> node = *aInOutNode;
  int32_t offset = *aInOutOffset;

  // In some cases, the node may be the anonymous div elemnt or a mozBR
  // element.  Let's try to look for better insertion point in the nearest
  // text node if there is.
  FindBetterInsertionPoint(node, offset);

  nsresult res;
  if (ShouldHandleIMEComposition()) {
    CheckedInt<int32_t> newOffset;
    if (!node->IsNodeOfType(nsINode::eTEXT)) {
      // create a text node
      nsRefPtr<nsTextNode> newNode = aDoc->CreateTextNode(EmptyString());
      // then we insert it into the dom tree
      res = InsertNode(*newNode, *node, offset);
      NS_ENSURE_SUCCESS(res, res);
      node = newNode;
      offset = 0;
      newOffset = lengthToInsert;
    } else {
      newOffset = lengthToInsert + offset;
      NS_ENSURE_TRUE(newOffset.isValid(), NS_ERROR_FAILURE);
    }
    res = InsertTextIntoTextNodeImpl(aStringToInsert, *node->GetAsText(),
                                     offset);
    NS_ENSURE_SUCCESS(res, res);
    offset = newOffset.value();
  } else {
    if (node->IsNodeOfType(nsINode::eTEXT)) {
      CheckedInt<int32_t> newOffset = lengthToInsert + offset;
      NS_ENSURE_TRUE(newOffset.isValid(), NS_ERROR_FAILURE);
      // we are inserting text into an existing text node.
      res = InsertTextIntoTextNodeImpl(aStringToInsert, *node->GetAsText(),
                                       offset);
      NS_ENSURE_SUCCESS(res, res);
      offset = newOffset.value();
    } else {
      // we are inserting text into a non-text node.  first we have to create a
      // textnode (this also populates it with the text)
      nsRefPtr<nsTextNode> newNode = aDoc->CreateTextNode(aStringToInsert);
      // then we insert it into the dom tree
      res = InsertNode(*newNode, *node, offset);
      NS_ENSURE_SUCCESS(res, res);
      node = newNode;
      offset = lengthToInsert.value();
    }
  }

  *aInOutNode = node;
  *aInOutOffset = offset;
  return NS_OK;
}


nsresult
nsEditor::InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert,
                                     Text& aTextNode,
                                     int32_t aOffset, bool aSuppressIME)
{
  nsRefPtr<EditTxn> txn;
  bool isIMETransaction = false;
  int32_t replacedOffset = 0;
  int32_t replacedLength = 0;
  // aSuppressIME is used when editor must insert text, yet this text is not
  // part of the current IME operation. Example: adjusting whitespace around an
  // IME insertion.
  if (ShouldHandleIMEComposition() && !aSuppressIME) {
    if (!mIMETextNode) {
      mIMETextNode = &aTextNode;
      mIMETextOffset = aOffset;
    }
    // Modify mPhonetic with raw text input clauses.
    const TextRangeArray* ranges = mComposition->GetRanges();
    for (uint32_t i = 0; i < (ranges ? ranges->Length() : 0); ++i) {
      const TextRange& textRange = ranges->ElementAt(i);
      if (!textRange.Length() ||
          textRange.mRangeType != NS_TEXTRANGE_RAWINPUT) {
        continue;
      }
      if (!mPhonetic) {
        mPhonetic = new nsString();
      }
      nsAutoString stringToInsert(aStringToInsert);
      stringToInsert.Mid(*mPhonetic,
                         textRange.mStartOffset, textRange.Length());
    }

    txn = CreateTxnForIMEText(aStringToInsert);
    isIMETransaction = true;
    // All characters of the composition string will be replaced with
    // aStringToInsert.  So, we need to emulate to remove the composition
    // string.
    replacedOffset = mIMETextOffset;
    replacedLength = mIMETextLength;
    mIMETextLength = aStringToInsert.Length();
  } else {
    txn = CreateTxnForInsertText(aStringToInsert, aTextNode, aOffset);
  }

  // Let listeners know what's up
  for (auto& listener : mActionListeners) {
    listener->WillInsertText(
      static_cast<nsIDOMCharacterData*>(aTextNode.AsDOMNode()), aOffset,
      aStringToInsert);
  }

  // XXX We may not need these view batches anymore.  This is handled at a
  // higher level now I believe.
  BeginUpdateViewBatch();
  nsresult res = DoTransaction(txn);
  EndUpdateViewBatch();

  if (replacedLength) {
    mRangeUpdater.SelAdjDeleteText(
      static_cast<nsIDOMCharacterData*>(aTextNode.AsDOMNode()),
      replacedOffset, replacedLength);
  }
  mRangeUpdater.SelAdjInsertText(aTextNode, aOffset, aStringToInsert);

  // let listeners know what happened
  for (auto& listener : mActionListeners) {
    listener->DidInsertText(
      static_cast<nsIDOMCharacterData*>(aTextNode.AsDOMNode()),
      aOffset, aStringToInsert, res);
  }

  // Added some cruft here for bug 43366.  Layout was crashing because we left
  // an empty text node lying around in the document.  So I delete empty text
  // nodes caused by IME.  I have to mark the IME transaction as "fixed", which
  // means that furure IME txns won't merge with it.  This is because we don't
  // want future IME txns trying to put their text into a node that is no
  // longer in the document.  This does not break undo/redo, because all these
  // txns are wrapped in a parent PlaceHolder txn, and placeholder txns are
  // already savvy to having multiple ime txns inside them.

  // Delete empty IME text node if there is one
  if (isIMETransaction && mIMETextNode) {
    uint32_t len = mIMETextNode->Length();
    if (!len) {
      DeleteNode(mIMETextNode);
      mIMETextNode = nullptr;
      static_cast<IMETextTxn*>(txn.get())->MarkFixed();
    }
  }

  return res;
}


nsresult
nsEditor::SelectEntireDocument(Selection* aSelection)
{
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(GetRoot());
  if (!rootElement) { return NS_ERROR_NOT_INITIALIZED; }

  return aSelection->SelectAllChildren(rootElement);
}


nsINode*
nsEditor::GetFirstEditableNode(nsINode* aRoot)
{
  MOZ_ASSERT(aRoot);

  nsIContent* node = GetLeftmostChild(aRoot);
  if (node && !IsEditable(node)) {
    node = GetNextNode(node, /* aEditableNode = */ true);
  }

  return (node != aRoot) ? node : nullptr;
}


NS_IMETHODIMP
nsEditor::NotifyDocumentListeners(TDocumentListenerNotification aNotificationType)
{
  if (!mDocStateListeners.Length()) {
    // Maybe there just aren't any.
    return NS_OK;
  }

  nsTArray<OwningNonNull<nsIDocumentStateListener>>
    listeners(mDocStateListeners);
  nsresult rv = NS_OK;

  switch (aNotificationType)
  {
    case eDocumentCreated:
      for (auto& listener : listeners) {
        rv = listener->NotifyDocumentCreated();
        if (NS_FAILED(rv))
          break;
      }
      break;

    case eDocumentToBeDestroyed:
      for (auto& listener : listeners) {
        rv = listener->NotifyDocumentWillBeDestroyed();
        if (NS_FAILED(rv))
          break;
      }
      break;

    case eDocumentStateChanged:
      {
        bool docIsDirty;
        rv = GetDocumentModified(&docIsDirty);
        NS_ENSURE_SUCCESS(rv, rv);

        if (static_cast<int8_t>(docIsDirty) == mDocDirtyState)
          return NS_OK;

        mDocDirtyState = docIsDirty;

        for (auto& listener : listeners) {
          rv = listener->NotifyDocumentStateChanged(mDocDirtyState);
          if (NS_FAILED(rv))
            break;
        }
      }
      break;

    default:
      NS_NOTREACHED("Unknown notification");
  }

  return rv;
}


already_AddRefed<InsertTextTxn>
nsEditor::CreateTxnForInsertText(const nsAString& aStringToInsert,
                                 Text& aTextNode, int32_t aOffset)
{
  nsRefPtr<InsertTextTxn> txn = new InsertTextTxn(aTextNode, aOffset,
                                                  aStringToInsert, *this);
  return txn.forget();
}


nsresult
nsEditor::DeleteText(nsGenericDOMDataNode& aCharData, uint32_t aOffset,
                     uint32_t aLength)
{
  nsRefPtr<DeleteTextTxn> txn =
    CreateTxnForDeleteText(aCharData, aOffset, aLength);
  NS_ENSURE_STATE(txn);

  nsAutoRules beginRulesSniffing(this, EditAction::deleteText, nsIEditor::ePrevious);

  // Let listeners know what's up
  for (auto& listener : mActionListeners) {
    listener->WillDeleteText(
        static_cast<nsIDOMCharacterData*>(GetAsDOMNode(&aCharData)), aOffset,
        aLength);
  }

  nsresult res = DoTransaction(txn);

  // Let listeners know what happened
  for (auto& listener : mActionListeners) {
    listener->DidDeleteText(
        static_cast<nsIDOMCharacterData*>(GetAsDOMNode(&aCharData)), aOffset,
        aLength, res);
  }

  return res;
}


already_AddRefed<DeleteTextTxn>
nsEditor::CreateTxnForDeleteText(nsGenericDOMDataNode& aCharData,
                                 uint32_t aOffset, uint32_t aLength)
{
  nsRefPtr<DeleteTextTxn> txn =
    new DeleteTextTxn(*this, aCharData, aOffset, aLength, &mRangeUpdater);

  nsresult res = txn->Init();
  NS_ENSURE_SUCCESS(res, nullptr);

  return txn.forget();
}

already_AddRefed<SplitNodeTxn>
nsEditor::CreateTxnForSplitNode(nsIContent& aNode, uint32_t aOffset)
{
  nsRefPtr<SplitNodeTxn> txn = new SplitNodeTxn(*this, aNode, aOffset);
  return txn.forget();
}

already_AddRefed<JoinNodeTxn>
nsEditor::CreateTxnForJoinNode(nsINode& aLeftNode, nsINode& aRightNode)
{
  nsRefPtr<JoinNodeTxn> txn = new JoinNodeTxn(*this, aLeftNode, aRightNode);

  NS_ENSURE_SUCCESS(txn->CheckValidity(), nullptr);

  return txn.forget();
}


// END nsEditor core implementation


// BEGIN nsEditor public helper methods

struct SavedRange {
  nsRefPtr<Selection> mSelection;
  nsCOMPtr<nsINode> mStartNode;
  nsCOMPtr<nsINode> mEndNode;
  int32_t mStartOffset;
  int32_t mEndOffset;
};

nsresult
nsEditor::SplitNodeImpl(nsIContent& aExistingRightNode,
                        int32_t aOffset,
                        nsIContent& aNewLeftNode)
{
  // Remember all selection points.
  nsAutoTArray<SavedRange, 10> savedRanges;
  for (size_t i = 0; i < nsISelectionController::NUM_SELECTIONTYPES - 1; ++i) {
    SelectionType type(1 << i);
    SavedRange range;
    range.mSelection = GetSelection(type);
    if (type == nsISelectionController::SELECTION_NORMAL) {
      NS_ENSURE_TRUE(range.mSelection, NS_ERROR_NULL_POINTER);
    } else if (!range.mSelection) {
      // For non-normal selections, skip over the non-existing ones.
      continue;
    }

    for (uint32_t j = 0; j < range.mSelection->RangeCount(); ++j) {
      nsRefPtr<nsRange> r = range.mSelection->GetRangeAt(j);
      MOZ_ASSERT(r->IsPositioned());
      range.mStartNode = r->GetStartParent();
      range.mStartOffset = r->StartOffset();
      range.mEndNode = r->GetEndParent();
      range.mEndOffset = r->EndOffset();

      savedRanges.AppendElement(range);
    }
  }

  nsCOMPtr<nsINode> parent = aExistingRightNode.GetParentNode();
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  ErrorResult rv;
  parent->InsertBefore(aNewLeftNode, &aExistingRightNode, rv);
  NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());

  // Split the children between the two nodes.  At this point,
  // aExistingRightNode has all the children.  Move all the children whose
  // index is < aOffset to aNewLeftNode.
  if (aOffset < 0) {
    // This means move no children
    return NS_OK;
  }

  // If it's a text node, just shuffle around some text
  if (aExistingRightNode.GetAsText() && aNewLeftNode.GetAsText()) {
    // Fix right node
    nsAutoString leftText;
    aExistingRightNode.GetAsText()->SubstringData(0, aOffset, leftText);
    aExistingRightNode.GetAsText()->DeleteData(0, aOffset);
    // Fix left node
    aNewLeftNode.GetAsText()->SetData(leftText);
  } else {
    // Otherwise it's an interior node, so shuffle around the children. Go
    // through list backwards so deletes don't interfere with the iteration.
    nsCOMPtr<nsINodeList> childNodes = aExistingRightNode.ChildNodes();
    for (int32_t i = aOffset - 1; i >= 0; i--) {
      nsCOMPtr<nsIContent> childNode = childNodes->Item(i);
      if (childNode) {
        aExistingRightNode.RemoveChild(*childNode, rv);
        if (!rv.Failed()) {
          nsCOMPtr<nsIContent> firstChild = aNewLeftNode.GetFirstChild();
          aNewLeftNode.InsertBefore(*childNode, firstChild, rv);
        }
      }
      if (rv.Failed()) {
        break;
      }
    }
  }

  // Handle selection
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  if (ps) {
    ps->FlushPendingNotifications(Flush_Frames);
  }

  bool shouldSetSelection = GetShouldTxnSetSelection();

  nsRefPtr<Selection> previousSelection;
  for (size_t i = 0; i < savedRanges.Length(); ++i) {
    // Adjust the selection if needed.
    SavedRange& range = savedRanges[i];

    // If we have not seen the selection yet, clear all of its ranges.
    if (range.mSelection != previousSelection) {
      nsresult rv = range.mSelection->RemoveAllRanges();
      NS_ENSURE_SUCCESS(rv, rv);
      previousSelection = range.mSelection;
    }

    if (shouldSetSelection &&
        range.mSelection->Type() ==
          nsISelectionController::SELECTION_NORMAL) {
      // If the editor should adjust the selection, don't bother restoring
      // the ranges for the normal selection here.
      continue;
    }

    // Split the selection into existing node and new node.
    if (range.mStartNode == &aExistingRightNode) {
      if (range.mStartOffset < aOffset) {
        range.mStartNode = &aNewLeftNode;
      } else {
        range.mStartOffset -= aOffset;
      }
    }

    if (range.mEndNode == &aExistingRightNode) {
      if (range.mEndOffset < aOffset) {
        range.mEndNode = &aNewLeftNode;
      } else {
        range.mEndOffset -= aOffset;
      }
    }

    nsRefPtr<nsRange> newRange;
    nsresult rv = nsRange::CreateRange(range.mStartNode, range.mStartOffset,
                                       range.mEndNode, range.mEndOffset,
                                       getter_AddRefs(newRange));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = range.mSelection->AddRange(newRange);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (shouldSetSelection) {
    // Editor wants us to set selection at split point.
    nsRefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    selection->Collapse(&aNewLeftNode, aOffset);
  }

  return NS_OK;
}

nsresult
nsEditor::JoinNodesImpl(nsINode* aNodeToKeep,
                        nsINode* aNodeToJoin,
                        nsINode* aParent)
{
  MOZ_ASSERT(aNodeToKeep);
  MOZ_ASSERT(aNodeToJoin);
  MOZ_ASSERT(aParent);

  uint32_t firstNodeLength = aNodeToJoin->Length();

  int32_t joinOffset;
  GetNodeLocation(aNodeToJoin, &joinOffset);
  int32_t keepOffset;
  nsINode* parent = GetNodeLocation(aNodeToKeep, &keepOffset);

  // Remember all selection points.
  nsAutoTArray<SavedRange, 10> savedRanges;
  for (size_t i = 0; i < nsISelectionController::NUM_SELECTIONTYPES - 1; ++i) {
    SelectionType type(1 << i);
    SavedRange range;
    range.mSelection = GetSelection(type);
    if (type == nsISelectionController::SELECTION_NORMAL) {
      NS_ENSURE_TRUE(range.mSelection, NS_ERROR_NULL_POINTER);
    } else if (!range.mSelection) {
      // For non-normal selections, skip over the non-existing ones.
      continue;
    }

    for (uint32_t j = 0; j < range.mSelection->RangeCount(); ++j) {
      nsRefPtr<nsRange> r = range.mSelection->GetRangeAt(j);
      MOZ_ASSERT(r->IsPositioned());
      range.mStartNode = r->GetStartParent();
      range.mStartOffset = r->StartOffset();
      range.mEndNode = r->GetEndParent();
      range.mEndOffset = r->EndOffset();

      // If selection endpoint is between the nodes, remember it as being
      // in the one that is going away instead.  This simplifies later selection
      // adjustment logic at end of this method.
      if (range.mStartNode) {
        if (range.mStartNode == parent &&
            joinOffset < range.mStartOffset &&
            range.mStartOffset <= keepOffset) {
          range.mStartNode = aNodeToJoin;
          range.mStartOffset = firstNodeLength;
        }
        if (range.mEndNode == parent &&
            joinOffset < range.mEndOffset &&
            range.mEndOffset <= keepOffset) {
          range.mEndNode = aNodeToJoin;
          range.mEndOffset = firstNodeLength;
        }
      }

      savedRanges.AppendElement(range);
    }
  }

  // OK, ready to do join now.
  // If it's a text node, just shuffle around some text.
  nsCOMPtr<nsIDOMCharacterData> keepNodeAsText( do_QueryInterface(aNodeToKeep) );
  nsCOMPtr<nsIDOMCharacterData> joinNodeAsText( do_QueryInterface(aNodeToJoin) );
  if (keepNodeAsText && joinNodeAsText) {
    nsAutoString rightText;
    nsAutoString leftText;
    keepNodeAsText->GetData(rightText);
    joinNodeAsText->GetData(leftText);
    leftText += rightText;
    keepNodeAsText->SetData(leftText);
  } else {
    // Otherwise it's an interior node, so shuffle around the children.
    nsCOMPtr<nsINodeList> childNodes = aNodeToJoin->ChildNodes();
    MOZ_ASSERT(childNodes);

    // Remember the first child in aNodeToKeep, we'll insert all the children of aNodeToJoin in front of it
    // GetFirstChild returns nullptr firstNode if aNodeToKeep has no children, that's OK.
    nsCOMPtr<nsIContent> firstNode = aNodeToKeep->GetFirstChild();

    // Have to go through the list backwards to keep deletes from interfering with iteration.
    for (uint32_t i = childNodes->Length(); i > 0; --i) {
      nsCOMPtr<nsIContent> childNode = childNodes->Item(i - 1);
      if (childNode) {
        // prepend children of aNodeToJoin
        ErrorResult err;
        aNodeToKeep->InsertBefore(*childNode, firstNode, err);
        NS_ENSURE_TRUE(!err.Failed(), err.StealNSResult());
        firstNode = childNode.forget();
      }
    }
  }

  // Delete the extra node.
  ErrorResult err;
  aParent->RemoveChild(*aNodeToJoin, err);

  bool shouldSetSelection = GetShouldTxnSetSelection();

  nsRefPtr<Selection> previousSelection;
  for (size_t i = 0; i < savedRanges.Length(); ++i) {
    // And adjust the selection if needed.
    SavedRange& range = savedRanges[i];

    // If we have not seen the selection yet, clear all of its ranges.
    if (range.mSelection != previousSelection) {
      nsresult rv = range.mSelection->RemoveAllRanges();
      NS_ENSURE_SUCCESS(rv, rv);
      previousSelection = range.mSelection;
    }

    if (shouldSetSelection &&
        range.mSelection->Type() ==
          nsISelectionController::SELECTION_NORMAL) {
      // If the editor should adjust the selection, don't bother restoring
      // the ranges for the normal selection here.
      continue;
    }

    // Check to see if we joined nodes where selection starts.
    if (range.mStartNode == aNodeToJoin) {
      range.mStartNode = aNodeToKeep;
    } else if (range.mStartNode == aNodeToKeep) {
      range.mStartOffset += firstNodeLength;
    }

    // Check to see if we joined nodes where selection ends.
    if (range.mEndNode == aNodeToJoin) {
      range.mEndNode = aNodeToKeep;
    } else if (range.mEndNode == aNodeToKeep) {
      range.mEndOffset += firstNodeLength;
    }

    nsRefPtr<nsRange> newRange;
    nsresult rv = nsRange::CreateRange(range.mStartNode, range.mStartOffset,
                                       range.mEndNode, range.mEndOffset,
                                       getter_AddRefs(newRange));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = range.mSelection->AddRange(newRange);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (shouldSetSelection) {
    // Editor wants us to set selection at join point.
    nsRefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    selection->Collapse(aNodeToKeep, AssertedCast<int32_t>(firstNodeLength));
  }

  return err.StealNSResult();
}


int32_t
nsEditor::GetChildOffset(nsIDOMNode* aChild, nsIDOMNode* aParent)
{
  MOZ_ASSERT(aChild && aParent);

  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  nsCOMPtr<nsINode> child = do_QueryInterface(aChild);
  MOZ_ASSERT(parent && child);

  int32_t idx = parent->IndexOf(child);
  MOZ_ASSERT(idx != -1);
  return idx;
}

// static
already_AddRefed<nsIDOMNode>
nsEditor::GetNodeLocation(nsIDOMNode* aChild, int32_t* outOffset)
{
  MOZ_ASSERT(aChild && outOffset);
  NS_ENSURE_TRUE(aChild && outOffset, nullptr);
  *outOffset = -1;

  nsCOMPtr<nsIDOMNode> parent;

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    aChild->GetParentNode(getter_AddRefs(parent))));
  if (parent) {
    *outOffset = GetChildOffset(aChild, parent);
  }

  return parent.forget();
}

nsINode*
nsEditor::GetNodeLocation(nsINode* aChild, int32_t* aOffset)
{
  MOZ_ASSERT(aChild);
  MOZ_ASSERT(aOffset);

  nsINode* parent = aChild->GetParentNode();
  if (parent) {
    *aOffset = parent->IndexOf(aChild);
    MOZ_ASSERT(*aOffset != -1);
  } else {
    *aOffset = -1;
  }
  return parent;
}

// returns the number of things inside aNode.
// If aNode is text, returns number of characters. If not, returns number of children nodes.
nsresult
nsEditor::GetLengthOfDOMNode(nsIDOMNode *aNode, uint32_t &aCount)
{
  aCount = 0;
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  aCount = node->Length();
  return NS_OK;
}


nsIContent*
nsEditor::GetPriorNode(nsINode* aParentNode,
                       int32_t aOffset,
                       bool aEditableNode,
                       bool aNoBlockCrossing)
{
  MOZ_ASSERT(aParentNode);

  // If we are at the beginning of the node, or it is a text node, then just
  // look before it.
  if (!aOffset || aParentNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    if (aNoBlockCrossing && IsBlockNode(aParentNode)) {
      // If we aren't allowed to cross blocks, don't look before this block.
      return nullptr;
    }
    return GetPriorNode(aParentNode, aEditableNode, aNoBlockCrossing);
  }

  // else look before the child at 'aOffset'
  if (nsIContent* child = aParentNode->GetChildAt(aOffset)) {
    return GetPriorNode(child, aEditableNode, aNoBlockCrossing);
  }

  // unless there isn't one, in which case we are at the end of the node
  // and want the deep-right child.
  nsIContent* resultNode = GetRightmostChild(aParentNode, aNoBlockCrossing);
  if (!resultNode || !aEditableNode || IsEditable(resultNode)) {
    return resultNode;
  }

  // restart the search from the non-editable node we just found
  return GetPriorNode(resultNode, aEditableNode, aNoBlockCrossing);
}


nsIContent*
nsEditor::GetNextNode(nsINode* aParentNode,
                      int32_t aOffset,
                      bool aEditableNode,
                      bool aNoBlockCrossing)
{
  MOZ_ASSERT(aParentNode);

  // if aParentNode is a text node, use its location instead
  if (aParentNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    nsINode* parent = aParentNode->GetParentNode();
    NS_ENSURE_TRUE(parent, nullptr);
    aOffset = parent->IndexOf(aParentNode) + 1; // _after_ the text node
    aParentNode = parent;
  }

  // look at the child at 'aOffset'
  nsIContent* child = aParentNode->GetChildAt(aOffset);
  if (child) {
    if (aNoBlockCrossing && IsBlockNode(child)) {
      return child;
    }

    nsIContent* resultNode = GetLeftmostChild(child, aNoBlockCrossing);
    if (!resultNode) {
      return child;
    }

    if (!IsDescendantOfEditorRoot(resultNode)) {
      return nullptr;
    }

    if (!aEditableNode || IsEditable(resultNode)) {
      return resultNode;
    }

    // restart the search from the non-editable node we just found
    return GetNextNode(resultNode, aEditableNode, aNoBlockCrossing);
  }

  // unless there isn't one, in which case we are at the end of the node
  // and want the next one.
  if (aNoBlockCrossing && IsBlockNode(aParentNode)) {
    // don't cross out of parent block
    return nullptr;
  }

  return GetNextNode(aParentNode, aEditableNode, aNoBlockCrossing);
}


nsIContent*
nsEditor::GetPriorNode(nsINode* aCurrentNode, bool aEditableNode,
                       bool aNoBlockCrossing /* = false */)
{
  MOZ_ASSERT(aCurrentNode);

  if (!IsDescendantOfEditorRoot(aCurrentNode)) {
    return nullptr;
  }

  return FindNode(aCurrentNode, false, aEditableNode, aNoBlockCrossing);
}

nsIContent*
nsEditor::FindNextLeafNode(nsINode  *aCurrentNode,
                           bool      aGoForward,
                           bool      bNoBlockCrossing)
{
  // called only by GetPriorNode so we don't need to check params.
  NS_PRECONDITION(IsDescendantOfEditorRoot(aCurrentNode) &&
                  !IsEditorRoot(aCurrentNode),
                  "Bogus arguments");

  nsINode* cur = aCurrentNode;
  for (;;) {
    // if aCurrentNode has a sibling in the right direction, return
    // that sibling's closest child (or itself if it has no children)
    nsIContent* sibling =
      aGoForward ? cur->GetNextSibling() : cur->GetPreviousSibling();
    if (sibling) {
      if (bNoBlockCrossing && IsBlockNode(sibling)) {
        // don't look inside prevsib, since it is a block
        return sibling;
      }
      nsIContent *leaf =
        aGoForward ? GetLeftmostChild(sibling, bNoBlockCrossing) :
                     GetRightmostChild(sibling, bNoBlockCrossing);
      if (!leaf) {
        return sibling;
      }

      return leaf;
    }

    nsINode *parent = cur->GetParentNode();
    if (!parent) {
      return nullptr;
    }

    NS_ASSERTION(IsDescendantOfEditorRoot(parent),
                 "We started with a proper descendant of root, and should stop "
                 "if we ever hit the root, so we better have a descendant of "
                 "root now!");
    if (IsEditorRoot(parent) ||
        (bNoBlockCrossing && IsBlockNode(parent))) {
      return nullptr;
    }

    cur = parent;
  }

  NS_NOTREACHED("What part of for(;;) do you not understand?");
  return nullptr;
}

nsIContent*
nsEditor::GetNextNode(nsINode* aCurrentNode,
                      bool aEditableNode,
                      bool bNoBlockCrossing)
{
  MOZ_ASSERT(aCurrentNode);

  if (!IsDescendantOfEditorRoot(aCurrentNode)) {
    return nullptr;
  }

  return FindNode(aCurrentNode, true, aEditableNode, bNoBlockCrossing);
}

nsIContent*
nsEditor::FindNode(nsINode *aCurrentNode,
                   bool     aGoForward,
                   bool     aEditableNode,
                   bool     bNoBlockCrossing)
{
  if (IsEditorRoot(aCurrentNode)) {
    // Don't allow traversal above the root node! This helps
    // prevent us from accidentally editing browser content
    // when the editor is in a text widget.

    return nullptr;
  }

  nsCOMPtr<nsIContent> candidate =
    FindNextLeafNode(aCurrentNode, aGoForward, bNoBlockCrossing);

  if (!candidate) {
    return nullptr;
  }

  if (!aEditableNode || IsEditable(candidate)) {
    return candidate;
  }

  return FindNode(candidate, aGoForward, aEditableNode, bNoBlockCrossing);
}

nsIContent*
nsEditor::GetRightmostChild(nsINode *aCurrentNode,
                            bool     bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nullptr);
  nsIContent *cur = aCurrentNode->GetLastChild();
  if (!cur) {
    return nullptr;
  }
  for (;;) {
    if (bNoBlockCrossing && IsBlockNode(cur)) {
      return cur;
    }
    nsIContent* next = cur->GetLastChild();
    if (!next) {
      return cur;
    }
    cur = next;
  }

  NS_NOTREACHED("What part of for(;;) do you not understand?");
  return nullptr;
}

nsIContent*
nsEditor::GetLeftmostChild(nsINode *aCurrentNode,
                           bool     bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nullptr);
  nsIContent *cur = aCurrentNode->GetFirstChild();
  if (!cur) {
    return nullptr;
  }
  for (;;) {
    if (bNoBlockCrossing && IsBlockNode(cur)) {
      return cur;
    }
    nsIContent *next = cur->GetFirstChild();
    if (!next) {
      return cur;
    }
    cur = next;
  }

  NS_NOTREACHED("What part of for(;;) do you not understand?");
  return nullptr;
}

bool
nsEditor::IsBlockNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return IsBlockNode(node);
}

bool
nsEditor::IsBlockNode(nsINode* aNode)
{
  // stub to be overridden in nsHTMLEditor.
  // screwing around with the class hierarchy here in order
  // to not duplicate the code in GetNextNode/GetPrevNode
  // across both nsEditor/nsHTMLEditor.
  return false;
}

bool
nsEditor::CanContain(nsINode& aParent, nsIContent& aChild)
{
  switch (aParent.NodeType()) {
  case nsIDOMNode::ELEMENT_NODE:
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    return TagCanContain(*aParent.NodeInfo()->NameAtom(), aChild);
  }
  return false;
}

bool
nsEditor::CanContainTag(nsINode& aParent, nsIAtom& aChildTag)
{
  switch (aParent.NodeType()) {
  case nsIDOMNode::ELEMENT_NODE:
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    return TagCanContainTag(*aParent.NodeInfo()->NameAtom(), aChildTag);
  }
  return false;
}

bool
nsEditor::TagCanContain(nsIAtom& aParentTag, nsIContent& aChild)
{
  switch (aChild.NodeType()) {
  case nsIDOMNode::TEXT_NODE:
  case nsIDOMNode::ELEMENT_NODE:
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    return TagCanContainTag(aParentTag, *aChild.NodeInfo()->NameAtom());
  }
  return false;
}

bool
nsEditor::TagCanContainTag(nsIAtom& aParentTag, nsIAtom& aChildTag)
{
  return true;
}

bool
nsEditor::IsRoot(nsIDOMNode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);

  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(GetRoot());

  return inNode == rootNode;
}

bool
nsEditor::IsRoot(nsINode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);

  nsCOMPtr<nsINode> rootNode = GetRoot();

  return inNode == rootNode;
}

bool
nsEditor::IsEditorRoot(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);
  nsCOMPtr<nsINode> rootNode = GetEditorRoot();
  return aNode == rootNode;
}

bool
nsEditor::IsDescendantOfRoot(nsIDOMNode* inNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  return IsDescendantOfRoot(node);
}

bool
nsEditor::IsDescendantOfRoot(nsINode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);
  nsCOMPtr<nsIContent> root = GetRoot();
  NS_ENSURE_TRUE(root, false);

  return nsContentUtils::ContentIsDescendantOf(inNode, root);
}

bool
nsEditor::IsDescendantOfEditorRoot(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return IsDescendantOfEditorRoot(node);
}

bool
nsEditor::IsDescendantOfEditorRoot(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);
  nsCOMPtr<nsIContent> root = GetEditorRoot();
  NS_ENSURE_TRUE(root, false);

  return nsContentUtils::ContentIsDescendantOf(aNode, root);
}

bool
nsEditor::IsContainer(nsINode* aNode)
{
  return aNode ? true : false;
}

bool
nsEditor::IsContainer(nsIDOMNode* aNode)
{
  return aNode ? true : false;
}

static inline bool
IsElementVisible(dom::Element* aElement)
{
  if (aElement->GetPrimaryFrame()) {
    // It's visible, for our purposes
    return true;
  }

  nsIContent *cur = aElement;
  for (; ;) {
    // Walk up the tree looking for the nearest ancestor with a frame.
    // The state of the child right below it will determine whether
    // we might possibly have a frame or not.
    bool haveLazyBitOnChild = cur->HasFlag(NODE_NEEDS_FRAME);
    cur = cur->GetFlattenedTreeParent();
    if (!cur) {
      if (!haveLazyBitOnChild) {
        // None of our ancestors have lazy bits set, so we shouldn't
        // have a frame
        return false;
      }

      // The root has a lazy frame construction bit.  We need to check
      // our style.
      break;
    }

    if (cur->GetPrimaryFrame()) {
      if (!haveLazyBitOnChild) {
        // Our ancestor directly under |cur| doesn't have lazy bits;
        // that means we won't get a frame
        return false;
      }

      if (cur->GetPrimaryFrame()->IsLeaf()) {
        // Nothing under here will ever get frames
        return false;
      }

      // Otherwise, we might end up with a frame when that lazy bit is
      // processed.  Figure out our actual style.
      break;
    }
  }

  // Now it might be that we have no frame because we're in a
  // display:none subtree, or it might be that we're just dealing with
  // lazy frame construction and it hasn't happened yet.  Check which
  // one it is.
  nsRefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement,
                                                         nullptr, nullptr);
  if (styleContext) {
    return styleContext->StyleDisplay()->mDisplay != NS_STYLE_DISPLAY_NONE;
  }
  return false;
}

bool
nsEditor::IsEditable(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  return IsEditable(content);
}

bool
nsEditor::IsEditable(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);

  if (!aNode->IsNodeOfType(nsINode::eCONTENT) || IsMozEditorBogusNode(aNode) ||
      !IsModifiableNode(aNode)) {
    return false;
  }

  // see if it has a frame.  If so, we'll edit it.
  // special case for textnodes: frame must have width.
  if (aNode->IsElement() && !IsElementVisible(aNode->AsElement())) {
    // If the element has no frame, it's not editable.  Note that we
    // need to check IsElement() here, because some of our tests
    // rely on frameless textnodes being visible.
    return false;
  }
  switch (aNode->NodeType()) {
    case nsIDOMNode::ELEMENT_NODE:
    case nsIDOMNode::TEXT_NODE:
      return true; // element or text node; not invisible
    default:
      return false;
  }
}

bool
nsEditor::IsMozEditorBogusNode(nsINode* element)
{
  return element && element->IsElement() &&
         element->AsElement()->AttrValueIs(kNameSpaceID_None,
             kMOZEditorBogusNodeAttrAtom, kMOZEditorBogusNodeValue,
             eCaseMatters);
}

uint32_t
nsEditor::CountEditableChildren(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  uint32_t count = 0;
  for (nsIContent* child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (IsEditable(child)) {
      ++count;
    }
  }
  return count;
}

//END nsEditor static utility methods


NS_IMETHODIMP nsEditor::IncrementModificationCount(int32_t inNumMods)
{
  uint32_t oldModCount = mModCount;

  mModCount += inNumMods;

  if ((oldModCount == 0 && mModCount != 0)
   || (oldModCount != 0 && mModCount == 0))
    NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}


NS_IMETHODIMP nsEditor::GetModificationCount(int32_t *outModCount)
{
  NS_ENSURE_ARG_POINTER(outModCount);
  *outModCount = mModCount;
  return NS_OK;
}


NS_IMETHODIMP nsEditor::ResetModificationCount()
{
  bool doNotify = (mModCount != 0);

  mModCount = 0;

  if (doNotify)
    NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}

//END nsEditor Private methods



///////////////////////////////////////////////////////////////////////////
// GetTag: digs out the atom for the tag of this node
//
nsIAtom *
nsEditor::GetTag(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);

  if (!content)
  {
    NS_ASSERTION(aNode, "null node passed to nsEditor::GetTag()");

    return nullptr;
  }

  return content->NodeInfo()->NameAtom();
}


///////////////////////////////////////////////////////////////////////////
// GetTagString: digs out string for the tag of this node
//
nsresult
nsEditor::GetTagString(nsIDOMNode *aNode, nsAString& outString)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to nsEditor::GetTagString()");
    return NS_ERROR_NULL_POINTER;
  }

  nsIAtom *atom = GetTag(aNode);
  if (!atom)
  {
    return NS_ERROR_FAILURE;
  }

  atom->ToString(outString);
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// NodesSameType: do these nodes have the same tag?
//
bool
nsEditor::NodesSameType(nsIDOMNode *aNode1, nsIDOMNode *aNode2)
{
  if (!aNode1 || !aNode2) {
    NS_NOTREACHED("null node passed to nsEditor::NodesSameType()");
    return false;
  }

  nsCOMPtr<nsIContent> content1 = do_QueryInterface(aNode1);
  NS_ENSURE_TRUE(content1, false);

  nsCOMPtr<nsIContent> content2 = do_QueryInterface(aNode2);
  NS_ENSURE_TRUE(content2, false);

  return AreNodesSameType(content1, content2);
}

/* virtual */
bool
nsEditor::AreNodesSameType(nsIContent* aNode1, nsIContent* aNode2)
{
  MOZ_ASSERT(aNode1);
  MOZ_ASSERT(aNode2);
  return aNode1->NodeInfo()->NameAtom() == aNode2->NodeInfo()->NameAtom();
}


///////////////////////////////////////////////////////////////////////////
// IsTextNode: true if node of dom type text
//
bool
nsEditor::IsTextNode(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to IsTextNode()");
    return false;
  }

  uint16_t nodeType;
  aNode->GetNodeType(&nodeType);
  return (nodeType == nsIDOMNode::TEXT_NODE);
}

bool
nsEditor::IsTextNode(nsINode *aNode)
{
  return aNode->NodeType() == nsIDOMNode::TEXT_NODE;
}

///////////////////////////////////////////////////////////////////////////
// GetChildAt: returns the node at this position index in the parent
//
nsCOMPtr<nsIDOMNode>
nsEditor::GetChildAt(nsIDOMNode *aParent, int32_t aOffset)
{
  nsCOMPtr<nsIDOMNode> resultNode;

  nsCOMPtr<nsIContent> parent = do_QueryInterface(aParent);

  NS_ENSURE_TRUE(parent, resultNode);

  resultNode = do_QueryInterface(parent->GetChildAt(aOffset));

  return resultNode;
}

///////////////////////////////////////////////////////////////////////////
// GetNodeAtRangeOffsetPoint: returns the node at this position in a range,
// assuming that aParentOrNode is the node itself if it's a text node, or
// the node's parent otherwise.
//
nsCOMPtr<nsIDOMNode>
nsEditor::GetNodeAtRangeOffsetPoint(nsIDOMNode* aParentOrNode, int32_t aOffset)
{
  if (IsTextNode(aParentOrNode)) {
    return aParentOrNode;
  }
  return GetChildAt(aParentOrNode, aOffset);
}


///////////////////////////////////////////////////////////////////////////
// GetStartNodeAndOffset: returns whatever the start parent & offset is of
//                        the first range in the selection.
nsresult
nsEditor::GetStartNodeAndOffset(Selection* aSelection,
                                       nsIDOMNode **outStartNode,
                                       int32_t *outStartOffset)
{
  NS_ENSURE_TRUE(outStartNode && outStartOffset && aSelection, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> startNode;
  nsresult rv = GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode),
                                      outStartOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (startNode) {
    NS_ADDREF(*outStartNode = startNode->AsDOMNode());
  } else {
    *outStartNode = nullptr;
  }
  return NS_OK;
}

nsresult
nsEditor::GetStartNodeAndOffset(Selection* aSelection, nsINode** aStartNode,
                                int32_t* aStartOffset)
{
  MOZ_ASSERT(aSelection);
  MOZ_ASSERT(aStartNode);
  MOZ_ASSERT(aStartOffset);

  *aStartNode = nullptr;
  *aStartOffset = 0;

  NS_ENSURE_TRUE(aSelection->RangeCount(), NS_ERROR_FAILURE);

  const nsRange* range = aSelection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  NS_ENSURE_TRUE(range->IsPositioned(), NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aStartNode = range->GetStartParent());
  *aStartOffset = range->StartOffset();
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetEndNodeAndOffset: returns whatever the end parent & offset is of
//                        the first range in the selection.
nsresult
nsEditor::GetEndNodeAndOffset(Selection* aSelection,
                                       nsIDOMNode **outEndNode,
                                       int32_t *outEndOffset)
{
  NS_ENSURE_TRUE(outEndNode && outEndOffset && aSelection, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> endNode;
  nsresult rv = GetEndNodeAndOffset(aSelection, getter_AddRefs(endNode),
                                    outEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (endNode) {
    NS_ADDREF(*outEndNode = endNode->AsDOMNode());
  } else {
    *outEndNode = nullptr;
  }
  return NS_OK;
}

nsresult
nsEditor::GetEndNodeAndOffset(Selection* aSelection, nsINode** aEndNode,
                              int32_t* aEndOffset)
{
  MOZ_ASSERT(aSelection);
  MOZ_ASSERT(aEndNode);
  MOZ_ASSERT(aEndOffset);

  *aEndNode = nullptr;
  *aEndOffset = 0;

  NS_ENSURE_TRUE(aSelection->RangeCount(), NS_ERROR_FAILURE);

  const nsRange* range = aSelection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  NS_ENSURE_TRUE(range->IsPositioned(), NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aEndNode = range->GetEndParent());
  *aEndOffset = range->EndOffset();
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsPreformatted: checks the style info for the node for the preformatted
//                 text style.
nsresult
nsEditor::IsPreformatted(nsIDOMNode *aNode, bool *aResult)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);

  NS_ENSURE_TRUE(aResult && content, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  // Look at the node (and its parent if it's not an element), and grab its style context
  nsRefPtr<nsStyleContext> elementStyle;
  if (!content->IsElement()) {
    content = content->GetParent();
  }
  if (content && content->IsElement()) {
    elementStyle = nsComputedDOMStyle::GetStyleContextForElementNoFlush(content->AsElement(),
                                                                        nullptr,
                                                                        ps);
  }

  if (!elementStyle)
  {
    // Consider nodes without a style context to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no style context).
    *aResult = false;
    return NS_OK;
  }

  const nsStyleText* styleText = elementStyle->StyleText();

  *aResult = styleText->WhiteSpaceIsSignificant();
  return NS_OK;
}


/**
 * This splits a node "deeply", splitting children as appropriate.  The place
 * to split is represented by a DOM point at {splitPointParent,
 * splitPointOffset}.  That DOM point must be inside aNode, which is the node
 * to split.  We return the offset in the parent of aNode where the split
 * terminates - where you would want to insert a new element, for instance, if
 * that's why you were splitting the node.
 *
 * -1 is returned on failure, in unlikely cases like the selection being
 * unavailable or cloning the node failing.  Make sure not to use the returned
 * offset for anything without checking that it's valid!  If you're not using
 * the offset, it's okay to ignore the return value.
 */
int32_t
nsEditor::SplitNodeDeep(nsIContent& aNode,
                        nsIContent& aSplitPointParent,
                        int32_t aSplitPointOffset,
                        EmptyContainers aEmptyContainers,
                        nsIContent** aOutLeftNode,
                        nsIContent** aOutRightNode)
{
  MOZ_ASSERT(&aSplitPointParent == &aNode ||
             nsEditorUtils::IsDescendantOf(&aSplitPointParent, &aNode));
  int32_t offset = aSplitPointOffset;

  nsCOMPtr<nsIContent> leftNode, rightNode;
  OwningNonNull<nsIContent> nodeToSplit = aSplitPointParent;
  while (true) {
    // Need to insert rules code call here to do things like not split a list
    // if you are after the last <li> or before the first, etc.  For now we
    // just have some smarts about unneccessarily splitting text nodes, which
    // should be universal enough to put straight in this nsEditor routine.

    bool didSplit = false;

    if ((aEmptyContainers == EmptyContainers::yes &&
         !nodeToSplit->GetAsText()) ||
        (offset && offset != (int32_t)nodeToSplit->Length())) {
      didSplit = true;
      ErrorResult rv;
      nsCOMPtr<nsIContent> newLeftNode = SplitNode(nodeToSplit, offset, rv);
      NS_ENSURE_TRUE(!rv.Failed(), -1);

      rightNode = nodeToSplit;
      leftNode = newLeftNode;
    }

    NS_ENSURE_TRUE(nodeToSplit->GetParent(), -1);
    OwningNonNull<nsIContent> parentNode = *nodeToSplit->GetParent();

    if (!didSplit && offset) {
      // Must be "end of text node" case, we didn't split it, just move past it
      offset = parentNode->IndexOf(nodeToSplit) + 1;
      leftNode = nodeToSplit;
    } else {
      offset = parentNode->IndexOf(nodeToSplit);
      rightNode = nodeToSplit;
    }

    if (nodeToSplit == &aNode) {
      // we split all the way up to (and including) aNode; we're done
      break;
    }

    nodeToSplit = parentNode;
  }

  if (aOutLeftNode) {
    leftNode.forget(aOutLeftNode);
  }
  if (aOutRightNode) {
    rightNode.forget(aOutRightNode);
  }

  return offset;
}


/**
 * This joins two like nodes "deeply", joining children as appropriate.
 * Returns the point of the join, or (nullptr, -1) in case of error.
 */
::DOMPoint
nsEditor::JoinNodeDeep(nsIContent& aLeftNode, nsIContent& aRightNode)
{
  // While the rightmost children and their descendants of the left node match
  // the leftmost children and their descendants of the right node, join them
  // up.

  nsCOMPtr<nsIContent> leftNodeToJoin = &aLeftNode;
  nsCOMPtr<nsIContent> rightNodeToJoin = &aRightNode;
  nsCOMPtr<nsINode> parentNode = aRightNode.GetParentNode();

  ::DOMPoint ret;

  while (leftNodeToJoin && rightNodeToJoin && parentNode &&
         AreNodesSameType(leftNodeToJoin, rightNodeToJoin)) {
    uint32_t length = leftNodeToJoin->Length();

    ret.node = rightNodeToJoin;
    ret.offset = length;

    // Do the join
    nsresult res = JoinNodes(*leftNodeToJoin, *rightNodeToJoin);
    NS_ENSURE_SUCCESS(res, ::DOMPoint());

    if (parentNode->GetAsText()) {
      // We've joined all the way down to text nodes, we're done!
      return ret;
    }

    // Get new left and right nodes, and begin anew
    parentNode = rightNodeToJoin;
    leftNodeToJoin = parentNode->GetChildAt(length - 1);
    rightNodeToJoin = parentNode->GetChildAt(length);

    // Skip over non-editable nodes
    while (leftNodeToJoin && !IsEditable(leftNodeToJoin)) {
      leftNodeToJoin = leftNodeToJoin->GetPreviousSibling();
    }
    if (!leftNodeToJoin) {
      return ret;
    }

    while (rightNodeToJoin && !IsEditable(rightNodeToJoin)) {
      rightNodeToJoin = rightNodeToJoin->GetNextSibling();
    }
    if (!rightNodeToJoin) {
      return ret;
    }
  }

  return ret;
}

void
nsEditor::BeginUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount >= 0, "bad state");

  if (0 == mUpdateCount)
  {
    // Turn off selection updates and notifications.

    nsRefPtr<Selection> selection = GetSelection();

    if (selection) {
      selection->StartBatchChanges();
    }
  }

  mUpdateCount++;
}


nsresult nsEditor::EndUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount > 0, "bad state");

  if (mUpdateCount <= 0)
  {
    mUpdateCount = 0;
    return NS_ERROR_FAILURE;
  }

  mUpdateCount--;

  if (0 == mUpdateCount)
  {
    // Turn selection updating and notifications back on.

    nsRefPtr<Selection> selection = GetSelection();
    if (selection) {
      selection->EndBatchChanges();
    }
  }

  return NS_OK;
}

bool
nsEditor::GetShouldTxnSetSelection()
{
  return mShouldTxnSetSelection;
}


NS_IMETHODIMP
nsEditor::DeleteSelectionImpl(EDirection aAction,
                              EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);
  nsRefPtr<EditAggregateTxn> txn;
  nsCOMPtr<nsINode> deleteNode;
  int32_t deleteCharOffset = 0, deleteCharLength = 0;
  nsresult res = CreateTxnForDeleteSelection(aAction, getter_AddRefs(txn),
                                             getter_AddRefs(deleteNode),
                                             &deleteCharOffset,
                                             &deleteCharLength);
  nsCOMPtr<nsIDOMCharacterData> deleteCharData(do_QueryInterface(deleteNode));

  if (NS_SUCCEEDED(res))
  {
    nsAutoRules beginRulesSniffing(this, EditAction::deleteSelection, aAction);
    // Notify nsIEditActionListener::WillDelete[Selection|Text|Node]
    if (!deleteNode) {
      for (auto& listener : mActionListeners) {
        listener->WillDeleteSelection(selection);
      }
    } else if (deleteCharData) {
      for (auto& listener : mActionListeners) {
        listener->WillDeleteText(deleteCharData, deleteCharOffset, 1);
      }
    } else {
      for (auto& listener : mActionListeners) {
        listener->WillDeleteNode(deleteNode->AsDOMNode());
      }
    }

    // Delete the specified amount
    res = DoTransaction(txn);

    // Notify nsIEditActionListener::DidDelete[Selection|Text|Node]
    if (!deleteNode) {
      for (auto& listener : mActionListeners) {
        listener->DidDeleteSelection(selection);
      }
    } else if (deleteCharData) {
      for (auto& listener : mActionListeners) {
        listener->DidDeleteText(deleteCharData, deleteCharOffset, 1, res);
      }
    } else {
      for (auto& listener : mActionListeners) {
        listener->DidDeleteNode(deleteNode->AsDOMNode(), res);
      }
    }
  }

  return res;
}

already_AddRefed<Element>
nsEditor::DeleteSelectionAndCreateElement(nsIAtom& aTag)
{
  nsresult res = DeleteSelectionAndPrepareToCreateNode();
  NS_ENSURE_SUCCESS(res, nullptr);

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, nullptr);

  nsCOMPtr<nsINode> node = selection->GetAnchorNode();
  uint32_t offset = selection->AnchorOffset();

  nsCOMPtr<Element> newElement = CreateNode(&aTag, node, offset);

  // We want the selection to be just after the new node
  res = selection->Collapse(node, offset + 1);
  NS_ENSURE_SUCCESS(res, nullptr);

  return newElement.forget();
}


/* Non-interface, protected methods */

TextComposition*
nsEditor::GetComposition() const
{
  return mComposition;
}

bool
nsEditor::IsIMEComposing() const
{
  return mComposition && mComposition->IsComposing();
}

bool
nsEditor::ShouldHandleIMEComposition() const
{
  // When the editor is being reframed, the old value may be restored with
  // InsertText().  In this time, the text should be inserted as not a part
  // of the composition.
  return mComposition && mDidPostCreate;
}

nsresult
nsEditor::DeleteSelectionAndPrepareToCreateNode()
{
  nsresult res;
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  MOZ_ASSERT(selection->GetAnchorFocusRange());

  if (!selection->GetAnchorFocusRange()->Collapsed()) {
    res = DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(res, res);

    MOZ_ASSERT(selection->GetAnchorFocusRange() &&
               selection->GetAnchorFocusRange()->Collapsed(),
               "Selection not collapsed after delete");
  }

  // If the selection is a chardata node, split it if necessary and compute
  // where to put the new node
  nsCOMPtr<nsINode> node = selection->GetAnchorNode();
  MOZ_ASSERT(node, "Selection has no ranges in it");

  if (node && node->IsNodeOfType(nsINode::eDATA_NODE)) {
    NS_ASSERTION(node->GetParentNode(),
                 "It's impossible to insert into chardata with no parent -- "
                 "fix the caller");
    NS_ENSURE_STATE(node->GetParentNode());

    uint32_t offset = selection->AnchorOffset();

    if (offset == 0) {
      res = selection->Collapse(node->GetParentNode(),
                                node->GetParentNode()->IndexOf(node));
      MOZ_ASSERT(NS_SUCCEEDED(res));
      NS_ENSURE_SUCCESS(res, res);
    } else if (offset == node->Length()) {
      res = selection->Collapse(node->GetParentNode(),
                                node->GetParentNode()->IndexOf(node) + 1);
      MOZ_ASSERT(NS_SUCCEEDED(res));
      NS_ENSURE_SUCCESS(res, res);
    } else {
      nsCOMPtr<nsIDOMNode> tmp;
      res = SplitNode(node->AsDOMNode(), offset, getter_AddRefs(tmp));
      NS_ENSURE_SUCCESS(res, res);
      res = selection->Collapse(node->GetParentNode(),
                                node->GetParentNode()->IndexOf(node));
      MOZ_ASSERT(NS_SUCCEEDED(res));
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}



void
nsEditor::DoAfterDoTransaction(nsITransaction *aTxn)
{
  bool isTransientTransaction;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    aTxn->GetIsTransient(&isTransientTransaction)));

  if (!isTransientTransaction)
  {
    // we need to deal here with the case where the user saved after some
    // edits, then undid one or more times. Then, the undo count is -ve,
    // but we can't let a do take it back to zero. So we flip it up to
    // a +ve number.
    int32_t modCount;
    GetModificationCount(&modCount);
    if (modCount < 0)
      modCount = -modCount;

    // don't count transient transactions
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      IncrementModificationCount(1)));
  }
}


void
nsEditor::DoAfterUndoTransaction()
{
  // all undoable transactions are non-transient
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    IncrementModificationCount(-1)));
}

void
nsEditor::DoAfterRedoTransaction()
{
  // all redoable transactions are non-transient
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    IncrementModificationCount(1)));
}

already_AddRefed<ChangeAttributeTxn>
nsEditor::CreateTxnForSetAttribute(Element& aElement, nsIAtom& aAttribute,
                                   const nsAString& aValue)
{
  nsRefPtr<ChangeAttributeTxn> txn =
    new ChangeAttributeTxn(aElement, aAttribute, &aValue);

  return txn.forget();
}


already_AddRefed<ChangeAttributeTxn>
nsEditor::CreateTxnForRemoveAttribute(Element& aElement, nsIAtom& aAttribute)
{
  nsRefPtr<ChangeAttributeTxn> txn =
    new ChangeAttributeTxn(aElement, aAttribute, nullptr);

  return txn.forget();
}


already_AddRefed<CreateElementTxn>
nsEditor::CreateTxnForCreateElement(nsIAtom& aTag,
                                    nsINode& aParent,
                                    int32_t aPosition)
{
  nsRefPtr<CreateElementTxn> txn =
    new CreateElementTxn(*this, aTag, aParent, aPosition);

  return txn.forget();
}


already_AddRefed<InsertNodeTxn>
nsEditor::CreateTxnForInsertNode(nsIContent& aNode,
                                 nsINode& aParent,
                                 int32_t aPosition)
{
  nsRefPtr<InsertNodeTxn> txn = new InsertNodeTxn(aNode, aParent, aPosition,
                                                  *this);
  return txn.forget();
}

nsresult
nsEditor::CreateTxnForDeleteNode(nsINode* aNode, DeleteNodeTxn** aTxn)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsRefPtr<DeleteNodeTxn> txn = new DeleteNodeTxn();

  nsresult res = txn->Init(this, aNode, &mRangeUpdater);
  NS_ENSURE_SUCCESS(res, res);

  txn.forget(aTxn);
  return NS_OK;
}

already_AddRefed<IMETextTxn>
nsEditor::CreateTxnForIMEText(const nsAString& aStringToInsert)
{
  MOZ_ASSERT(mIMETextNode);
  // During handling IME composition, mComposition must have been initialized.
  // TODO: We can simplify IMETextTxn::Init() with TextComposition class.
  nsRefPtr<IMETextTxn> txn = new IMETextTxn(*mIMETextNode, mIMETextOffset,
                                            mIMETextLength,
                                            mComposition->GetRanges(),
                                            aStringToInsert, *this);
  return txn.forget();
}


NS_IMETHODIMP
nsEditor::CreateTxnForAddStyleSheet(CSSStyleSheet* aSheet, AddStyleSheetTxn* *aTxn)
{
  nsRefPtr<AddStyleSheetTxn> txn = new AddStyleSheetTxn();

  nsresult rv = txn->Init(this, aSheet);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}



NS_IMETHODIMP
nsEditor::CreateTxnForRemoveStyleSheet(CSSStyleSheet* aSheet, RemoveStyleSheetTxn* *aTxn)
{
  nsRefPtr<RemoveStyleSheetTxn> txn = new RemoveStyleSheetTxn();

  nsresult rv = txn->Init(this, aSheet);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


nsresult
nsEditor::CreateTxnForDeleteSelection(EDirection aAction,
                                      EditAggregateTxn** aTxn,
                                      nsINode** aNode,
                                      int32_t* aOffset,
                                      int32_t* aLength)
{
  MOZ_ASSERT(aTxn);
  *aTxn = nullptr;

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  // Check whether the selection is collapsed and we should do nothing:
  if (selection->Collapsed() && aAction == eNone) {
    return NS_OK;
  }

  // allocate the out-param transaction
  nsRefPtr<EditAggregateTxn> aggTxn = new EditAggregateTxn();

  for (uint32_t rangeIdx = 0; rangeIdx < selection->RangeCount(); ++rangeIdx) {
    nsRefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
    NS_ENSURE_STATE(range);

    // Same with range as with selection; if it is collapsed and action
    // is eNone, do nothing.
    if (!range->Collapsed()) {
      nsRefPtr<DeleteRangeTxn> txn = new DeleteRangeTxn();
      txn->Init(this, range, &mRangeUpdater);
      aggTxn->AppendChild(txn);
    } else if (aAction != eNone) {
      // we have an insertion point.  delete the thing in front of it or
      // behind it, depending on aAction
      nsresult res = CreateTxnForDeleteInsertionPoint(range, aAction, aggTxn,
                                                      aNode, aOffset, aLength);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  aggTxn.forget(aTxn);

  return NS_OK;
}

already_AddRefed<DeleteTextTxn>
nsEditor::CreateTxnForDeleteCharacter(nsGenericDOMDataNode& aData,
                                      uint32_t aOffset, EDirection aDirection)
{
  NS_ASSERTION(aDirection == eNext || aDirection == ePrevious,
               "Invalid direction");
  nsAutoString data;
  aData.GetData(data);
  NS_ASSERTION(data.Length(), "Trying to delete from a zero-length node");
  NS_ENSURE_TRUE(data.Length(), nullptr);

  uint32_t segOffset = aOffset, segLength = 1;
  if (aDirection == eNext) {
    if (segOffset + 1 < data.Length() &&
        NS_IS_HIGH_SURROGATE(data[segOffset]) &&
        NS_IS_LOW_SURROGATE(data[segOffset+1])) {
      // Delete both halves of the surrogate pair
      ++segLength;
    }
  } else if (aOffset > 0) {
    --segOffset;
    if (segOffset > 0 &&
      NS_IS_LOW_SURROGATE(data[segOffset]) &&
      NS_IS_HIGH_SURROGATE(data[segOffset-1])) {
      ++segLength;
      --segOffset;
    }
  } else {
    return nullptr;
  }
  return CreateTxnForDeleteText(aData, segOffset, segLength);
}

//XXX: currently, this doesn't handle edge conditions because GetNext/GetPrior
//are not implemented
nsresult
nsEditor::CreateTxnForDeleteInsertionPoint(nsRange*          aRange,
                                           EDirection        aAction,
                                           EditAggregateTxn* aTxn,
                                           nsINode**         aNode,
                                           int32_t*          aOffset,
                                           int32_t*          aLength)
{
  MOZ_ASSERT(aAction != eNone);

  nsresult res;

  // get the node and offset of the insertion point
  nsCOMPtr<nsINode> node = aRange->GetStartParent();
  NS_ENSURE_STATE(node);

  int32_t offset = aRange->StartOffset();

  // determine if the insertion point is at the beginning, middle, or end of
  // the node

  uint32_t count = node->Length();

  bool isFirst = (0 == offset);
  bool isLast  = (count == (uint32_t)offset);

  // XXX: if isFirst && isLast, then we'll need to delete the node
  //      as well as the 1 child

  // build a transaction for deleting the appropriate data
  // XXX: this has to come from rule section
  if (aAction == ePrevious && isFirst) {
    // we're backspacing from the beginning of the node.  Delete the first
    // thing to our left
    nsCOMPtr<nsIContent> priorNode = GetPriorNode(node, true);
    NS_ENSURE_STATE(priorNode);

    // there is a priorNode, so delete its last child (if chardata, delete the
    // last char). if it has no children, delete it
    if (priorNode->IsNodeOfType(nsINode::eDATA_NODE)) {
      nsRefPtr<nsGenericDOMDataNode> priorNodeAsCharData =
        static_cast<nsGenericDOMDataNode*>(priorNode.get());
      uint32_t length = priorNode->Length();
      // Bail out for empty chardata XXX: Do we want to do something else?
      NS_ENSURE_STATE(length);
      nsRefPtr<DeleteTextTxn> txn =
        CreateTxnForDeleteCharacter(*priorNodeAsCharData, length, ePrevious);
      NS_ENSURE_STATE(txn);

      *aOffset = txn->GetOffset();
      *aLength = txn->GetNumCharsToDelete();
      aTxn->AppendChild(txn);
    } else {
      // priorNode is not chardata, so tell its parent to delete it
      nsRefPtr<DeleteNodeTxn> txn;
      res = CreateTxnForDeleteNode(priorNode, getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res);

      aTxn->AppendChild(txn);
    }

    NS_ADDREF(*aNode = priorNode);

    return NS_OK;
  }

  if (aAction == eNext && isLast) {
    // we're deleting from the end of the node.  Delete the first thing to our
    // right
    nsCOMPtr<nsIContent> nextNode = GetNextNode(node, true);
    NS_ENSURE_STATE(nextNode);

    // there is a nextNode, so delete its first child (if chardata, delete the
    // first char). if it has no children, delete it
    if (nextNode->IsNodeOfType(nsINode::eDATA_NODE)) {
      nsRefPtr<nsGenericDOMDataNode> nextNodeAsCharData =
        static_cast<nsGenericDOMDataNode*>(nextNode.get());
      uint32_t length = nextNode->Length();
      // Bail out for empty chardata XXX: Do we want to do something else?
      NS_ENSURE_STATE(length);
      nsRefPtr<DeleteTextTxn> txn =
        CreateTxnForDeleteCharacter(*nextNodeAsCharData, 0, eNext);
      NS_ENSURE_STATE(txn);

      *aOffset = txn->GetOffset();
      *aLength = txn->GetNumCharsToDelete();
      aTxn->AppendChild(txn);
    } else {
      // nextNode is not chardata, so tell its parent to delete it
      nsRefPtr<DeleteNodeTxn> txn;
      res = CreateTxnForDeleteNode(nextNode, getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res);
      aTxn->AppendChild(txn);
    }

    NS_ADDREF(*aNode = nextNode);

    return NS_OK;
  }

  if (node->IsNodeOfType(nsINode::eDATA_NODE)) {
    nsRefPtr<nsGenericDOMDataNode> nodeAsCharData =
      static_cast<nsGenericDOMDataNode*>(node.get());
    // we have chardata, so delete a char at the proper offset
    nsRefPtr<DeleteTextTxn> txn = CreateTxnForDeleteCharacter(*nodeAsCharData,
                                                              offset, aAction);
    NS_ENSURE_STATE(txn);

    aTxn->AppendChild(txn);
    NS_ADDREF(*aNode = node);
    *aOffset = txn->GetOffset();
    *aLength = txn->GetNumCharsToDelete();
  } else {
    // we're either deleting a node or chardata, need to dig into the next/prev
    // node to find out
    nsCOMPtr<nsINode> selectedNode;
    if (aAction == ePrevious) {
      selectedNode = GetPriorNode(node, offset, true);
    } else if (aAction == eNext) {
      selectedNode = GetNextNode(node, offset, true);
    }

    while (selectedNode &&
           selectedNode->IsNodeOfType(nsINode::eDATA_NODE) &&
           !selectedNode->Length()) {
      // Can't delete an empty chardata node (bug 762183)
      if (aAction == ePrevious) {
        selectedNode = GetPriorNode(selectedNode, true);
      } else if (aAction == eNext) {
        selectedNode = GetNextNode(selectedNode, true);
      }
    }
    NS_ENSURE_STATE(selectedNode);

    if (selectedNode->IsNodeOfType(nsINode::eDATA_NODE)) {
      nsRefPtr<nsGenericDOMDataNode> selectedNodeAsCharData =
        static_cast<nsGenericDOMDataNode*>(selectedNode.get());
      // we are deleting from a chardata node, so do a character deletion
      uint32_t position = 0;
      if (aAction == ePrevious) {
        position = selectedNode->Length();
      }
      nsRefPtr<DeleteTextTxn> delTextTxn =
        CreateTxnForDeleteCharacter(*selectedNodeAsCharData, position,
                                    aAction);
      NS_ENSURE_TRUE(delTextTxn, NS_ERROR_NULL_POINTER);

      aTxn->AppendChild(delTextTxn);
      *aOffset = delTextTxn->GetOffset();
      *aLength = delTextTxn->GetNumCharsToDelete();
    } else {
      nsRefPtr<DeleteNodeTxn> delElementTxn;
      res = CreateTxnForDeleteNode(selectedNode, getter_AddRefs(delElementTxn));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(delElementTxn, NS_ERROR_NULL_POINTER);

      aTxn->AppendChild(delElementTxn);
    }

    NS_ADDREF(*aNode = selectedNode);
  }

  return NS_OK;
}

nsresult
nsEditor::CreateRange(nsIDOMNode *aStartParent, int32_t aStartOffset,
                      nsIDOMNode *aEndParent, int32_t aEndOffset,
                      nsRange** aRange)
{
  return nsRange::CreateRange(aStartParent, aStartOffset, aEndParent,
                              aEndOffset, aRange);
}

nsresult
nsEditor::AppendNodeToSelectionAsRange(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNode> parentNode;
  nsresult res = aNode->GetParentNode(getter_AddRefs(parentNode));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(parentNode, NS_ERROR_NULL_POINTER);

  int32_t offset = GetChildOffset(aNode, parentNode);

  nsRefPtr<nsRange> range;
  res = CreateRange(parentNode, offset, parentNode, offset+1, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

  return selection->AddRange(range);
}

nsresult nsEditor::ClearSelection()
{
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  return selection->RemoveAllRanges();
}

already_AddRefed<Element>
nsEditor::CreateHTMLContent(nsIAtom* aTag)
{
  MOZ_ASSERT(aTag);

  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (!doc) {
    return nullptr;
  }

  // XXX Wallpaper over editor bug (editor tries to create elements with an
  //     empty nodename).
  if (aTag == nsGkAtoms::_empty) {
    NS_ERROR("Don't pass an empty tag to nsEditor::CreateHTMLContent, "
             "check caller.");
    return nullptr;
  }

  return doc->CreateElem(nsDependentAtomString(aTag), nullptr,
                         kNameSpaceID_XHTML);
}

nsresult
nsEditor::SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                   const nsAString & aAttribute,
                                   const nsAString & aValue,
                                   bool aSuppressTransaction)
{
  return SetAttribute(aElement, aAttribute, aValue);
}

nsresult
nsEditor::RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                      const nsAString & aAttribute,
                                      bool aSuppressTransaction)
{
  return RemoveAttribute(aElement, aAttribute);
}

nsresult
nsEditor::HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent)
{
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/tests/test_texteditor_keyevent_handling.html
  //   * editor/libeditor/tests/test_htmleditor_keyevent_handling.html
  //
  // And also when you add new key handling, you need to change the subclass's
  // HandleKeyPressEvent()'s switch statement.

  WidgetKeyboardEvent* nativeKeyEvent =
    aKeyEvent->GetInternalNSEvent()->AsKeyboardEvent();
  NS_ENSURE_TRUE(nativeKeyEvent, NS_ERROR_UNEXPECTED);
  NS_ASSERTION(nativeKeyEvent->mMessage == eKeyPress,
               "HandleKeyPressEvent gets non-keypress event");

  // if we are readonly or disabled, then do nothing.
  if (IsReadonly() || IsDisabled()) {
    // consume backspace for disabled and readonly textfields, to prevent
    // back in history, which could be confusing to users
    if (nativeKeyEvent->keyCode == nsIDOMKeyEvent::DOM_VK_BACK_SPACE) {
      aKeyEvent->PreventDefault();
    }
    return NS_OK;
  }

  switch (nativeKeyEvent->keyCode) {
    case nsIDOMKeyEvent::DOM_VK_META:
    case nsIDOMKeyEvent::DOM_VK_WIN:
    case nsIDOMKeyEvent::DOM_VK_SHIFT:
    case nsIDOMKeyEvent::DOM_VK_CONTROL:
    case nsIDOMKeyEvent::DOM_VK_ALT:
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK;
    case nsIDOMKeyEvent::DOM_VK_BACK_SPACE:
      if (nativeKeyEvent->IsControl() || nativeKeyEvent->IsAlt() ||
          nativeKeyEvent->IsMeta() || nativeKeyEvent->IsOS()) {
        return NS_OK;
      }
      DeleteSelection(nsIEditor::ePrevious, nsIEditor::eStrip);
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK;
    case nsIDOMKeyEvent::DOM_VK_DELETE:
      // on certain platforms (such as windows) the shift key
      // modifies what delete does (cmd_cut in this case).
      // bailing here to allow the keybindings to do the cut.
      if (nativeKeyEvent->IsShift() || nativeKeyEvent->IsControl() ||
          nativeKeyEvent->IsAlt() || nativeKeyEvent->IsMeta() ||
          nativeKeyEvent->IsOS()) {
        return NS_OK;
      }
      DeleteSelection(nsIEditor::eNext, nsIEditor::eStrip);
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK;
  }
  return NS_OK;
}

nsresult
nsEditor::HandleInlineSpellCheck(EditAction action,
                                   Selection* aSelection,
                                   nsIDOMNode *previousSelectedNode,
                                   int32_t previousSelectedOffset,
                                   nsIDOMNode *aStartNode,
                                   int32_t aStartOffset,
                                   nsIDOMNode *aEndNode,
                                   int32_t aEndOffset)
{
  // Have to cast action here because this method is from an IDL
  return mInlineSpellChecker ? mInlineSpellChecker->SpellCheckAfterEditorChange(
                                 (int32_t)action, aSelection,
                                 previousSelectedNode, previousSelectedOffset,
                                 aStartNode, aStartOffset, aEndNode,
                                 aEndOffset)
                             : NS_OK;
}

already_AddRefed<nsIContent>
nsEditor::FindSelectionRoot(nsINode *aNode)
{
  nsCOMPtr<nsIContent> rootContent = GetRoot();
  return rootContent.forget();
}

nsresult
nsEditor::InitializeSelection(nsIDOMEventTarget* aFocusEventTarget)
{
  nsCOMPtr<nsINode> targetNode = do_QueryInterface(aFocusEventTarget);
  NS_ENSURE_TRUE(targetNode, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsIContent> selectionRootContent = FindSelectionRoot(targetNode);
  if (!selectionRootContent) {
    return NS_OK;
  }

  bool isTargetDoc =
    targetNode->NodeType() == nsIDOMNode::DOCUMENT_NODE &&
    targetNode->HasFlag(NODE_IS_EDITABLE);

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsISelectionController> selCon;
  nsresult rv = GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_SUCCESS(rv, rv);

  // Init the caret
  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  NS_ENSURE_TRUE(caret, NS_ERROR_UNEXPECTED);
  caret->SetIgnoreUserModify(false);
  caret->SetSelection(selection);
  selCon->SetCaretReadOnly(IsReadonly());
  selCon->SetCaretEnabled(true);

  // Init selection
  selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  selCon->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);
  selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
  // If the computed selection root isn't root content, we should set it
  // as selection ancestor limit.  However, if that is root element, it means
  // there is not limitation of the selection, then, we must set nullptr.
  // NOTE: If we set a root element to the ancestor limit, some selection
  // methods don't work fine.
  if (selectionRootContent->GetParent()) {
    selection->SetAncestorLimiter(selectionRootContent);
  } else {
    selection->SetAncestorLimiter(nullptr);
  }

  // XXX What case needs this?
  if (isTargetDoc) {
    int32_t rangeCount;
    selection->GetRangeCount(&rangeCount);
    if (rangeCount == 0) {
      BeginningOfDocument();
    }
  }

  // If there is composition when this is called, we may need to restore IME
  // selection because if the editor is reframed, this already forgot IME
  // selection and the transaction.
  if (mComposition && !mIMETextNode && mIMETextLength) {
    // We need to look for the new mIMETextNode from current selection.
    // XXX If selection is changed during reframe, this doesn't work well!
    nsRange* firstRange = selection->GetRangeAt(0);
    NS_ENSURE_TRUE(firstRange, NS_ERROR_FAILURE);
    nsCOMPtr<nsINode> startNode = firstRange->GetStartParent();
    int32_t startOffset = firstRange->StartOffset();
    FindBetterInsertionPoint(startNode, startOffset);
    Text* textNode = startNode->GetAsText();
    MOZ_ASSERT(textNode,
               "There must be text node if mIMETextLength is larger than 0");
    if (textNode) {
      MOZ_ASSERT(textNode->Length() >= mIMETextOffset + mIMETextLength,
                 "The text node must be different from the old mIMETextNode");
      IMETextTxn::SetIMESelection(*this, textNode, mIMETextOffset,
                                  mIMETextLength, mComposition->GetRanges());
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::FinalizeSelection()
{
  nsCOMPtr<nsISelectionController> selCon;
  nsresult rv = GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  selection->SetAncestorLimiter(nullptr);

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_INITIALIZED);

  selCon->SetCaretEnabled(false);

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, NS_ERROR_NOT_INITIALIZED);
  fm->UpdateCaretForCaretBrowsingMode();

  if (!HasIndependentSelection()) {
    // If this editor doesn't have an independent selection, i.e., it must
    // mean that it is an HTML editor, the selection controller is shared with
    // presShell.  So, even this editor loses focus, other part of the document
    // may still have focus.
    nsCOMPtr<nsIDocument> doc = GetDocument();
    ErrorResult ret;
    if (!doc || !doc->HasFocus(ret)) {
      // If the document already lost focus, mark the selection as disabled.
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);
    } else {
      // Otherwise, mark selection as normal because outside of a
      // contenteditable element should be selected with normal selection
      // color after here.
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
    }
  } else if (IsFormWidget() || IsPasswordEditor() ||
             IsReadonly() || IsDisabled() || IsInputFiltered()) {
    // In <input> or <textarea>, the independent selection should be hidden
    // while this editor doesn't have focus.
    selCon->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);
  } else {
    // Otherwise, although we're not sure how this case happens, the
    // independent selection should be marked as disabled.
    selCon->SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);
  }

  selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
  return NS_OK;
}

dom::Element *
nsEditor::GetRoot()
{
  if (!mRootElement)
  {
    nsCOMPtr<nsIDOMElement> root;

    // Let GetRootElement() do the work
    GetRootElement(getter_AddRefs(root));
  }

  return mRootElement;
}

dom::Element*
nsEditor::GetEditorRoot()
{
  return GetRoot();
}

Element*
nsEditor::GetExposedRoot()
{
  Element* rootElement = GetRoot();

  // For plaintext editors, we need to ask the input/textarea element directly.
  if (rootElement && rootElement->IsRootOfNativeAnonymousSubtree()) {
    rootElement = rootElement->GetParent()->AsElement();
  }

  return rootElement;
}

nsresult
nsEditor::DetermineCurrentDirection()
{
  // Get the current root direction from its frame
  nsIContent* rootElement = GetExposedRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_FAILURE);

  // If we don't have an explicit direction, determine our direction
  // from the content's direction
  if (!(mFlags & (nsIPlaintextEditor::eEditorLeftToRight |
                  nsIPlaintextEditor::eEditorRightToLeft))) {

    nsIFrame* frame = rootElement->GetPrimaryFrame();
    NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

    // Set the flag here, to enable us to use the same code path below.
    // It will be flipped before returning from the function.
    if (frame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
      mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    } else {
      mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SwitchTextDirection()
{
  // Get the current root direction from its frame
  nsIContent* rootElement = GetExposedRoot();

  nsresult rv = DetermineCurrentDirection();
  NS_ENSURE_SUCCESS(rv, rv);

  // Apply the opposite direction
  if (mFlags & nsIPlaintextEditor::eEditorRightToLeft) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorLeftToRight),
                 "Unexpected mutually exclusive flag");
    mFlags &= ~nsIPlaintextEditor::eEditorRightToLeft;
    mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("ltr"), true);
  } else if (mFlags & nsIPlaintextEditor::eEditorLeftToRight) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorRightToLeft),
                 "Unexpected mutually exclusive flag");
    mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    mFlags &= ~nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("rtl"), true);
  }

  if (NS_SUCCEEDED(rv)) {
    FireInputEvent();
  }

  return rv;
}

void
nsEditor::SwitchTextDirectionTo(uint32_t aDirection)
{
  // Get the current root direction from its frame
  nsIContent* rootElement = GetExposedRoot();

  nsresult rv = DetermineCurrentDirection();
  NS_ENSURE_SUCCESS_VOID(rv);

  // Apply the requested direction
  if (aDirection == nsIPlaintextEditor::eEditorLeftToRight &&
      (mFlags & nsIPlaintextEditor::eEditorRightToLeft)) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorLeftToRight),
                 "Unexpected mutually exclusive flag");
    mFlags &= ~nsIPlaintextEditor::eEditorRightToLeft;
    mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("ltr"), true);
  } else if (aDirection == nsIPlaintextEditor::eEditorRightToLeft &&
             (mFlags & nsIPlaintextEditor::eEditorLeftToRight)) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorRightToLeft),
                 "Unexpected mutually exclusive flag");
    mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    mFlags &= ~nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("rtl"), true);
  }

  if (NS_SUCCEEDED(rv)) {
    FireInputEvent();
  }
}

#if DEBUG_JOE
void
nsEditor::DumpNode(nsIDOMNode *aNode, int32_t indent)
{
  int32_t i;
  for (i=0; i<indent; i++)
    printf("  ");

  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
  nsCOMPtr<nsIDOMDocumentFragment> docfrag = do_QueryInterface(aNode);

  if (element || docfrag)
  {
    if (element)
    {
      nsAutoString tag;
      element->GetTagName(tag);
      printf("<%s>\n", NS_LossyConvertUTF16toASCII(tag).get());
    }
    else
    {
      printf("<document fragment>\n");
    }
    nsCOMPtr<nsIDOMNodeList> childList;
    aNode->GetChildNodes(getter_AddRefs(childList));
    NS_ENSURE_TRUE(childList, NS_ERROR_NULL_POINTER);
    uint32_t numChildren;
    childList->GetLength(&numChildren);
    nsCOMPtr<nsIDOMNode> child, tmp;
    aNode->GetFirstChild(getter_AddRefs(child));
    for (i=0; i<numChildren; i++)
    {
      DumpNode(child, indent+1);
      child->GetNextSibling(getter_AddRefs(tmp));
      child = tmp;
    }
  }
  else if (IsTextNode(aNode))
  {
    nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(aNode);
    nsAutoString str;
    textNode->GetData(str);
    nsAutoCString cstr;
    LossyCopyUTF16toASCII(str, cstr);
    cstr.ReplaceChar('\n', ' ');
    printf("<textnode> %s\n", cstr.get());
  }
}
#endif

bool
nsEditor::IsModifiableNode(nsIDOMNode *aNode)
{
  return true;
}

bool
nsEditor::IsModifiableNode(nsINode *aNode)
{
  return true;
}

already_AddRefed<nsIContent>
nsEditor::GetFocusedContent()
{
  nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return nullptr;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nullptr);

  nsCOMPtr<nsIContent> content = fm->GetFocusedContent();
  return SameCOMIdentity(content, piTarget) ? content.forget() : nullptr;
}

already_AddRefed<nsIContent>
nsEditor::GetFocusedContentForIME()
{
  return GetFocusedContent();
}

bool
nsEditor::IsActiveInDOMWindow()
{
  nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return false;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  nsPIDOMWindow* ourWindow = doc->GetWindow();
  nsCOMPtr<nsPIDOMWindow> win;
  nsIContent* content =
    nsFocusManager::GetFocusedDescendant(ourWindow, false,
                                         getter_AddRefs(win));
  return SameCOMIdentity(content, piTarget);
}

bool
nsEditor::IsAcceptableInputEvent(nsIDOMEvent* aEvent)
{
  // If the event is trusted, the event should always cause input.
  NS_ENSURE_TRUE(aEvent, false);

  WidgetEvent* widgetEvent = aEvent->GetInternalNSEvent();
  if (NS_WARN_IF(!widgetEvent)) {
    return false;
  }

  // If this is dispatched by using cordinates but this editor doesn't have
  // focus, we shouldn't handle it.
  if (widgetEvent->IsUsingCoordinates()) {
    nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
    if (!focusedContent) {
      return false;
    }
  }

  // If a composition event isn't dispatched via widget, we need to ignore them
  // since they cannot be managed by TextComposition. E.g., the event was
  // created by chrome JS.
  // Note that if we allow to handle such events, editor may be confused by
  // strange event order.
  bool needsWidget = false;
  WidgetGUIEvent* widgetGUIEvent = nullptr;
  switch (widgetEvent->mMessage) {
    case eUnidentifiedEvent:
      // If events are not created with proper event interface, their message
      // are initialized with eUnidentifiedEvent.  Let's ignore such event.
      return false;
    case eCompositionStart:
    case eCompositionEnd:
    case eCompositionUpdate:
    case eCompositionChange:
    case eCompositionCommitAsIs:
      // Don't allow composition events whose internal event are not
      // WidgetCompositionEvent.
      widgetGUIEvent = aEvent->GetInternalNSEvent()->AsCompositionEvent();
      needsWidget = true;
      break;
    default:
      break;
  }
  if (needsWidget &&
      (!widgetGUIEvent || !widgetGUIEvent->widget)) {
    return false;
  }

  // Accept all trusted events.
  if (widgetEvent->mFlags.mIsTrusted) {
    return true;
  }

  // Ignore untrusted mouse event.
  // XXX Why are we handling other untrusted input events?
  if (widgetEvent->AsMouseEventBase()) {
    return false;
  }

  // Otherwise, we shouldn't handle any input events when we're not an active
  // element of the DOM window.
  return IsActiveInDOMWindow();
}

void
nsEditor::OnFocus(nsIDOMEventTarget* aFocusEventTarget)
{
  InitializeSelection(aFocusEventTarget);
  if (mInlineSpellChecker) {
    mInlineSpellChecker->UpdateCurrentDictionary();
  }
}

NS_IMETHODIMP
nsEditor::GetSuppressDispatchingInputEvent(bool *aSuppressed)
{
  NS_ENSURE_ARG_POINTER(aSuppressed);
  *aSuppressed = !mDispatchInputEvent;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetSuppressDispatchingInputEvent(bool aSuppress)
{
  mDispatchInputEvent = !aSuppress;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetIsInEditAction(bool* aIsInEditAction)
{
  MOZ_ASSERT(aIsInEditAction, "aIsInEditAction must not be null");
  *aIsInEditAction = mIsInEditAction;
  return NS_OK;
}

int32_t
nsEditor::GetIMESelectionStartOffsetIn(nsINode* aTextNode)
{
  MOZ_ASSERT(aTextNode, "aTextNode must not be nullptr");

  nsCOMPtr<nsISelectionController> selectionController;
  nsresult rv = GetSelectionController(getter_AddRefs(selectionController));
  NS_ENSURE_SUCCESS(rv, -1);
  NS_ENSURE_TRUE(selectionController, -1);

  int32_t minOffset = INT32_MAX;
  static const SelectionType kIMESelectionTypes[] = {
    nsISelectionController::SELECTION_IME_RAWINPUT,
    nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT,
    nsISelectionController::SELECTION_IME_CONVERTEDTEXT,
    nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT
  };
  for (auto selectionType : kIMESelectionTypes) {
    nsRefPtr<Selection> selection = GetSelection(selectionType);
    if (!selection) {
      continue;
    }
    for (uint32_t i = 0; i < selection->RangeCount(); i++) {
      nsRefPtr<nsRange> range = selection->GetRangeAt(i);
      if (NS_WARN_IF(!range)) {
        continue;
      }
      if (NS_WARN_IF(range->GetStartParent() != aTextNode)) {
        // ignore the start offset...
      } else {
        MOZ_ASSERT(range->StartOffset() >= 0,
                   "start offset shouldn't be negative");
        minOffset = std::min(minOffset, range->StartOffset());
      }
      if (NS_WARN_IF(range->GetEndParent() != aTextNode)) {
        // ignore the end offset...
      } else {
        MOZ_ASSERT(range->EndOffset() >= 0,
                   "start offset shouldn't be negative");
        minOffset = std::min(minOffset, range->EndOffset());
      }
    }
  }
  return minOffset < INT32_MAX ? minOffset : -1;
}

void
nsEditor::HideCaret(bool aHide)
{
  if (mHidingCaret == aHide) {
    return;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE_VOID(presShell);
  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  NS_ENSURE_TRUE_VOID(caret);

  mHidingCaret = aHide;
  if (aHide) {
    caret->AddForceHide();
  } else {
    caret->RemoveForceHide();
  }
}
