/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditor.h"

#include "EditAction.h"
#include "EditorBase.h"
#include "EditorDOMPoint.h"
#include "EditorUtils.h"
#include "HTMLEditorEventListener.h"
#include "HTMLEditUtils.h"
#include "InsertNodeTransaction.h"
#include "JoinNodesTransaction.h"
#include "MoveNodeTransaction.h"
#include "ReplaceTextTransaction.h"
#include "SplitNodeTransaction.h"
#include "TypeInState.h"
#include "WSRunObject.h"

#include "mozilla/ComposerCommandsUpdater.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Encoding.h"      // for Encoding
#include "mozilla/IntegerRange.h"  // for IntegerRange
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/mozInlineSpellChecker.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_editor.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TextServicesDocument.h"
#include "mozilla/ToString.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/AncestorIterator.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/HTMLAnchorElement.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"

#include "nsContentList.h"
#include "nsContentUtils.h"
#include "nsCRT.h"
#include "nsElementTable.h"
#include "nsFocusManager.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsHTMLDocument.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIEditActionListener.h"
#include "nsIFrame.h"
#include "nsIPrincipal.h"
#include "nsISelectionController.h"
#include "nsIURI.h"
#include "nsIWidget.h"
#include "nsNetUtil.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"
#include "nsPIDOMWindow.h"
#include "nsStyledElement.h"
#include "nsTextFragment.h"
#include "nsUnicharUtils.h"

namespace mozilla {

using namespace dom;
using namespace widget;

using EmptyCheckOption = HTMLEditUtils::EmptyCheckOption;
using LeafNodeType = HTMLEditUtils::LeafNodeType;
using LeafNodeTypes = HTMLEditUtils::LeafNodeTypes;
using WalkTreeOption = HTMLEditUtils::WalkTreeOption;

// Some utilities to handle overloading of "A" tag for link and named anchor.
static bool IsLinkTag(const nsAtom& aTagName) {
  return &aTagName == nsGkAtoms::href;
}

static bool IsNamedAnchorTag(const nsAtom& aTagName) {
  return &aTagName == nsGkAtoms::anchor;
}

// Helper struct for DoJoinNodes() and DoSplitNode().
struct MOZ_STACK_CLASS SavedRange final {
  RefPtr<Selection> mSelection;
  nsCOMPtr<nsINode> mStartContainer;
  nsCOMPtr<nsINode> mEndContainer;
  uint32_t mStartOffset = 0;
  uint32_t mEndOffset = 0;
};

/******************************************************************************
 * HTMLEditor::AutoSelectionRestorer
 *****************************************************************************/

HTMLEditor::AutoSelectionRestorer::AutoSelectionRestorer(
    HTMLEditor& aHTMLEditor)
    : mHTMLEditor(nullptr) {
  if (aHTMLEditor.ArePreservingSelection()) {
    // We already have initialized mParentData::mSavedSelection, so this must
    // be nested call.
    return;
  }
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  mHTMLEditor = &aHTMLEditor;
  mHTMLEditor->PreserveSelectionAcrossActions();
}

HTMLEditor::AutoSelectionRestorer::~AutoSelectionRestorer() {
  if (!mHTMLEditor || !mHTMLEditor->ArePreservingSelection()) {
    return;
  }
  DebugOnly<nsresult> rvIgnored = mHTMLEditor->RestorePreservedSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EditorBase::RestorePreservedSelection() failed, but ignored");
}

void HTMLEditor::AutoSelectionRestorer::Abort() {
  if (mHTMLEditor) {
    mHTMLEditor->StopPreservingSelection();
  }
}

/******************************************************************************
 * HTMLEditor
 *****************************************************************************/

template CreateContentResult
HTMLEditor::InsertNodeIntoProperAncestorWithTransaction(
    nsIContent& aContentToInsert, const EditorDOMPoint& aPointToInsert,
    SplitAtEdges aSplitAtEdges);
template CreateElementResult
HTMLEditor::InsertNodeIntoProperAncestorWithTransaction(
    Element& aContentToInsert, const EditorDOMPoint& aPointToInsert,
    SplitAtEdges aSplitAtEdges);
template CreateTextResult
HTMLEditor::InsertNodeIntoProperAncestorWithTransaction(
    Text& aContentToInsert, const EditorDOMPoint& aPointToInsert,
    SplitAtEdges aSplitAtEdges);

HTMLEditor::InitializeInsertingElement HTMLEditor::DoNothingForNewElement =
    [](HTMLEditor&, Element&, const EditorDOMPoint&) { return NS_OK; };

HTMLEditor::HTMLEditor()
    : EditorBase(EditorBase::EditorType::HTML),
      mCRInParagraphCreatesParagraph(false),
      mIsObjectResizingEnabled(
          StaticPrefs::editor_resizing_enabled_by_default()),
      mIsResizing(false),
      mPreserveRatio(false),
      mResizedObjectIsAnImage(false),
      mIsAbsolutelyPositioningEnabled(
          StaticPrefs::editor_positioning_enabled_by_default()),
      mResizedObjectIsAbsolutelyPositioned(false),
      mGrabberClicked(false),
      mIsMoving(false),
      mSnapToGridEnabled(false),
      mIsInlineTableEditingEnabled(
          StaticPrefs::editor_inline_table_editing_enabled_by_default()),
      mOriginalX(0),
      mOriginalY(0),
      mResizedObjectX(0),
      mResizedObjectY(0),
      mResizedObjectWidth(0),
      mResizedObjectHeight(0),
      mResizedObjectMarginLeft(0),
      mResizedObjectMarginTop(0),
      mResizedObjectBorderLeft(0),
      mResizedObjectBorderTop(0),
      mXIncrementFactor(0),
      mYIncrementFactor(0),
      mWidthIncrementFactor(0),
      mHeightIncrementFactor(0),
      mInfoXIncrement(20),
      mInfoYIncrement(20),
      mPositionedObjectX(0),
      mPositionedObjectY(0),
      mPositionedObjectWidth(0),
      mPositionedObjectHeight(0),
      mPositionedObjectMarginLeft(0),
      mPositionedObjectMarginTop(0),
      mPositionedObjectBorderLeft(0),
      mPositionedObjectBorderTop(0),
      mGridSize(0),
      mDefaultParagraphSeparator(
          StaticPrefs::editor_use_div_for_default_newlines()
              ? ParagraphSeparator::div
              : ParagraphSeparator::br) {}

HTMLEditor::~HTMLEditor() {
  // Collect the data of `beforeinput` event only when it's enabled because
  // web apps should switch their behavior with feature detection with
  // checking `onbeforeinput` or `getTargetRanges`.
  if (StaticPrefs::dom_input_events_beforeinput_enabled()) {
    Telemetry::Accumulate(
        Telemetry::HTMLEDITORS_WITH_BEFOREINPUT_LISTENERS,
        MayHaveBeforeInputEventListenersForTelemetry() ? 1 : 0);
    Telemetry::Accumulate(
        Telemetry::HTMLEDITORS_OVERRIDDEN_BY_BEFOREINPUT_LISTENERS,
        mHasBeforeInputBeenCanceled ? 1 : 0);
    Telemetry::Accumulate(
        Telemetry::
            HTMLEDITORS_WITH_MUTATION_LISTENERS_WITHOUT_BEFOREINPUT_LISTENERS,
        !MayHaveBeforeInputEventListenersForTelemetry() &&
                MayHaveMutationEventListeners()
            ? 1
            : 0);
    Telemetry::Accumulate(
        Telemetry::
            HTMLEDITORS_WITH_MUTATION_OBSERVERS_WITHOUT_BEFOREINPUT_LISTENERS,
        !MayHaveBeforeInputEventListenersForTelemetry() &&
                MutationObserverHasObservedNodeForTelemetry()
            ? 1
            : 0);
  }

  mTypeInState = nullptr;

  if (mDisabledLinkHandling) {
    if (Document* doc = GetDocument()) {
      doc->SetLinkHandlingEnabled(mOldLinkHandlingEnabled);
    }
  }

  RemoveEventListeners();

  HideAnonymousEditingUIs();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLEditor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLEditor, EditorBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTypeInState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mComposerCommandsUpdater)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChangedRangeForTopLevelEditSubAction)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPaddingBRElementForEmptyEditor)
  tmp->HideAnonymousEditingUIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLEditor, EditorBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTypeInState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mComposerCommandsUpdater)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChangedRangeForTopLevelEditSubAction)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPaddingBRElementForEmptyEditor)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTopLeftHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTopHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTopRightHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLeftHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRightHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBottomLeftHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBottomHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBottomRightHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mActivatedHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResizingShadow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResizingInfo)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResizedObject)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAbsolutelyPositionedObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGrabber)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPositioningShadow)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInlineEditedCell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAddColumnBeforeButton)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRemoveColumnButton)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAddColumnAfterButton)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAddRowBeforeButton)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRemoveRowButton)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAddRowAfterButton)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(HTMLEditor, EditorBase)
NS_IMPL_RELEASE_INHERITED(HTMLEditor, EditorBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HTMLEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLObjectResizer)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLAbsPosEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLInlineTableEditor)
  NS_INTERFACE_MAP_ENTRY(nsITableEditor)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsIEditorMailSupport)
NS_INTERFACE_MAP_END_INHERITING(EditorBase)

nsresult HTMLEditor::Init(Document& aDocument,
                          ComposerCommandsUpdater& aComposerCommandsUpdater,
                          uint32_t aFlags) {
  MOZ_ASSERT(!mInitSucceeded,
             "HTMLEditor::Init() called again without calling PreDestroy()?");

  MOZ_DIAGNOSTIC_ASSERT(!mComposerCommandsUpdater ||
                        mComposerCommandsUpdater == &aComposerCommandsUpdater);
  mComposerCommandsUpdater = &aComposerCommandsUpdater;

  RefPtr<PresShell> presShell = aDocument.GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = InitInternal(aDocument, nullptr, *presShell, aFlags);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InitInternal() failed");
    return rv;
  }

  // Init mutation observer
  aDocument.AddMutationObserverUnlessExists(this);

  if (!mRootElement) {
    UpdateRootElement();
  }

  // disable Composer-only features
  if (IsMailEditor()) {
    DebugOnly<nsresult> rvIgnored = SetAbsolutePositioningEnabled(false);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::SetAbsolutePositioningEnabled(false) failed, but ignored");
    rvIgnored = SetSnapToGridEnabled(false);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::SetSnapToGridEnabled(false) failed, but ignored");
  }

  // Init the HTML-CSS utils
  mCSSEditUtils = MakeUnique<CSSEditUtils>(this);

  // disable links
  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_FAILURE;
  }
  if (!IsInPlaintextMode() && !IsInteractionAllowed()) {
    mDisabledLinkHandling = true;
    mOldLinkHandlingEnabled = document->LinkHandlingEnabled();
    document->SetLinkHandlingEnabled(false);
  }

  // init the type-in state
  mTypeInState = new TypeInState();

  if (!IsInteractionAllowed()) {
    nsCOMPtr<nsIURI> uaURI;
    rv = NS_NewURI(getter_AddRefs(uaURI),
                   "resource://gre/res/EditorOverride.css");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = document->LoadAdditionalStyleSheet(Document::eAgentSheet, uaURI);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eInitializing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_FAILURE;
  }

  rv = InitEditorContentAndSelection();
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::InitEditorContentAndSelection() failed");
    // XXX Sholdn't we expose `NS_ERROR_EDITOR_DESTROYED` even though this
    //     is a public method?
    return EditorBase::ToGenericNSResult(rv);
  }

  // Throw away the old transaction manager if this is not the first time that
  // we're initializing the editor.
  ClearUndoRedo();
  EnableUndoRedo();  // FYI: Creating mTransactionManager in this call

  if (mTransactionManager) {
    mTransactionManager->Attach(*this);
  }

  MOZ_ASSERT(!mInitSucceeded, "HTMLEditor::Init() shouldn't be nested");
  mInitSucceeded = true;
  return NS_OK;
}

nsresult HTMLEditor::PostCreate() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = PostCreateInternal();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::PostCreatInternal() failed");
  return rv;
}

void HTMLEditor::PreDestroy() {
  if (mDidPreDestroy) {
    return;
  }

  mInitSucceeded = false;

  // FYI: Cannot create AutoEditActionDataSetter here.  However, it does not
  //      necessary for the methods called by the following code.

  RefPtr<Document> document = GetDocument();
  if (document) {
    document->RemoveMutationObserver(this);

    if (!IsInteractionAllowed()) {
      nsCOMPtr<nsIURI> uaURI;
      nsresult rv = NS_NewURI(getter_AddRefs(uaURI),
                              "resource://gre/res/EditorOverride.css");
      if (NS_SUCCEEDED(rv)) {
        document->RemoveAdditionalStyleSheet(Document::eAgentSheet, uaURI);
      }
    }
  }

  // Clean up after our anonymous content -- we don't want these nodes to
  // stay around (which they would, since the frames have an owning reference).
  PresShell* presShell = GetPresShell();
  if (presShell && presShell->IsDestroying()) {
    // Just destroying PresShell now.
    // We have to keep UI elements of anonymous content until PresShell
    // is destroyed.
    RefPtr<HTMLEditor> self = this;
    nsContentUtils::AddScriptRunner(
        NS_NewRunnableFunction("HTMLEditor::PreDestroy",
                               [self]() { self->HideAnonymousEditingUIs(); }));
  } else {
    // PresShell is alive or already gone.
    HideAnonymousEditingUIs();
  }

  mPaddingBRElementForEmptyEditor = nullptr;

  PreDestroyInternal();
}

NS_IMETHODIMP HTMLEditor::GetDocumentCharacterSet(nsACString& aCharacterSet) {
  nsresult rv = GetDocumentCharsetInternal(aCharacterSet);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::GetDocumentCharsetInternal() failed");
  return rv;
}

NS_IMETHODIMP HTMLEditor::SetDocumentCharacterSet(
    const nsACString& aCharacterSet) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eSetCharacterSet);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return EditorBase::ToGenericNSResult(NS_ERROR_NOT_INITIALIZED);
  }
  // This method is scriptable, so add-ons could pass in something other
  // than a canonical name.
  const Encoding* encoding = Encoding::ForLabelNoReplacement(aCharacterSet);
  if (!encoding) {
    NS_WARNING("Encoding::ForLabelNoReplacement() failed");
    return EditorBase::ToGenericNSResult(NS_ERROR_INVALID_ARG);
  }
  document->SetDocumentCharacterSet(WrapNotNull(encoding));

  // Update META charset element.
  if (UpdateMetaCharsetWithTransaction(*document, aCharacterSet)) {
    return NS_OK;
  }

  // Set attributes to the created element
  if (aCharacterSet.IsEmpty()) {
    return NS_OK;
  }

  RefPtr<nsContentList> headElementList =
      document->GetElementsByTagName(u"head"_ns);
  if (NS_WARN_IF(!headElementList)) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> primaryHeadElement = headElementList->Item(0);
  if (NS_WARN_IF(!primaryHeadElement)) {
    return NS_OK;
  }

  // Create a new meta charset tag
  CreateElementResult createNewMetaElementResult = CreateAndInsertElement(
      WithTransaction::Yes, *nsGkAtoms::meta,
      EditorDOMPoint(primaryHeadElement, 0),
      [&aCharacterSet](HTMLEditor&, Element& aMetaElement,
                       const EditorDOMPoint&) {
        DebugOnly<nsresult> rvIgnored = aMetaElement.SetAttr(
            kNameSpaceID_None, nsGkAtoms::httpEquiv, u"Content-Type"_ns,
            aMetaElement.IsInComposedDoc());
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rvIgnored),
            nsPrintfCString(
                "Element::SetAttr(nsGkAtoms::httpEquiv, \"Content-Type\", "
                "%s) failed, but ignored",
                aMetaElement.IsInComposedDoc() ? "true" : "false")
                .get());
        rvIgnored = aMetaElement.SetAttr(
            kNameSpaceID_None, nsGkAtoms::content,
            u"text/html;charset="_ns + NS_ConvertASCIItoUTF16(aCharacterSet),
            aMetaElement.IsInComposedDoc());
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rvIgnored),
            nsPrintfCString("Element::SetAttr(nsGkAtoms::content, "
                            "\"text/html;charset=%s\", %s) failed, but ignored",
                            nsPromiseFlatCString(aCharacterSet).get(),
                            aMetaElement.IsInComposedDoc() ? "true" : "false")
                .get());
        return NS_OK;
      });
  NS_WARNING_ASSERTION(createNewMetaElementResult.isOk(),
                       "HTMLEditor::CreateAndInsertElement(WithTransaction::"
                       "Yes, nsGkAtoms::meta) failed, but ignored");
  // Probably, we don't need to update selection in this case since we should
  // not put selection into <head> element.
  createNewMetaElementResult.IgnoreCaretPointSuggestion();
  return NS_OK;
}

bool HTMLEditor::UpdateMetaCharsetWithTransaction(
    Document& aDocument, const nsACString& aCharacterSet) {
  // get a list of META tags
  RefPtr<nsContentList> metaElementList =
      aDocument.GetElementsByTagName(u"meta"_ns);
  if (NS_WARN_IF(!metaElementList)) {
    return false;
  }

  for (uint32_t i = 0; i < metaElementList->Length(true); ++i) {
    RefPtr<Element> metaElement = metaElementList->Item(i)->AsElement();
    MOZ_ASSERT(metaElement);

    nsAutoString currentValue;
    metaElement->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, currentValue);

    if (!FindInReadable(u"content-type"_ns, currentValue,
                        nsCaseInsensitiveStringComparator)) {
      continue;
    }

    metaElement->GetAttr(kNameSpaceID_None, nsGkAtoms::content, currentValue);

    constexpr auto charsetEquals = u"charset="_ns;
    nsAString::const_iterator originalStart, start, end;
    originalStart = currentValue.BeginReading(start);
    currentValue.EndReading(end);
    if (!FindInReadable(charsetEquals, start, end,
                        nsCaseInsensitiveStringComparator)) {
      continue;
    }

    // set attribute to <original prefix> charset=text/html
    nsresult rv = SetAttributeWithTransaction(
        *metaElement, *nsGkAtoms::content,
        Substring(originalStart, start) + charsetEquals +
            NS_ConvertASCIItoUTF16(aCharacterSet));
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::SetAttributeWithTransaction(nsGkAtoms::content) failed");
    return NS_SUCCEEDED(rv);
  }
  return false;
}

NS_IMETHODIMP HTMLEditor::NotifySelectionChanged(Document* aDocument,
                                                 Selection* aSelection,
                                                 int16_t aReason,
                                                 int32_t aAmount) {
  if (NS_WARN_IF(!aDocument) || NS_WARN_IF(!aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mTypeInState) {
    RefPtr<TypeInState> typeInState = mTypeInState;
    typeInState->OnSelectionChange(*this, aReason);

    // We used a class which derived from nsISelectionListener to call
    // HTMLEditor::RefreshEditingUI().  The lifetime of the class was
    // exactly same as mTypeInState.  So, call it only when mTypeInState
    // is not nullptr.
    if ((aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                    nsISelectionListener::KEYPRESS_REASON |
                    nsISelectionListener::SELECTALL_REASON)) &&
        aSelection) {
      // the selection changed and we need to check if we have to
      // hide and/or redisplay resizing handles
      // FYI: This is an XPCOM method.  So, the caller, Selection, guarantees
      //      the lifetime of this instance.  So, don't need to grab this with
      //      local variable.
      DebugOnly<nsresult> rv = RefreshEditingUI();
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::RefreshEditingUI() failed, but ignored");
    }
  }

  if (mComposerCommandsUpdater) {
    RefPtr<ComposerCommandsUpdater> updater = mComposerCommandsUpdater;
    updater->OnSelectionChange();
  }

  nsresult rv = EditorBase::NotifySelectionChanged(aDocument, aSelection,
                                                   aReason, aAmount);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::NotifySelectionChanged() failed");
  return rv;
}

void HTMLEditor::UpdateRootElement() {
  // Use the HTML documents body element as the editor root if we didn't
  // get a root element during initialization.

  mRootElement = GetBodyElement();
  if (!mRootElement) {
    RefPtr<Document> doc = GetDocument();
    if (doc) {
      // If there is no HTML body element,
      // we should use the document root element instead.
      mRootElement = doc->GetDocumentElement();
    }
    // else leave it null, for lack of anything better.
  }
}

nsresult HTMLEditor::OnFocus(const nsINode& aOriginalEventTargetNode) {
  // Before doing anything, we should check whether the original target is still
  // valid focus event target because it may have already lost focus.
  if (!CanKeepHandlingFocusEvent(aOriginalEventTargetNode)) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_FAILURE;
  }

  return EditorBase::OnFocus(aOriginalEventTargetNode);
}

nsresult HTMLEditor::OnBlur(const EventTarget* aEventTarget) {
  // check if something else is focused. If another element is focused, then
  // we should not change the selection.
  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (MOZ_UNLIKELY(!focusManager)) {
    return NS_OK;
  }

  // If another element already has focus, we should not maintain the selection
  // because we may not have the rights doing it.
  if (focusManager->GetFocusedElement()) {
    return NS_OK;
  }

  // If it's in the designMode, and blur occurs, the target must be the
  // document node.  If a blur event is fired and the target is an element, it
  // must be delayed blur event at initializing the `HTMLEditor`.
  // TODO: Add automated tests for checking the case that the target node
  //       is in a shadow DOM tree whose host is in design mode.
  if (IsInDesignMode() && Element::FromEventTargetOrNull(aEventTarget)) {
    return NS_OK;
  }
  nsresult rv = FinalizeSelection();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::FinalizeSelection() failed");
  return rv;
}

Element* HTMLEditor::FindSelectionRoot(const nsINode& aNode) const {
  MOZ_ASSERT(aNode.IsDocument() || aNode.IsContent(),
             "aNode must be content or document node");

  if (NS_WARN_IF(!aNode.IsInComposedDoc())) {
    return nullptr;
  }

  if (aNode.IsInDesignMode()) {
    return GetDocument()->GetRootElement();
  }

  // XXX If we have readonly flag, shouldn't return the element which has
  // contenteditable="true"?  However, such case isn't there without chrome
  // permission script.
  if (IsReadonly()) {
    // We still want to allow selection in a readonly editor.
    return GetRoot();
  }

  nsIContent* content = const_cast<nsIContent*>(aNode.AsContent());
  if (!content->HasFlag(NODE_IS_EDITABLE)) {
    // If the content is in read-write state but is not editable itself,
    // return it as the selection root.
    if (content->IsElement() &&
        content->AsElement()->State().HasState(ElementState::READWRITE)) {
      return content->AsElement();
    }
    return nullptr;
  }

  // For non-readonly editors we want to find the root of the editable subtree
  // containing aContent.
  return content->GetEditingHost();
}

bool HTMLEditor::IsInDesignMode() const {
  // TODO: If active editing host is in a shadow tree, it means that we should
  //       behave exactly same as contenteditable mode because shadow tree
  //       content is not editable even if composed document is in design mode,
  //       but contenteditable elements in shoadow trees are focusable and
  //       their content is editable.  Changing this affects to drop event
  //       handler and blur event handler, so please add new tests for them
  //       when you change here.
  Document* document = GetDocument();
  return document && document->IsInDesignMode();
}

void HTMLEditor::CreateEventListeners() {
  // Don't create the handler twice
  if (!mEventListener) {
    mEventListener = new HTMLEditorEventListener();
  }
}

nsresult HTMLEditor::InstallEventListeners() {
  if (NS_WARN_IF(!IsInitialized()) || NS_WARN_IF(!mEventListener)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // NOTE: HTMLEditor doesn't need to initialize mEventTarget here because
  // the target must be document node and it must be referenced as weak pointer.

  HTMLEditorEventListener* listener =
      reinterpret_cast<HTMLEditorEventListener*>(mEventListener.get());
  nsresult rv = listener->Connect(this);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditorEventListener::Connect() failed");
  return rv;
}

void HTMLEditor::RemoveEventListeners() {
  if (!IsInitialized()) {
    return;
  }

  EditorBase::RemoveEventListeners();
}

void HTMLEditor::Detach(
    const ComposerCommandsUpdater& aComposerCommandsUpdater) {
  MOZ_DIAGNOSTIC_ASSERT_IF(
      mComposerCommandsUpdater,
      &aComposerCommandsUpdater == mComposerCommandsUpdater);
  if (mComposerCommandsUpdater == &aComposerCommandsUpdater) {
    mComposerCommandsUpdater = nullptr;
    if (mTransactionManager) {
      mTransactionManager->Detach(*this);
    }
  }
}

NS_IMETHODIMP HTMLEditor::BeginningOfDocument() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = MaybeCollapseSelectionAtFirstEditableNode(false);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::MaybeCollapseSelectionAtFirstEditableNode(false) failed");
  return rv;
}

void HTMLEditor::InitializeSelectionAncestorLimit(
    nsIContent& aAncestorLimit) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Hack for initializing selection.
  // HTMLEditor::MaybeCollapseSelectionAtFirstEditableNode() will try to
  // collapse selection at first editable text node or inline element which
  // cannot have text nodes as its children.  However, selection has already
  // set into the new editing host by user, we should not change it.  For
  // solving this issue, we should do nothing if selection range is in active
  // editing host except it's not collapsed at start of the editing host since
  // aSelection.SetAncestorLimiter(aAncestorLimit) will collapse selection
  // at start of the new limiter if focus node of aSelection is outside of the
  // editing host.  However, we need to check here if selection is already
  // collapsed at start of the editing host because it's possible JS to do it.
  // In such case, we should not modify selection with calling
  // MaybeCollapseSelectionAtFirstEditableNode().

  // Basically, we should try to collapse selection at first editable node
  // in HTMLEditor.
  bool tryToCollapseSelectionAtFirstEditableNode = true;
  if (SelectionRef().RangeCount() == 1 && SelectionRef().IsCollapsed()) {
    Element* editingHost = ComputeEditingHost();
    const nsRange* range = SelectionRef().GetRangeAt(0);
    if (range->GetStartContainer() == editingHost && !range->StartOffset()) {
      // JS or user operation has already collapsed selection at start of
      // the editing host.  So, we don't need to try to change selection
      // in this case.
      tryToCollapseSelectionAtFirstEditableNode = false;
    }
  }

  EditorBase::InitializeSelectionAncestorLimit(aAncestorLimit);

  // XXX Do we need to check if we still need to change selection?  E.g.,
  //     we could have already lost focus while we're changing the ancestor
  //     limiter because it may causes "selectionchange" event.
  if (tryToCollapseSelectionAtFirstEditableNode) {
    DebugOnly<nsresult> rvIgnored =
        MaybeCollapseSelectionAtFirstEditableNode(true);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::MaybeCollapseSelectionAtFirstEditableNode(true) failed, "
        "but ignored");
  }

  // If the target is a text control element, we won't handle user input
  // for the `TextEditor` in it.  However, we need to be open for `execCommand`.
  // Therefore, we shouldn't set ancestor limit in this case.
  // Note that we should do this once setting ancestor limiter for backward
  // compatiblity of select events, etc.  (Selection should be collapsed into
  // the text control element.)
  if (aAncestorLimit.HasIndependentSelection()) {
    SelectionRef().SetAncestorLimiter(nullptr);
  }
}

nsresult HTMLEditor::MaybeCollapseSelectionAtFirstEditableNode(
    bool aIgnoreIfSelectionInEditingHost) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> editingHost = ComputeEditingHost(LimitInBodyElement::No);
  if (NS_WARN_IF(!editingHost)) {
    return NS_OK;
  }

  // If selection range is already in the editing host and the range is not
  // start of the editing host, we shouldn't reset selection.  E.g., window
  // is activated when the editor had focus before inactivated.
  if (aIgnoreIfSelectionInEditingHost && SelectionRef().RangeCount() == 1) {
    const nsRange* range = SelectionRef().GetRangeAt(0);
    if (!range->Collapsed() ||
        range->GetStartContainer() != editingHost.get() ||
        range->StartOffset()) {
      return NS_OK;
    }
  }

  for (nsIContent* leafContent = HTMLEditUtils::GetFirstLeafContent(
           *editingHost,
           {LeafNodeType::LeafNodeOrNonEditableNode,
            LeafNodeType::LeafNodeOrChildBlock},
           editingHost);
       leafContent;) {
    // If we meet a non-editable node first, we should move caret to start
    // of the container block or editing host.
    if (!EditorUtils::IsEditableContent(*leafContent, EditorType::HTML)) {
      MOZ_ASSERT(leafContent->GetParent());
      MOZ_ASSERT(EditorUtils::IsEditableContent(*leafContent->GetParent(),
                                                EditorType::HTML));
      if (const Element* editableBlockElementOrInlineEditingHost =
              HTMLEditUtils::GetAncestorElement(
                  *leafContent,
                  HTMLEditUtils::
                      ClosestEditableBlockElementOrInlineEditingHost)) {
        nsresult rv = CollapseSelectionTo(
            EditorDOMPoint(editableBlockElementOrInlineEditingHost, 0));
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "EditorBase::CollapseSelectionTo() failed");
        return rv;
      }
      NS_WARNING("Found leaf content did not have editable parent, why?");
      return NS_ERROR_FAILURE;
    }

    // When we meet an empty inline element, we should look for a next sibling.
    // For example, if current editor is:
    // <div contenteditable><span></span><b><br></b></div>
    // then, we should put caret at the <br> element.  So, let's check if found
    // node is an empty inline container element.
    if (leafContent->IsElement() &&
        HTMLEditUtils::IsInlineElement(*leafContent) &&
        !HTMLEditUtils::IsNeverElementContentsEditableByUser(*leafContent) &&
        HTMLEditUtils::CanNodeContain(*leafContent, *nsGkAtoms::textTagName)) {
      // Chromium collaps selection to start of the editing host when this is
      // the last leaf content.  So, we don't need special handling here.
      leafContent = HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          *leafContent, *editingHost,
          {LeafNodeType::LeafNodeOrNonEditableNode,
           LeafNodeType::LeafNodeOrChildBlock},
          editingHost);
      continue;
    }

    if (Text* text = leafContent->GetAsText()) {
      // If there is editable and visible text node, move caret at first of
      // the visible character.
      WSScanResult scanResultInTextNode =
          WSRunScanner::ScanNextVisibleNodeOrBlockBoundary(
              editingHost, EditorRawDOMPoint(text, 0));
      if ((scanResultInTextNode.InVisibleOrCollapsibleCharacters() ||
           scanResultInTextNode.ReachedPreformattedLineBreak()) &&
          scanResultInTextNode.TextPtr() == text) {
        nsresult rv = CollapseSelectionTo(
            scanResultInTextNode.Point<EditorRawDOMPoint>());
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "EditorBase::CollapseSelectionTo() failed");
        return rv;
      }
      // If it's an invisible text node, keep scanning next leaf.
      leafContent = HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          *leafContent, *editingHost,
          {LeafNodeType::LeafNodeOrNonEditableNode,
           LeafNodeType::LeafNodeOrChildBlock},
          editingHost);
      continue;
    }

    // If there is editable <br> or something void element like <img>, <input>,
    // <hr> etc, move caret before it.
    if (!HTMLEditUtils::CanNodeContain(*leafContent, *nsGkAtoms::textTagName) ||
        HTMLEditUtils::IsNeverElementContentsEditableByUser(*leafContent)) {
      MOZ_ASSERT(leafContent->GetParent());
      if (EditorUtils::IsEditableContent(*leafContent, EditorType::HTML)) {
        nsresult rv = CollapseSelectionTo(EditorDOMPoint(leafContent));
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "EditorBase::CollapseSelectionTo() failed");
        return rv;
      }
      MOZ_ASSERT_UNREACHABLE(
          "How do we reach editable leaf in non-editable element?");
      // But if it's not editable, let's put caret at start of editing host
      // for now.
      nsresult rv = CollapseSelectionTo(EditorDOMPoint(editingHost, 0));
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorBase::CollapseSelectionTo() failed");
      return rv;
    }

    // If we meet non-empty block element, we need to scan its child too.
    if (HTMLEditUtils::IsBlockElement(*leafContent) &&
        !HTMLEditUtils::IsEmptyNode(
            *leafContent, {EmptyCheckOption::TreatSingleBRElementAsVisible}) &&
        !HTMLEditUtils::IsNeverElementContentsEditableByUser(*leafContent)) {
      leafContent = HTMLEditUtils::GetFirstLeafContent(
          *leafContent,
          {LeafNodeType::LeafNodeOrNonEditableNode,
           LeafNodeType::LeafNodeOrChildBlock},
          editingHost);
      continue;
    }

    // Otherwise, we must meet an empty block element or a data node like
    // comment node.  Let's ignore it.
    leafContent = HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
        *leafContent, *editingHost,
        {LeafNodeType::LeafNodeOrNonEditableNode,
         LeafNodeType::LeafNodeOrChildBlock},
        editingHost);
  }

  // If there is no visible/editable node except another block element in
  // current editing host, we should move caret to very first of the editing
  // host.
  // XXX This may not make sense, but Chromium behaves so.  Therefore, the
  //     reason why we do this is just compatibility with Chromium.
  nsresult rv = CollapseSelectionTo(EditorDOMPoint(editingHost, 0));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CollapseSelectionTo() failed");
  return rv;
}

bool HTMLEditor::ArePreservingSelection() const {
  return IsEditActionDataAvailable() && SavedSelectionRef().RangeCount();
}

void HTMLEditor::PreserveSelectionAcrossActions() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  SavedSelectionRef().SaveSelection(SelectionRef());
  RangeUpdaterRef().RegisterSelectionState(SavedSelectionRef());
}

nsresult HTMLEditor::RestorePreservedSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!SavedSelectionRef().RangeCount()) {
    // XXX Returing error when it does not store is odd because no selection
    //     ranges is not illegal case in general.
    return NS_ERROR_FAILURE;
  }
  DebugOnly<nsresult> rvIgnored =
      SavedSelectionRef().RestoreSelection(SelectionRef());
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "SelectionState::RestoreSelection() failed, but ignored");
  StopPreservingSelection();
  return NS_OK;
}

void HTMLEditor::StopPreservingSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RangeUpdaterRef().DropSelectionState(SavedSelectionRef());
  SavedSelectionRef().RemoveAllRanges();
}

void HTMLEditor::PreHandleMouseDown(const MouseEvent& aMouseDownEvent) {
  if (mTypeInState) {
    // mTypeInState will be notified of selection change even if aMouseDownEvent
    // is not an acceptable event for this editor.  Therefore, we need to notify
    // it of this event too.
    mTypeInState->PreHandleMouseEvent(aMouseDownEvent);
  }
}

void HTMLEditor::PreHandleMouseUp(const MouseEvent& aMouseUpEvent) {
  if (mTypeInState) {
    // mTypeInState will be notified of selection change even if aMouseUpEvent
    // is not an acceptable event for this editor.  Therefore, we need to notify
    // it of this event too.
    mTypeInState->PreHandleMouseEvent(aMouseUpEvent);
  }
}

void HTMLEditor::PreHandleSelectionChangeCommand(Command aCommand) {
  if (mTypeInState) {
    mTypeInState->PreHandleSelectionChangeCommand(aCommand);
  }
}

void HTMLEditor::PostHandleSelectionChangeCommand(Command aCommand) {
  if (!mTypeInState) {
    return;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (!editActionData.CanHandle()) {
    return;
  }
  mTypeInState->PostHandleSelectionChangeCommand(*this, aCommand);
}

nsresult HTMLEditor::HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent) {
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/tests/test_htmleditor_keyevent_handling.html
  if (NS_WARN_IF(!aKeyboardEvent)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (IsReadonly()) {
    HandleKeyPressEventInReadOnlyMode(*aKeyboardEvent);
    return NS_OK;
  }

  MOZ_ASSERT(aKeyboardEvent->mMessage == eKeyPress,
             "HandleKeyPressEvent gets non-keypress event");

  switch (aKeyboardEvent->mKeyCode) {
    case NS_VK_META:
    case NS_VK_WIN:
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
      // FYI: This shouldn't occur since modifier key shouldn't cause eKeyPress
      //      event.
      aKeyboardEvent->PreventDefault();
      return NS_OK;

    case NS_VK_BACK:
    case NS_VK_DELETE: {
      nsresult rv = EditorBase::HandleKeyPressEvent(aKeyboardEvent);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorBase::HandleKeyPressEvent() failed");
      return rv;
    }
    case NS_VK_TAB: {
      // Basically, "Tab" key be used only for focus navigation.
      // FYI: In web apps, this is always true.
      if (IsTabbable()) {
        return NS_OK;
      }

      // If we're in the plaintext mode, and not tabbable editor, let's
      // insert a horizontal tabulation.
      if (IsInPlaintextMode()) {
        if (aKeyboardEvent->IsShift() || aKeyboardEvent->IsControl() ||
            aKeyboardEvent->IsAlt() || aKeyboardEvent->IsMeta() ||
            aKeyboardEvent->IsOS()) {
          return NS_OK;
        }

        // else we insert the tab straight through
        aKeyboardEvent->PreventDefault();
        nsresult rv = OnInputText(u"\t"_ns);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "EditorBase::OnInputText(\\t) failed");
        return rv;
      }

      // Otherwise, e.g., we're an embedding editor in chrome, we can handle
      // "Tab" key as an input.
      if (aKeyboardEvent->IsControl() || aKeyboardEvent->IsAlt() ||
          aKeyboardEvent->IsMeta() || aKeyboardEvent->IsOS()) {
        return NS_OK;
      }

      RefPtr<Selection> selection = GetSelection();
      if (NS_WARN_IF(!selection) || NS_WARN_IF(!selection->RangeCount())) {
        return NS_ERROR_FAILURE;
      }

      nsINode* startContainer = selection->GetRangeAt(0)->GetStartContainer();
      MOZ_ASSERT(startContainer);
      if (!startContainer->IsContent()) {
        break;
      }

      const Element* editableBlockElement =
          HTMLEditUtils::GetInclusiveAncestorElement(
              *startContainer->AsContent(),
              HTMLEditUtils::ClosestEditableBlockElement);
      if (!editableBlockElement) {
        break;
      }

      // If selection is in a table element, we need special handling.
      if (HTMLEditUtils::IsAnyTableElement(editableBlockElement)) {
        EditActionResult result = HandleTabKeyPressInTable(aKeyboardEvent);
        if (result.Failed()) {
          NS_WARNING("HTMLEditor::HandleTabKeyPressInTable() failed");
          return EditorBase::ToGenericNSResult(result.Rv());
        }
        if (!result.Handled()) {
          return NS_OK;
        }
        nsresult rv = ScrollSelectionFocusIntoView();
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rv),
            "EditorBase::ScrollSelectionFocusIntoView() failed");
        return EditorBase::ToGenericNSResult(rv);
      }

      // If selection is in an list item element, treat it as indent or outdent.
      if (HTMLEditUtils::IsListItem(editableBlockElement)) {
        aKeyboardEvent->PreventDefault();
        if (!aKeyboardEvent->IsShift()) {
          nsresult rv = IndentAsAction();
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "HTMLEditor::IndentAsAction() failed");
          return EditorBase::ToGenericNSResult(rv);
        }
        nsresult rv = OutdentAsAction();
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "HTMLEditor::OutdentAsAction() failed");
        return EditorBase::ToGenericNSResult(rv);
      }

      // If only "Tab" key is pressed in normal context, just treat it as
      // horizontal tab character input.
      if (aKeyboardEvent->IsShift()) {
        return NS_OK;
      }
      aKeyboardEvent->PreventDefault();
      nsresult rv = OnInputText(u"\t"_ns);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorBase::OnInputText(\\t) failed");
      return EditorBase::ToGenericNSResult(rv);
    }
    case NS_VK_RETURN:
      if (!aKeyboardEvent->IsInputtingLineBreak()) {
        return NS_OK;
      }
      aKeyboardEvent->PreventDefault();  // consumed
      if (aKeyboardEvent->IsShift()) {
        // Only inserts a <br> element.
        nsresult rv = InsertLineBreakAsAction();
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "HTMLEditor::InsertLineBreakAsAction() failed");
        return EditorBase::ToGenericNSResult(rv);
      }
      // uses rules to figure out what to insert
      nsresult rv = InsertParagraphSeparatorAsAction();
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::InsertParagraphSeparatorAsAction() failed");
      return EditorBase::ToGenericNSResult(rv);
  }

  if (!aKeyboardEvent->IsInputtingText()) {
    // we don't PreventDefault() here or keybindings like control-x won't work
    return NS_OK;
  }
  aKeyboardEvent->PreventDefault();
  nsAutoString str(aKeyboardEvent->mCharCode);
  nsresult rv = OnInputText(str);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "EditorBase::OnInputText() failed");
  return rv;
}

NS_IMETHODIMP HTMLEditor::NodeIsBlock(nsINode* aNode, bool* aIsBlock) {
  *aIsBlock = aNode && aNode->IsContent() &&
              HTMLEditUtils::IsBlockElement(*aNode->AsContent());
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::UpdateBaseURL() {
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_FAILURE;
  }

  // Look for an HTML <base> tag
  RefPtr<nsContentList> baseElementList =
      document->GetElementsByTagName(u"base"_ns);

  // If no base tag, then set baseURL to the document's URL.  This is very
  // important, else relative URLs for links and images are wrong
  if (!baseElementList || !baseElementList->Item(0)) {
    document->SetBaseURI(document->GetDocumentURI());
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::InsertLineBreak() {
  // XPCOM method's InsertLineBreak() should insert paragraph separator in
  // HTMLEditor.
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eInsertParagraphSeparator);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  const RefPtr<Element> editingHost = ComputeEditingHost();
  if (!editingHost) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  EditActionResult result = InsertParagraphSeparatorAsSubAction(*editingHost);
  NS_WARNING_ASSERTION(
      result.Succeeded(),
      "HTMLEditor::InsertParagraphSeparatorAsSubAction() failed");
  return EditorBase::ToGenericNSResult(result.Rv());
}

nsresult HTMLEditor::InsertLineBreakAsAction(nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertLineBreak,
                                          aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (IsSelectionRangeContainerNotContent()) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  rv = InsertLineBreakAsSubAction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertLineBreakAsSubAction() failed");
  // Don't return NS_SUCCESS_DOM_NO_OPERATION for compatibility of `execCommand`
  // result of Chrome.
  return NS_FAILED(rv) ? rv : NS_OK;
}

nsresult HTMLEditor::InsertParagraphSeparatorAsAction(
    nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eInsertParagraphSeparator, aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  const RefPtr<Element> editingHost = ComputeEditingHost();
  if (!editingHost) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  EditActionResult result = InsertParagraphSeparatorAsSubAction(*editingHost);
  NS_WARNING_ASSERTION(
      result.Succeeded(),
      "HTMLEditor::InsertParagraphSeparatorAsSubAction() failed");
  return EditorBase::ToGenericNSResult(result.Rv());
}

EditActionResult HTMLEditor::HandleTabKeyPressInTable(
    WidgetKeyboardEvent* aKeyboardEvent) {
  MOZ_ASSERT(aKeyboardEvent);

  AutoEditActionDataSetter dummyEditActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!dummyEditActionData.CanHandle())) {
    // Do nothing if we didn't find a table cell.
    return EditActionIgnored();
  }

  // Find enclosing table cell from selection (cell may be selected element)
  const RefPtr<Element> cellElement =
      GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::td);
  if (!cellElement) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::td) "
        "returned nullptr");
    // Do nothing if we didn't find a table cell.
    return EditActionIgnored();
  }

  // find enclosing table
  RefPtr<Element> table =
      HTMLEditUtils::GetClosestAncestorTableElement(*cellElement);
  if (!table) {
    NS_WARNING("HTMLEditor::GetClosestAncestorTableElement() failed");
    return EditActionIgnored();
  }

  // advance to next cell
  // first create an iterator over the table
  PostContentIterator postOrderIter;
  nsresult rv = postOrderIter.Init(table);
  if (NS_FAILED(rv)) {
    NS_WARNING("PostContentIterator::Init() failed");
    return EditActionResult(rv);
  }
  // position postOrderIter at block
  rv = postOrderIter.PositionAt(cellElement);
  if (NS_FAILED(rv)) {
    NS_WARNING("PostContentIterator::PositionAt() failed");
    return EditActionResult(rv);
  }

  do {
    if (aKeyboardEvent->IsShift()) {
      postOrderIter.Prev();
    } else {
      postOrderIter.Next();
    }

    nsCOMPtr<nsINode> node = postOrderIter.GetCurrentNode();
    if (node && HTMLEditUtils::IsTableCell(node) &&
        HTMLEditUtils::GetClosestAncestorTableElement(*node->AsElement()) ==
            table) {
      aKeyboardEvent->PreventDefault();
      CollapseSelectionToDeepestNonTableFirstChild(node);
      return EditActionHandled(
          NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK);
    }
  } while (!postOrderIter.IsDone());

  if (aKeyboardEvent->IsShift()) {
    return EditActionIgnored();
  }

  // If we haven't handled it yet, then we must have run off the end of the
  // table.  Insert a new row.
  // XXX We should investigate whether this behavior is supported by other
  //     browsers later.
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eInsertTableRowElement);
  rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (MOZ_UNLIKELY(NS_FAILED(rv))) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditActionHandled(rv);
  }
  rv = InsertTableRowsWithTransaction(*cellElement, 1,
                                      InsertPosition::eAfterSelectedCell);
  if (NS_WARN_IF(Destroyed())) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::InsertTableRowsWithTransaction(*cellElement, 1, "
        "InsertPosition::eAfterSelectedCell) failed");
    return EditActionHandled(rv);
  }
  aKeyboardEvent->PreventDefault();
  // Put selection in right place.  Use table code to get selection and index
  // to new row...
  RefPtr<Element> tblElement, cell;
  int32_t row;
  rv = GetCellContext(getter_AddRefs(tblElement), getter_AddRefs(cell), nullptr,
                      nullptr, &row, nullptr);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return EditActionHandled(rv);
  }
  if (!tblElement) {
    NS_WARNING("HTMLEditor::GetCellContext() didn't return table element");
    return EditActionHandled(NS_ERROR_FAILURE);
  }
  // ...so that we can ask for first cell in that row...
  cell = GetTableCellElementAt(*tblElement, row, 0);
  // ...and then set selection there.  (Note that normally you should use
  // CollapseSelectionToDeepestNonTableFirstChild(), but we know cell is an
  // empty new cell, so this works fine)
  if (cell) {
    nsresult rv = CollapseSelectionToStartOf(*cell);
    if (MOZ_UNLIKELY(NS_FAILED(rv))) {
      NS_WARNING(
          "EditorBase::CollapseSelectionToStartOf() caused destroying the "
          "editor");
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::CollapseSelectionToStartOf() failed, but ignored");
  }
  return EditActionHandled(NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED
                                                   : NS_OK);
}

void HTMLEditor::CollapseSelectionToDeepestNonTableFirstChild(nsINode* aNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(aNode);

  nsCOMPtr<nsINode> node = aNode;

  for (nsIContent* child = node->GetFirstChild(); child;
       child = child->GetFirstChild()) {
    // Stop if we find a table, don't want to go into nested tables
    if (HTMLEditUtils::IsTable(child) ||
        !HTMLEditUtils::IsContainerNode(*child)) {
      break;
    }
    node = child;
  }

  DebugOnly<nsresult> rvIgnored = CollapseSelectionToStartOf(*node);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EditorBase::CollapseSelectionToStartOf() failed, but ignored");
}

nsresult HTMLEditor::ReplaceHeadContentsWithSourceWithTransaction(
    const nsAString& aSourceToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // don't do any post processing, rules get confused
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eReplaceHeadWithHTMLSource, nsIEditor::eNone,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  CommitComposition();

  // Do not use AutoEditSubActionNotifier -- rules code won't let us insert in
  // <head>.  Use the head node as a parent and delete/insert directly.
  // XXX We're using AutoEditSubActionNotifier above...
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsContentList> headElementList =
      document->GetElementsByTagName(u"head"_ns);
  if (NS_WARN_IF(!headElementList)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> primaryHeadElement = headElementList->Item(0)->AsElement();
  if (NS_WARN_IF(!primaryHeadElement)) {
    return NS_ERROR_FAILURE;
  }

  // First, make sure there are no return chars in the source.  Bad things
  // happen if you insert returns (instead of dom newlines, \n) into an editor
  // document.
  nsAutoString inputString(aSourceToInsert);

  // Windows linebreaks: Map CRLF to LF:
  inputString.ReplaceSubstring(u"\r\n"_ns, u"\n"_ns);

  // Mac linebreaks: Map any remaining CR to LF:
  inputString.ReplaceSubstring(u"\r"_ns, u"\n"_ns);

  AutoPlaceholderBatch treatAsOneTransaction(
      *this, ScrollSelectionIntoView::Yes, __FUNCTION__);

  // Get the first range in the selection, for context:
  RefPtr<const nsRange> range = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult error;
  RefPtr<DocumentFragment> documentFragment =
      range->CreateContextualFragment(inputString, error);

  // XXXX BUG 50965: This is not returning the text between <title>...</title>
  // Special code is needed in JS to handle title anyway, so it doesn't matter!

  if (error.Failed()) {
    NS_WARNING("nsRange::CreateContextualFragment() failed");
    return error.StealNSResult();
  }
  if (NS_WARN_IF(!documentFragment)) {
    NS_WARNING(
        "nsRange::CreateContextualFragment() didn't create DocumentFragment");
    return NS_ERROR_FAILURE;
  }

  // First delete all children in head
  while (nsCOMPtr<nsIContent> child = primaryHeadElement->GetFirstChild()) {
    nsresult rv = DeleteNodeWithTransaction(*child);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }

  // Now insert the new nodes
  int32_t offsetOfNewNode = 0;

  // Loop over the contents of the fragment and move into the document
  while (nsCOMPtr<nsIContent> child = documentFragment->GetFirstChild()) {
    CreateContentResult insertChildContentResult = InsertNodeWithTransaction(
        *child, EditorDOMPoint(primaryHeadElement, offsetOfNewNode++));
    if (insertChildContentResult.isErr()) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return insertChildContentResult.unwrapErr();
    }
    // We probably don't need to adjust selection here, although we've done it
    // unless AutoTransactionsConserveSelection is created in a caller.
    insertChildContentResult.IgnoreCaretPointSuggestion();
  }

  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::RebuildDocumentFromSource(
    const nsAString& aSourceString) {
  CommitComposition();

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetHTML);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  RefPtr<Element> rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  // Find where the <body> tag starts.
  nsReadingIterator<char16_t> beginbody;
  nsReadingIterator<char16_t> endbody;
  aSourceString.BeginReading(beginbody);
  aSourceString.EndReading(endbody);
  bool foundbody =
      CaseInsensitiveFindInReadable(u"<body"_ns, beginbody, endbody);

  nsReadingIterator<char16_t> beginhead;
  nsReadingIterator<char16_t> endhead;
  aSourceString.BeginReading(beginhead);
  aSourceString.EndReading(endhead);
  bool foundhead =
      CaseInsensitiveFindInReadable(u"<head"_ns, beginhead, endhead);
  // a valid head appears before the body
  if (foundbody && beginhead.get() > beginbody.get()) {
    foundhead = false;
  }

  nsReadingIterator<char16_t> beginclosehead;
  nsReadingIterator<char16_t> endclosehead;
  aSourceString.BeginReading(beginclosehead);
  aSourceString.EndReading(endclosehead);

  // Find the index after "<head>"
  bool foundclosehead = CaseInsensitiveFindInReadable(
      u"</head>"_ns, beginclosehead, endclosehead);
  // a valid close head appears after a found head
  if (foundhead && beginhead.get() > beginclosehead.get()) {
    foundclosehead = false;
  }
  // a valid close head appears before a found body
  if (foundbody && beginclosehead.get() > beginbody.get()) {
    foundclosehead = false;
  }

  // Time to change the document
  AutoPlaceholderBatch treatAsOneTransaction(
      *this, ScrollSelectionIntoView::Yes, __FUNCTION__);

  nsReadingIterator<char16_t> endtotal;
  aSourceString.EndReading(endtotal);

  if (foundhead) {
    if (foundclosehead) {
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(
          Substring(beginhead, beginclosehead));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::ReplaceHeadContentsWithSourceWithTransaction() "
            "failed");
        return rv;
      }
    } else if (foundbody) {
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(
          Substring(beginhead, beginbody));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::ReplaceHeadContentsWithSourceWithTransaction() "
            "failed");
        return rv;
      }
    } else {
      // XXX Without recourse to some parser/content sink/docshell hackery we
      // don't really know where the head ends and the body begins so we assume
      // that there is no body
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(
          Substring(beginhead, endtotal));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::ReplaceHeadContentsWithSourceWithTransaction() "
            "failed");
        return rv;
      }
    }
  } else {
    nsReadingIterator<char16_t> begintotal;
    aSourceString.BeginReading(begintotal);
    constexpr auto head = u"<head>"_ns;
    if (foundclosehead) {
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(
          head + Substring(begintotal, beginclosehead));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::ReplaceHeadContentsWithSourceWithTransaction() "
            "failed");
        return rv;
      }
    } else if (foundbody) {
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(
          head + Substring(begintotal, beginbody));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::ReplaceHeadContentsWithSourceWithTransaction() "
            "failed");
        return rv;
      }
    } else {
      // XXX Without recourse to some parser/content sink/docshell hackery we
      // don't really know where the head ends and the body begins so we assume
      // that there is no head
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(head);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::ReplaceHeadContentsWithSourceWithTransaction() "
            "failed");
        return rv;
      }
    }
  }

  rv = SelectAll();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::SelectAll() failed");
    return rv;
  }

  if (!foundbody) {
    constexpr auto body = u"<body>"_ns;
    // XXX Without recourse to some parser/content sink/docshell hackery we
    // don't really know where the head ends and the body begins
    if (foundclosehead) {
      // assume body starts after the head ends
      nsresult rv = LoadHTML(body + Substring(endclosehead, endtotal));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::LoadHTML() failed");
        return rv;
      }
    } else if (foundhead) {
      // assume there is no body
      nsresult rv = LoadHTML(body);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::LoadHTML() failed");
        return rv;
      }
    } else {
      // assume there is no head, the entire source is body
      nsresult rv = LoadHTML(body + aSourceString);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::LoadHTML() failed");
        return rv;
      }
    }

    RefPtr<Element> divElement = CreateElementWithDefaults(*nsGkAtoms::div);
    if (!divElement) {
      NS_WARNING(
          "HTMLEditor::CreateElementWithDefaults(nsGkAtoms::div) failed");
      return NS_ERROR_FAILURE;
    }
    CloneAttributesWithTransaction(*rootElement, *divElement);

    nsresult rv = MaybeCollapseSelectionAtFirstEditableNode(false);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::MaybeCollapseSelectionAtFirstEditableNode(false) failed");
    return rv;
  }

  rv = LoadHTML(Substring(beginbody, endtotal));
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::LoadHTML() failed");
    return rv;
  }

  // Now we must copy attributes user might have edited on the <body> tag
  // because InsertHTML (actually, CreateContextualFragment()) will never
  // return a body node in the DOM fragment

  // We already know where "<body" begins
  nsReadingIterator<char16_t> beginclosebody = beginbody;
  nsReadingIterator<char16_t> endclosebody;
  aSourceString.EndReading(endclosebody);
  if (!FindInReadable(u">"_ns, beginclosebody, endclosebody)) {
    NS_WARNING("'>' was not found");
    return NS_ERROR_FAILURE;
  }

  // Truncate at the end of the body tag.  Kludge of the year: fool the parser
  // by replacing "body" with "div" so we get a node
  nsAutoString bodyTag;
  bodyTag.AssignLiteral("<div ");
  bodyTag.Append(Substring(endbody, endclosebody));

  RefPtr<const nsRange> range = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult error;
  RefPtr<DocumentFragment> documentFragment =
      range->CreateContextualFragment(bodyTag, error);
  if (error.Failed()) {
    NS_WARNING("nsRange::CreateContextualFragment() failed");
    return error.StealNSResult();
  }
  if (!documentFragment) {
    NS_WARNING(
        "nsRange::CreateContextualFragment() didn't create DocumentFagement");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> firstChild = documentFragment->GetFirstChild();
  if (!firstChild || !firstChild->IsElement()) {
    NS_WARNING("First child of DocumentFragment was not an Element node");
    return NS_ERROR_FAILURE;
  }

  // Copy all attributes from the div child to current body element
  CloneAttributesWithTransaction(*rootElement,
                                 MOZ_KnownLive(*firstChild->AsElement()));

  // place selection at first editable content
  rv = MaybeCollapseSelectionAtFirstEditableNode(false);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::MaybeCollapseSelectionAtFirstEditableNode(false) failed");
  return rv;
}

NS_IMETHODIMP HTMLEditor::InsertElementAtSelection(Element* aElement,
                                                   bool aDeleteSelection) {
  nsresult rv = InsertElementAtSelectionAsAction(aElement, aDeleteSelection);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertElementAtSelectionAsAction() failed");
  return rv;
}

nsresult HTMLEditor::InsertElementAtSelectionAsAction(
    Element* aElement, bool aDeleteSelection, nsIPrincipal* aPrincipal) {
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(
      *this, HTMLEditUtils::GetEditActionForInsert(*aElement), aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  DebugOnly<nsresult> rvIgnored = CommitComposition();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "EditorBase::CommitComposition() failed, but ignored");

  // XXX Oh, this should be done before dispatching `beforeinput` event.
  if (IsReadonly()) {
    return NS_OK;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return EditorBase::ToGenericNSResult(result.Rv());
  }

  UndefineCaretBidiLevel();

  AutoPlaceholderBatch treatAsOneTransaction(
      *this, ScrollSelectionIntoView::Yes, __FUNCTION__);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertElement, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  if (aDeleteSelection) {
    if (!HTMLEditUtils::IsBlockElement(*aElement)) {
      // E.g., inserting an image.  In this case we don't need to delete any
      // inline wrappers before we do the insertion.  Otherwise we let
      // DeleteSelectionAndPrepareToCreateNode do the deletion for us, which
      // calls DeleteSelection with aStripWrappers = eStrip.
      nsresult rv = DeleteSelectionAsSubAction(eNone, eNoStrip);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "EditorBase::DeleteSelectionAsSubAction(eNone, eNoStrip) failed");
        return EditorBase::ToGenericNSResult(rv);
      }
    }

    nsresult rv = DeleteSelectionAndPrepareToCreateNode();
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteSelectionAndPrepareToCreateNode() failed");
      return rv;
    }
  }
  // If deleting, selection will be collapsed.
  // so if not, we collapse it
  else {
    // Named Anchor is a special case,
    // We collapse to insert element BEFORE the selection
    // For all other tags, we insert AFTER the selection
    if (HTMLEditUtils::IsNamedAnchor(aElement)) {
      IgnoredErrorResult ignoredError;
      SelectionRef().CollapseToStart(ignoredError);
      if (NS_WARN_IF(Destroyed())) {
        return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "Selection::CollapseToStart() failed, but ignored");
    } else {
      IgnoredErrorResult ignoredError;
      SelectionRef().CollapseToEnd(ignoredError);
      if (NS_WARN_IF(Destroyed())) {
        return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "Selection::CollapseToEnd() failed, but ignored");
    }
  }

  if (!SelectionRef().GetAnchorNode()) {
    return NS_OK;
  }

  Element* editingHost = ComputeEditingHost(LimitInBodyElement::No);
  if (NS_WARN_IF(!editingHost)) {
    return EditorBase::ToGenericNSResult(NS_ERROR_FAILURE);
  }

  EditorRawDOMPoint atAnchor(SelectionRef().AnchorRef());
  // Adjust position based on the node we are going to insert.
  EditorDOMPoint pointToInsert =
      HTMLEditUtils::GetBetterInsertionPointFor<EditorDOMPoint>(
          *aElement, atAnchor, *editingHost);
  if (!pointToInsert.IsSet()) {
    NS_WARNING("HTMLEditUtils::GetBetterInsertionPointFor() failed");
    return NS_ERROR_FAILURE;
  }

  const CreateElementResult insertElementResult =
      InsertNodeIntoProperAncestorWithTransaction<Element>(
          *aElement, pointToInsert, SplitAtEdges::eAllowToCreateEmptyContainer);
  if (insertElementResult.isErr()) {
    NS_WARNING(
        "HTMLEditor::InsertNodeIntoProperAncestorWithTransaction(SplitAtEdges::"
        "eAllowToCreateEmptyContainer) failed");
    return EditorBase::ToGenericNSResult(insertElementResult.unwrapErr());
  }
  // Set caret after element, but check for special case
  //  of inserting table-related elements: set in first cell instead
  insertElementResult.IgnoreCaretPointSuggestion();
  if (!SetCaretInTableCell(aElement)) {
    if (NS_WARN_IF(Destroyed())) {
      return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    }
    nsresult rv = CollapseSelectionTo(EditorRawDOMPoint::After(*aElement));
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::CollapseSelectionTo() failed");
      return EditorBase::ToGenericNSResult(rv);
    }
  }

  // check for inserting a whole table at the end of a block. If so insert
  // a br after it.
  if (!HTMLEditUtils::IsTable(aElement) ||
      !HTMLEditUtils::IsLastChild(*aElement,
                                  {WalkTreeOption::IgnoreNonEditableNode})) {
    return NS_OK;
  }

  const auto afterElement = EditorDOMPoint::After(*aElement);
  // Collapse selection to the new `<br>` element node after creating it.
  const CreateElementResult insertBRElementResult =
      InsertBRElement(WithTransaction::Yes, afterElement, ePrevious);
  if (insertBRElementResult.isErr()) {
    NS_WARNING(
        "HTMLEditor::InsertBRElement(WithTransaction::Yes, ePrevious) failed");
    return EditorBase::ToGenericNSResult(insertBRElementResult.unwrapErr());
  }
  rv = insertBRElementResult.SuggestCaretPointTo(*this, {});
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CollapseSelectionTo() failed");
  MOZ_ASSERT(insertBRElementResult.GetNewNode());
  return EditorBase::ToGenericNSResult(rv);
}

template <typename NodeType>
CreateNodeResultBase<NodeType>
HTMLEditor::InsertNodeIntoProperAncestorWithTransaction(
    NodeType& aContentToInsert, const EditorDOMPoint& aPointToInsert,
    SplitAtEdges aSplitAtEdges) {
  using ResultType = CreateNodeResultBase<NodeType>;

  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return ResultType(NS_ERROR_FAILURE);
  }
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  // Search up the parent chain to find a suitable container.
  EditorDOMPoint pointToInsert(aPointToInsert);
  MOZ_ASSERT(pointToInsert.IsSet());
  while (!HTMLEditUtils::CanNodeContain(*pointToInsert.GetContainer(),
                                        aContentToInsert)) {
    // If the current parent is a root (body or table element)
    // then go no further - we can't insert.
    if (MOZ_UNLIKELY(
            pointToInsert.IsContainerHTMLElement(nsGkAtoms::body) ||
            HTMLEditUtils::IsAnyTableElement(pointToInsert.GetContainer()))) {
      NS_WARNING(
          "There was no proper container element to insert the content node in "
          "the document");
      return ResultType(NS_ERROR_FAILURE);
    }

    // Get the next point.
    pointToInsert = pointToInsert.ParentPoint();

    if (MOZ_UNLIKELY(
            !pointToInsert.IsInContentNode() ||
            !EditorUtils::IsEditableContent(*pointToInsert.ContainerAsContent(),
                                            EditorType::HTML))) {
      NS_WARNING(
          "There was no proper container element to insert the content node in "
          "the editing host");
      return ResultType(NS_ERROR_FAILURE);
    }
  }

  if (pointToInsert != aPointToInsert) {
    // We need to split some levels above the original selection parent.
    MOZ_ASSERT(pointToInsert.GetChild());
    const SplitNodeResult splitNodeResult =
        SplitNodeDeepWithTransaction(MOZ_KnownLive(*pointToInsert.GetChild()),
                                     aPointToInsert, aSplitAtEdges);
    if (splitNodeResult.isErr()) {
      NS_WARNING("HTMLEditor::SplitNodeDeepWithTransaction() failed");
      return ResultType(splitNodeResult.unwrapErr());
    }
    pointToInsert = splitNodeResult.AtSplitPoint<EditorDOMPoint>();
    MOZ_ASSERT(pointToInsert.IsSet());
    // Caret should be set by the caller of this method so that we don't
    // need to handle it here.
    splitNodeResult.IgnoreCaretPointSuggestion();
  }

  // Now we can insert the new node.
  ResultType insertContentNodeResult =
      InsertNodeWithTransaction<NodeType>(aContentToInsert, pointToInsert);
  if (MOZ_LIKELY(insertContentNodeResult.isOk()) &&
      MOZ_UNLIKELY(NS_WARN_IF(!aContentToInsert.GetParentNode()) ||
                   NS_WARN_IF(aContentToInsert.GetParentNode() !=
                              pointToInsert.GetContainer()))) {
    NS_WARNING(
        "EditorBase::InsertNodeWithTransaction() succeeded, but the inserted "
        "node was moved or removed by the web app");
    insertContentNodeResult.IgnoreCaretPointSuggestion();
    return ResultType(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  NS_WARNING_ASSERTION(insertContentNodeResult.isOk(),
                       "EditorBase::InsertNodeWithTransaction() failed");
  return insertContentNodeResult;
}

NS_IMETHODIMP HTMLEditor::SelectElement(Element* aElement) {
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = SelectContentInternal(MOZ_KnownLive(*aElement));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::SelectContentInternal() failed");
  return rv;
}

nsresult HTMLEditor::SelectContentInternal(nsIContent& aContentToSelect) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Must be sure that element is contained in the editing host
  const RefPtr<Element> editingHost = ComputeEditingHost();
  if (NS_WARN_IF(!editingHost) ||
      NS_WARN_IF(!aContentToSelect.IsInclusiveDescendantOf(editingHost))) {
    return NS_ERROR_FAILURE;
  }

  EditorRawDOMPoint newSelectionStart(&aContentToSelect);
  if (NS_WARN_IF(!newSelectionStart.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  EditorRawDOMPoint newSelectionEnd(EditorRawDOMPoint::After(aContentToSelect));
  MOZ_ASSERT(newSelectionEnd.IsSet());
  ErrorResult error;
  SelectionRef().SetStartAndEndInLimiter(newSelectionStart, newSelectionEnd,
                                         error);
  NS_WARNING_ASSERTION(!error.Failed(),
                       "Selection::SetStartAndEndInLimiter() failed");
  return error.StealNSResult();
}

nsresult HTMLEditor::AppendContentToSelectionAsRange(nsIContent& aContent) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  EditorRawDOMPoint atContent(&aContent);
  if (NS_WARN_IF(!atContent.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsRange> range = nsRange::Create(
      atContent.ToRawRangeBoundary(),
      atContent.NextPoint().ToRawRangeBoundary(), IgnoreErrors());
  if (NS_WARN_IF(!range)) {
    NS_WARNING("nsRange::Create() failed");
    return NS_ERROR_FAILURE;
  }

  ErrorResult error;
  SelectionRef().AddRangeAndSelectFramesAndNotifyListeners(*range, error);
  if (NS_WARN_IF(Destroyed())) {
    if (error.Failed()) {
      error.SuppressException();
    }
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(!error.Failed(), "Failed to add range to Selection");
  return error.StealNSResult();
}

nsresult HTMLEditor::ClearSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  ErrorResult error;
  SelectionRef().RemoveAllRanges(error);
  if (NS_WARN_IF(Destroyed())) {
    if (error.Failed()) {
      error.SuppressException();
    }
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(!error.Failed(), "Selection::RemoveAllRanges() failed");
  return error.StealNSResult();
}

nsresult HTMLEditor::SetParagraphFormatAsAction(
    const nsAString& aParagraphFormat, nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eInsertBlockElement, aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // TODO: Computing the editing host here makes the `execCommand` in
  //       docshell/base/crashtests/file_432114-2.xhtml cannot run
  //       `DOMNodeRemoved` event listener with deleting the bogus <br> element.
  //       So that it should be rewritten with different mutation event listener
  //       since we'd like to stop using it.

  nsAutoString lowerCaseTagName(aParagraphFormat);
  ToLowerCase(lowerCaseTagName);
  RefPtr<nsAtom> tagName = NS_Atomize(lowerCaseTagName);
  MOZ_ASSERT(tagName);
  if (tagName == nsGkAtoms::dd || tagName == nsGkAtoms::dt) {
    EditActionResult result = MakeOrChangeListAndListItemAsSubAction(
        *tagName, u""_ns, SelectAllOfCurrentList::No);
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::MakeOrChangeListAndListItemAsSubAction("
                         "SelectAllOfCurrentList::No) failed");
    return EditorBase::ToGenericNSResult(result.Rv());
  }

  rv = FormatBlockContainerAsSubAction(*tagName);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::FormatBlockContainerAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::GetParagraphState(bool* aMixed,
                                            nsAString& aFirstParagraphState) {
  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!mInitSucceeded) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  ErrorResult error;
  ParagraphStateAtSelection paragraphState(*this, error);
  if (error.Failed()) {
    NS_WARNING("ParagraphStateAtSelection failed");
    return error.StealNSResult();
  }

  *aMixed = paragraphState.IsMixed();
  if (NS_WARN_IF(!paragraphState.GetFirstParagraphStateAtSelection())) {
    // XXX Odd result, but keep this behavior for now...
    aFirstParagraphState.AssignASCII("x");
  } else {
    paragraphState.GetFirstParagraphStateAtSelection()->ToString(
        aFirstParagraphState);
  }
  return NS_OK;
}

nsresult HTMLEditor::GetBackgroundColorState(bool* aMixed,
                                             nsAString& aOutColor) {
  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (IsCSSEnabled()) {
    // if we are in CSS mode, we have to check if the containing block defines
    // a background color
    nsresult rv = GetCSSBackgroundColorState(aMixed, aOutColor, true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::GetCSSBackgroundColorState() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  // in HTML mode, we look only at page's background
  nsresult rv = GetHTMLBackgroundColorState(aMixed, aOutColor);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::GetCSSBackgroundColorState() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::GetHighlightColorState(bool* aMixed,
                                                 nsAString& aOutColor) {
  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aMixed = false;
  aOutColor.AssignLiteral("transparent");
  if (!IsCSSEnabled()) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // in CSS mode, text background can be added by the Text Highlight button
  // we need to query the background of the selection without looking for
  // the block container of the ranges in the selection
  nsresult rv = GetCSSBackgroundColorState(aMixed, aOutColor, false);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::GetCSSBackgroundColorState() failed");
  return rv;
}

nsresult HTMLEditor::GetCSSBackgroundColorState(bool* aMixed,
                                                nsAString& aOutColor,
                                                bool aBlockLevel) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aMixed = false;
  // the default background color is transparent
  aOutColor.AssignLiteral("transparent");

  RefPtr<const nsRange> firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> startContainer = firstRange->GetStartContainer();
  if (NS_WARN_IF(!startContainer) || NS_WARN_IF(!startContainer->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  // is the selection collapsed?
  nsIContent* contentToExamine;
  if (SelectionRef().IsCollapsed() || startContainer->IsText()) {
    if (NS_WARN_IF(!startContainer->IsContent())) {
      return NS_ERROR_FAILURE;
    }
    // we want to look at the startContainer and ancestors
    contentToExamine = startContainer->AsContent();
  } else {
    // otherwise we want to look at the first editable node after
    // {startContainer,offset} and its ancestors for divs with alignment on them
    contentToExamine = firstRange->GetChildAtStartOffset();
    // GetNextNode(startContainer, offset, true, address_of(contentToExamine));
  }

  if (NS_WARN_IF(!contentToExamine)) {
    return NS_ERROR_FAILURE;
  }

  if (aBlockLevel) {
    // we are querying the block background (and not the text background), let's
    // climb to the block container.  Note that background color of ancestor
    // of editing host may be what the caller wants to know.  Therefore, we
    // should ignore the editing host boundaries.
    Element* const closestBlockElement =
        HTMLEditUtils::GetInclusiveAncestorElement(
            *contentToExamine, HTMLEditUtils::ClosestBlockElement);
    if (NS_WARN_IF(!closestBlockElement)) {
      return NS_OK;
    }

    for (RefPtr<Element> blockElement = closestBlockElement; blockElement;) {
      RefPtr<Element> nextBlockElement = HTMLEditUtils::GetAncestorElement(
          *blockElement, HTMLEditUtils::ClosestBlockElement);
      DebugOnly<nsresult> rvIgnored = CSSEditUtils::GetComputedProperty(
          *blockElement, *nsGkAtoms::backgroundColor, aOutColor);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (MayHaveMutationEventListeners() &&
          NS_WARN_IF(nextBlockElement !=
                     HTMLEditUtils::GetAncestorElement(
                         *blockElement, HTMLEditUtils::ClosestBlockElement))) {
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "CSSEditUtils::GetComputedProperty(nsGkAtoms::"
                           "backgroundColor) failed, but ignored");
      // look at parent if the queried color is transparent and if the node to
      // examine is not the root of the document
      if (!aOutColor.EqualsLiteral("transparent") &&
          !aOutColor.EqualsLiteral("rgba(0, 0, 0, 0)")) {
        break;
      }
      blockElement = std::move(nextBlockElement);
    }

    if (aOutColor.EqualsLiteral("transparent") ||
        aOutColor.EqualsLiteral("rgba(0, 0, 0, 0)")) {
      // we have hit the root of the document and the color is still transparent
      // ! Grumble... Let's look at the default background color because that's
      // the color we are looking for
      CSSEditUtils::GetDefaultBackgroundColor(aOutColor);
    }
  } else {
    // no, we are querying the text background for the Text Highlight button
    if (contentToExamine->IsText()) {
      // if the node of interest is a text node, let's climb a level
      contentToExamine = contentToExamine->GetParent();
    }
    // Return default value due to no parent node
    if (!contentToExamine) {
      return NS_OK;
    }

    for (RefPtr<Element> element =
             contentToExamine->GetAsElementOrParentElement();
         element; element = element->GetParentElement()) {
      // is the node to examine a block ?
      if (HTMLEditUtils::IsBlockElement(*element)) {
        // yes it is a block; in that case, the text background color is
        // transparent
        aOutColor.AssignLiteral("transparent");
        break;
      }

      // no, it's not; let's retrieve the computed style of background-color
      // for the node to examine
      nsCOMPtr<nsINode> parentNode = element->GetParentNode();
      DebugOnly<nsresult> rvIgnored = CSSEditUtils::GetComputedProperty(
          *element, *nsGkAtoms::backgroundColor, aOutColor);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(parentNode != element->GetParentNode())) {
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "CSSEditUtils::GetComputedProperty(nsGkAtoms::"
                           "backgroundColor) failed, but ignored");
      if (!aOutColor.EqualsLiteral("transparent")) {
        break;
      }
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::GetHTMLBackgroundColorState(bool* aMixed,
                                                 nsAString& aOutColor) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // TODO: We don't handle "mixed" correctly!
  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aMixed = false;
  aOutColor.Truncate();

  Result<RefPtr<Element>, nsresult> cellOrRowOrTableElementOrError =
      GetSelectedOrParentTableElement();
  if (cellOrRowOrTableElementOrError.isErr()) {
    NS_WARNING("HTMLEditor::GetSelectedOrParentTableElement() returned error");
    return cellOrRowOrTableElementOrError.unwrapErr();
  }

  for (RefPtr<Element> element = cellOrRowOrTableElementOrError.unwrap();
       element; element = element->GetParentElement()) {
    // We are in a cell or selected table
    element->GetAttr(kNameSpaceID_None, nsGkAtoms::bgcolor, aOutColor);

    // Done if we have a color explicitly set
    if (!aOutColor.IsEmpty()) {
      return NS_OK;
    }

    // Once we hit the body, we're done
    if (element->IsHTMLElement(nsGkAtoms::body)) {
      return NS_OK;
    }

    // No color is set, but we need to report visible color inherited
    // from nested cells/tables, so search up parent chain so that
    // let's keep checking the ancestors.
  }

  // If no table or cell found, get page body
  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  rootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::bgcolor, aOutColor);
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetListState(bool* aMixed, bool* aOL, bool* aUL,
                                       bool* aDL) {
  if (NS_WARN_IF(!aMixed) || NS_WARN_IF(!aOL) || NS_WARN_IF(!aUL) ||
      NS_WARN_IF(!aDL)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!mInitSucceeded) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  ErrorResult error;
  ListElementSelectionState state(*this, error);
  if (error.Failed()) {
    NS_WARNING("ListElementSelectionState failed");
    return error.StealNSResult();
  }

  *aMixed = state.IsNotOneTypeListElementSelected();
  *aOL = state.IsOLElementSelected();
  *aUL = state.IsULElementSelected();
  *aDL = state.IsDLElementSelected();
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetListItemState(bool* aMixed, bool* aLI, bool* aDT,
                                           bool* aDD) {
  if (NS_WARN_IF(!aMixed) || NS_WARN_IF(!aLI) || NS_WARN_IF(!aDT) ||
      NS_WARN_IF(!aDD)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!mInitSucceeded) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  ErrorResult error;
  ListItemElementSelectionState state(*this, error);
  if (error.Failed()) {
    NS_WARNING("ListItemElementSelectionState failed");
    return error.StealNSResult();
  }

  // XXX Why do we ignore `<li>` element selected state?
  *aMixed = state.IsNotOneTypeDefinitionListItemElementSelected();
  *aLI = state.IsLIElementSelected();
  *aDT = state.IsDTElementSelected();
  *aDD = state.IsDDElementSelected();
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetAlignment(bool* aMixed,
                                       nsIHTMLEditor::EAlignment* aAlign) {
  if (NS_WARN_IF(!aMixed) || NS_WARN_IF(!aAlign)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!mInitSucceeded) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  ErrorResult error;
  AlignStateAtSelection state(*this, error);
  if (error.Failed()) {
    NS_WARNING("AlignStateAtSelection failed");
    return error.StealNSResult();
  }

  *aMixed = false;
  *aAlign = state.AlignmentAtSelectionStart();
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::MakeOrChangeList(const nsAString& aListType,
                                           bool aEntireList,
                                           const nsAString& aBulletType) {
  RefPtr<nsAtom> listTagName = NS_Atomize(aListType);
  if (NS_WARN_IF(!listTagName)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv = MakeOrChangeListAsAction(
      *listTagName, aBulletType,
      aEntireList ? SelectAllOfCurrentList::Yes : SelectAllOfCurrentList::No);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::MakeOrChangeListAsAction() failed");
  return rv;
}

nsresult HTMLEditor::MakeOrChangeListAsAction(
    nsAtom& aListTagName, const nsAString& aBulletType,
    SelectAllOfCurrentList aSelectAllOfCurrentList, nsIPrincipal* aPrincipal) {
  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(
      *this, HTMLEditUtils::GetEditActionForInsert(aListTagName), aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  EditActionResult result = MakeOrChangeListAndListItemAsSubAction(
      aListTagName, aBulletType, aSelectAllOfCurrentList);
  NS_WARNING_ASSERTION(
      result.Succeeded(),
      "HTMLEditor::MakeOrChangeListAndListItemAsSubAction() failed");
  return EditorBase::ToGenericNSResult(result.Rv());
}

NS_IMETHODIMP HTMLEditor::RemoveList(const nsAString& aListType) {
  nsresult rv = RemoveListAsAction(aListType);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::RemoveListAsAction() failed");
  return rv;
}

nsresult HTMLEditor::RemoveListAsAction(const nsAString& aListType,
                                        nsIPrincipal* aPrincipal) {
  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Note that we ignore aListType when we actually remove parent list elements.
  // However, we need to set InputEvent.inputType to "insertOrderedList" or
  // "insertedUnorderedList" when this is called for
  // execCommand("insertorderedlist") or execCommand("insertunorderedlist").
  // Otherwise, comm-central UI may call this methods with "dl" or "".
  // So, it's okay to use mismatched EditAction here if this is called in
  // comm-central.

  RefPtr<nsAtom> listAtom = NS_Atomize(aListType);
  if (NS_WARN_IF(!listAtom)) {
    return NS_ERROR_INVALID_ARG;
  }
  AutoEditActionDataSetter editActionData(
      *this, HTMLEditUtils::GetEditActionForRemoveList(*listAtom), aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  const RefPtr<Element> editingHost = ComputeEditingHost();
  if (!editingHost) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  rv = RemoveListAtSelectionAsSubAction(*editingHost);
  NS_WARNING_ASSERTION(NS_FAILED(rv),
                       "HTMLEditor::RemoveListAtSelectionAsSubAction() failed");
  return rv;
}

nsresult HTMLEditor::FormatBlockContainerAsSubAction(nsAtom& aTagName) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(&aTagName != nsGkAtoms::dd && &aTagName != nsGkAtoms::dt);

  AutoPlaceholderBatch treatAsOneTransaction(
      *this, ScrollSelectionIntoView::Yes, __FUNCTION__);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eCreateOrRemoveBlock, nsIEditor::eNext,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result.Rv();
  }

  if (IsSelectionRangeContainerNotContent()) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  // FormatBlockContainerWithTransaction() creates AutoSelectionRestorer.
  // Therefore, even if it returns NS_OK, editor might have been destroyed
  // at restoring Selection.
  const RefPtr<Element> editingHost = ComputeEditingHost();
  if (MOZ_UNLIKELY(!editingHost)) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }
  AutoRangeArray selectionRanges(SelectionRef());
  rv = FormatBlockContainerWithTransaction(selectionRanges, aTagName,
                                           *editingHost);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::FormatBlockContainerWithTransaction() failed");
    return rv;
  }

  if (selectionRanges.HasSavedRanges()) {
    selectionRanges.RestoreFromSavedRanges();
  }

  rv = selectionRanges.ApplyTo(SelectionRef());
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("AutoRangeArray::ApplyTo(SelectionRef()) failed, but ignored");
    return rv;
  }

  rv = MaybeInsertPaddingBRElementForEmptyLastLineAtSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::MaybeInsertPaddingBRElementForEmptyLastLineAtSelection() "
      "failed");
  return rv;
}

nsresult HTMLEditor::IndentAsAction(nsIPrincipal* aPrincipal) {
  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eIndent,
                                          aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  const RefPtr<Element> editingHost = ComputeEditingHost();
  if (!editingHost) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  EditActionResult result = IndentAsSubAction(*editingHost);
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "HTMLEditor::IndentAsSubAction() failed");
  return EditorBase::ToGenericNSResult(result.Rv());
}

nsresult HTMLEditor::OutdentAsAction(nsIPrincipal* aPrincipal) {
  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eOutdent,
                                          aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  const RefPtr<Element> editingHost = ComputeEditingHost();
  if (!editingHost) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  EditActionResult result = OutdentAsSubAction(*editingHost);
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "HTMLEditor::OutdentAsSubAction() failed");
  return EditorBase::ToGenericNSResult(result.Rv());
}

// TODO: IMPLEMENT ALIGNMENT!

nsresult HTMLEditor::AlignAsAction(const nsAString& aAlignType,
                                   nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this, HTMLEditUtils::GetEditActionForAlignment(aAlignType), aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  const RefPtr<Element> editingHost = ComputeEditingHost();
  if (!editingHost) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  EditActionResult result = AlignAsSubAction(aAlignType, *editingHost);
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "HTMLEditor::AlignAsSubAction() failed");
  return EditorBase::ToGenericNSResult(result.Rv());
}

Element* HTMLEditor::GetInclusiveAncestorByTagName(const nsStaticAtom& aTagName,
                                                   nsIContent& aContent) const {
  MOZ_ASSERT(&aTagName != nsGkAtoms::_empty);

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return nullptr;
  }

  return GetInclusiveAncestorByTagNameInternal(aTagName, aContent);
}

Element* HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(
    const nsStaticAtom& aTagName) const {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(&aTagName != nsGkAtoms::_empty);

  // If no node supplied, get it from anchor node of current selection
  const EditorRawDOMPoint atAnchor(SelectionRef().AnchorRef());
  if (NS_WARN_IF(!atAnchor.IsSet()) ||
      NS_WARN_IF(!atAnchor.GetContainerAsContent())) {
    return nullptr;
  }

  // Try to get the actual selected node
  nsIContent* content = nullptr;
  if (atAnchor.GetContainer()->HasChildNodes() &&
      atAnchor.GetContainerAsContent()) {
    content = atAnchor.GetChild();
  }
  // Anchor node is probably a text node - just use that
  if (!content) {
    content = atAnchor.GetContainerAsContent();
    if (NS_WARN_IF(!content)) {
      return nullptr;
    }
  }
  return GetInclusiveAncestorByTagNameInternal(aTagName, *content);
}

Element* HTMLEditor::GetInclusiveAncestorByTagNameInternal(
    const nsStaticAtom& aTagName, const nsIContent& aContent) const {
  MOZ_ASSERT(&aTagName != nsGkAtoms::_empty);

  Element* currentElement = aContent.GetAsElementOrParentElement();
  if (NS_WARN_IF(!currentElement)) {
    MOZ_ASSERT(!aContent.GetParentNode());
    return nullptr;
  }

  bool lookForLink = IsLinkTag(aTagName);
  bool lookForNamedAnchor = IsNamedAnchorTag(aTagName);
  for (Element* element : currentElement->InclusiveAncestorsOfType<Element>()) {
    // Stop searching if parent is a body element.  Note: Originally used
    // IsRoot() to/ stop at table cells, but that's too messy when you are
    // trying to find the parent table.
    if (element->IsHTMLElement(nsGkAtoms::body)) {
      return nullptr;
    }
    if (lookForLink) {
      // Test if we have a link (an anchor with href set)
      if (HTMLEditUtils::IsLink(element)) {
        return element;
      }
    } else if (lookForNamedAnchor) {
      // Test if we have a named anchor (an anchor with name set)
      if (HTMLEditUtils::IsNamedAnchor(element)) {
        return element;
      }
    } else if (&aTagName == nsGkAtoms::list_) {
      // Match "ol", "ul", or "dl" for lists
      if (HTMLEditUtils::IsAnyListElement(element)) {
        return element;
      }
    } else if (&aTagName == nsGkAtoms::td) {
      // Table cells are another special case: match either "td" or "th"
      if (HTMLEditUtils::IsTableCell(element)) {
        return element;
      }
    } else if (&aTagName == element->NodeInfo()->NameAtom()) {
      return element;
    }
  }
  return nullptr;
}

NS_IMETHODIMP HTMLEditor::GetElementOrParentByTagName(const nsAString& aTagName,
                                                      nsINode* aNode,
                                                      Element** aReturn) {
  if (NS_WARN_IF(aTagName.IsEmpty()) || NS_WARN_IF(!aReturn)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsStaticAtom* tagName = EditorUtils::GetTagNameAtom(aTagName);
  if (NS_WARN_IF(!tagName)) {
    // We don't need to support custom elements since this is an internal API.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }
  if (NS_WARN_IF(tagName == nsGkAtoms::_empty)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aNode) {
    AutoEditActionDataSetter dummyEditAction(*this, EditAction::eNotEditing);
    if (NS_WARN_IF(!dummyEditAction.CanHandle())) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    RefPtr<Element> parentElement =
        GetInclusiveAncestorByTagNameAtSelection(*tagName);
    if (!parentElement) {
      return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
    }
    parentElement.forget(aReturn);
    return NS_OK;
  }

  if (!aNode->IsContent() || !aNode->GetAsElementOrParentElement()) {
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  RefPtr<Element> parentElement =
      GetInclusiveAncestorByTagName(*tagName, *aNode->AsContent());
  if (!parentElement) {
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }
  parentElement.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetSelectedElement(const nsAString& aTagName,
                                             nsISupports** aReturn) {
  if (NS_WARN_IF(!aReturn)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aReturn = nullptr;

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  ErrorResult error;
  nsStaticAtom* tagName = EditorUtils::GetTagNameAtom(aTagName);
  if (!aTagName.IsEmpty() && !tagName) {
    // We don't need to support custom elements becaus of internal API.
    return NS_OK;
  }
  RefPtr<nsINode> selectedNode = GetSelectedElement(tagName, error);
  NS_WARNING_ASSERTION(!error.Failed(),
                       "HTMLEditor::GetSelectedElement() failed");
  selectedNode.forget(aReturn);
  return error.StealNSResult();
}

already_AddRefed<Element> HTMLEditor::GetSelectedElement(const nsAtom* aTagName,
                                                         ErrorResult& aRv) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(!aRv.Failed());

  // If there is no Selection or two or more selection ranges, that means that
  // not only one element is selected so that return nullptr.
  if (SelectionRef().RangeCount() != 1) {
    return nullptr;
  }

  bool isLinkTag = aTagName && IsLinkTag(*aTagName);
  bool isNamedAnchorTag = aTagName && IsNamedAnchorTag(*aTagName);

  RefPtr<nsRange> firstRange = SelectionRef().GetRangeAt(0);
  MOZ_ASSERT(firstRange);

  const RangeBoundary& startRef = firstRange->StartRef();
  if (NS_WARN_IF(!startRef.IsSet())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  const RangeBoundary& endRef = firstRange->EndRef();
  if (NS_WARN_IF(!endRef.IsSet())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Optimization for a single selected element
  if (startRef.Container() == endRef.Container()) {
    nsIContent* startContent = startRef.GetChildAtOffset();
    nsIContent* endContent = endRef.GetChildAtOffset();
    if (startContent && endContent &&
        startContent->GetNextSibling() == endContent) {
      if (!aTagName) {
        if (!startContent->IsElement()) {
          // This means only a text node or something is selected.  We should
          // return nullptr in this case since no other elements are selected.
          return nullptr;
        }
        return do_AddRef(startContent->AsElement());
      }
      // Test for appropriate node type requested
      if (aTagName == startContent->NodeInfo()->NameAtom() ||
          (isLinkTag && HTMLEditUtils::IsLink(startContent)) ||
          (isNamedAnchorTag && HTMLEditUtils::IsNamedAnchor(startContent))) {
        MOZ_ASSERT(startContent->IsElement());
        return do_AddRef(startContent->AsElement());
      }
    }
  }

  if (isLinkTag && startRef.Container()->IsContent() &&
      endRef.Container()->IsContent()) {
    // Link node must be the same for both ends of selection.
    Element* parentLinkOfStart = GetInclusiveAncestorByTagNameInternal(
        *nsGkAtoms::href, *startRef.Container()->AsContent());
    if (parentLinkOfStart) {
      if (SelectionRef().IsCollapsed()) {
        // We have just a caret in the link.
        return do_AddRef(parentLinkOfStart);
      }
      // Link node must be the same for both ends of selection.
      Element* parentLinkOfEnd = GetInclusiveAncestorByTagNameInternal(
          *nsGkAtoms::href, *endRef.Container()->AsContent());
      if (parentLinkOfStart == parentLinkOfEnd) {
        return do_AddRef(parentLinkOfStart);
      }
    }
  }

  if (SelectionRef().IsCollapsed()) {
    return nullptr;
  }

  PostContentIterator postOrderIter;
  postOrderIter.Init(firstRange);

  RefPtr<Element> lastElementInRange;
  for (nsINode* lastNodeInRange = nullptr; !postOrderIter.IsDone();
       postOrderIter.Next()) {
    if (lastElementInRange) {
      // When any node follows an element node, not only one element is
      // selected so that return nullptr.
      return nullptr;
    }

    // This loop ignors any non-element nodes before first element node.
    // Its purpose must be that this method treats this case as selecting
    // the <b> element:
    // - <p>abc <b>d[ef</b>}</p>
    // because children of an element node is listed up before the element.
    // However, this case must not be expected by the initial developer:
    // - <p>a[bc <b>def</b>}</p>
    // When we meet non-parent and non-next-sibling node of previous node,
    // it means that the range across element boundary (open tag in HTML
    // source).  So, in this case, we should not say only the following
    // element is selected.
    nsINode* currentNode = postOrderIter.GetCurrentNode();
    MOZ_ASSERT(currentNode);
    if (lastNodeInRange && lastNodeInRange->GetParentNode() != currentNode &&
        lastNodeInRange->GetNextSibling() != currentNode) {
      return nullptr;
    }

    lastNodeInRange = currentNode;

    lastElementInRange = Element::FromNodeOrNull(lastNodeInRange);
    if (!lastElementInRange) {
      continue;
    }

    // And also, if it's followed by a <br> element, we shouldn't treat the
    // the element is selected like this case:
    // - <p><b>[def</b>}<br></p>
    // Note that we don't need special handling for <a href> because double
    // clicking it selects the element and we use the first path to handle it.
    // Additionally, we have this case too:
    // - <p><b>[def</b><b>}<br></b></p>
    // In these cases, the <br> element is not listed up by PostContentIterator.
    // So, we should return nullptr if next sibling is a `<br>` element or
    // next sibling starts with `<br>` element.
    if (nsIContent* nextSibling = lastElementInRange->GetNextSibling()) {
      if (nextSibling->IsHTMLElement(nsGkAtoms::br)) {
        return nullptr;
      }
      nsIContent* firstEditableLeaf = HTMLEditUtils::GetFirstLeafContent(
          *nextSibling, {LeafNodeType::OnlyLeafNode});
      if (firstEditableLeaf &&
          firstEditableLeaf->IsHTMLElement(nsGkAtoms::br)) {
        return nullptr;
      }
    }

    if (!aTagName) {
      continue;
    }

    if (isLinkTag && HTMLEditUtils::IsLink(lastElementInRange)) {
      continue;
    }

    if (isNamedAnchorTag && HTMLEditUtils::IsNamedAnchor(lastElementInRange)) {
      continue;
    }

    if (aTagName == lastElementInRange->NodeInfo()->NameAtom()) {
      continue;
    }

    // First element in the range does not match what the caller is looking
    // for.
    return nullptr;
  }
  return lastElementInRange.forget();
}

CreateElementResult HTMLEditor::CreateAndInsertElement(
    WithTransaction aWithTransaction, nsAtom& aTagName,
    const EditorDOMPoint& aPointToInsert,
    const InitializeInsertingElement& aInitializer) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  // XXX We need offset at new node for RangeUpdaterRef().  Therefore, we need
  //     to compute the offset now but this is expensive.  So, if it's possible,
  //     we need to redesign RangeUpdaterRef() as avoiding using indices.
  Unused << aPointToInsert.Offset();

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eCreateNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // TODO: This method should have a callback function which is called
  //       immediately after creating an element but before it's inserted into
  //       the DOM tree.  Then, caller can init the new element's attributes
  //       and children **without** transactions (it'll reduce the number of
  //       legacy mutation events).  Finally, we can get rid of
  //       CreatElementTransaction since we can use InsertNodeTransaction
  //       instead.

  CreateElementResult createNewElementResult = [&]() MOZ_CAN_RUN_SCRIPT {
    RefPtr<Element> newElement = CreateHTMLContent(&aTagName);
    if (MOZ_UNLIKELY(!newElement)) {
      NS_WARNING("EditorBase::CreateHTMLContent() failed");
      return CreateElementResult(NS_ERROR_FAILURE);
    }
    nsresult rv = MarkElementDirty(*newElement);
    if (MOZ_UNLIKELY(rv == NS_ERROR_EDITOR_DESTROYED)) {
      NS_WARNING("EditorBase::MarkElementDirty() caused destroying the editor");
      return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::MarkElementDirty() failed, but ignored");
    if (StaticPrefs::editor_initialize_element_before_connect()) {
      rv = aInitializer(*this, *newElement, aPointToInsert);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "aInitializer failed");
      if (NS_WARN_IF(Destroyed())) {
        return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
      }
    }
    RefPtr<InsertNodeTransaction> transaction =
        InsertNodeTransaction::Create(*this, *newElement, aPointToInsert);
    rv = aWithTransaction == WithTransaction::Yes
             ? DoTransactionInternal(transaction)
             : transaction->DoTransaction();
    // FYI: Transaction::DoTransaction never returns NS_ERROR_EDITOR_*.
    if (MOZ_UNLIKELY(Destroyed())) {
      NS_WARNING(
          "InsertNodeTransaction::DoTransaction() caused destroying the "
          "editor");
      return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("InsertNodeTransaction::DoTransaction() failed");
      return CreateElementResult(rv);
    }
    // Override the success code if new element was moved by the web apps.
    if (newElement &&
        newElement->GetParentNode() != aPointToInsert.GetContainer()) {
      NS_WARNING("The new element was not inserted into the expected node");
      return CreateElementResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
    return CreateElementResult(
        std::move(newElement),
        transaction->SuggestPointToPutCaret<EditorDOMPoint>());
  }();

  if (createNewElementResult.isErr()) {
    NS_WARNING("EditorBase::DoTransactionInternal() failed");
    // XXX Why do we do this even when DoTransaction() returned error?
    DebugOnly<nsresult> rvIgnored =
        RangeUpdaterRef().SelAdjCreateNode(aPointToInsert);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Rangeupdater::SelAdjCreateNode() failed");
    return createNewElementResult;
  }

  // If we succeeded to create and insert new element, we need to adjust
  // ranges in RangeUpdaterRef().  It currently requires offset of the new
  // node.  So, let's call it with original offset.  Note that if
  // aPointToInsert stores child node, it may not be at the offset since new
  // element must be inserted before the old child.  Although, mutation
  // observer can do anything, but currently, we don't check it.
  DebugOnly<nsresult> rvIgnored =
      RangeUpdaterRef().SelAdjCreateNode(EditorRawDOMPoint(
          aPointToInsert.GetContainer(), aPointToInsert.Offset()));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "Rangeupdater::SelAdjCreateNode() failed, but ignored");
  if (createNewElementResult.GetNewNode()) {
    TopLevelEditSubActionDataRef().DidCreateElement(
        *this, *createNewElementResult.GetNewNode());
  }

  if (!StaticPrefs::editor_initialize_element_before_connect() &&
      createNewElementResult.GetNewNode()) {
    // MOZ_KnownLive(newElement) because it's grabbed by createNewElementResult.
    nsresult rv =
        aInitializer(*this, MOZ_KnownLive(*createNewElementResult.GetNewNode()),
                     aPointToInsert);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "aInitializer failed");
    if (MOZ_UNLIKELY(NS_WARN_IF(Destroyed()))) {
      return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      return CreateElementResult(rv);
    }
  }

  return createNewElementResult;
}

already_AddRefed<Element> HTMLEditor::CreateElementWithDefaults(
    const nsAtom& aTagName) {
  // NOTE: Despite of public method, this can be called for internal use.

  // Although this creates an element, but won't change the DOM tree nor
  // transaction.  So, EditAtion::eNotEditing is proper value here.  If
  // this is called for internal when there is already AutoEditActionDataSetter
  // instance, this would be initialized with its EditAction value.
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return nullptr;
  }

  const nsAtom* realTagName = IsLinkTag(aTagName) || IsNamedAnchorTag(aTagName)
                                  ? nsGkAtoms::a
                                  : &aTagName;

  // We don't use editor's CreateElement because we don't want to go through
  // the transaction system

  // New call to use instead to get proper HTML element, bug 39919
  RefPtr<Element> newElement = CreateHTMLContent(realTagName);
  if (!newElement) {
    return nullptr;
  }

  // Mark the new element dirty, so it will be formatted
  // XXX Don't we need to check the error result of setting _moz_dirty attr?
  IgnoredErrorResult ignoredError;
  newElement->SetAttribute(u"_moz_dirty"_ns, u""_ns, ignoredError);
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "Element::SetAttribute(_moz_dirty) failed, but ignored");
  ignoredError.SuppressException();

  // Set default values for new elements
  if (realTagName == nsGkAtoms::table) {
    newElement->SetAttr(nsGkAtoms::cellpadding, u"2"_ns, ignoredError);
    if (ignoredError.Failed()) {
      NS_WARNING("Element::SetAttr(nsGkAtoms::cellpadding, 2) failed");
      return nullptr;
    }
    ignoredError.SuppressException();

    newElement->SetAttr(nsGkAtoms::cellspacing, u"2"_ns, ignoredError);
    if (ignoredError.Failed()) {
      NS_WARNING("Element::SetAttr(nsGkAtoms::cellspacing, 2) failed");
      return nullptr;
    }
    ignoredError.SuppressException();

    newElement->SetAttr(nsGkAtoms::border, u"1"_ns, ignoredError);
    if (ignoredError.Failed()) {
      NS_WARNING("Element::SetAttr(nsGkAtoms::border, 1) failed");
      return nullptr;
    }
  } else if (realTagName == nsGkAtoms::td) {
    nsresult rv = SetAttributeOrEquivalent(newElement, nsGkAtoms::valign,
                                           u"top"_ns, true);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::SetAttributeOrEquivalent(nsGkAtoms::valign, top) "
          "failed");
      return nullptr;
    }
  }
  // ADD OTHER TAGS HERE

  return newElement.forget();
}

NS_IMETHODIMP HTMLEditor::CreateElementWithDefaults(const nsAString& aTagName,
                                                    Element** aReturn) {
  if (NS_WARN_IF(aTagName.IsEmpty()) || NS_WARN_IF(!aReturn)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aReturn = nullptr;

  nsStaticAtom* tagName = EditorUtils::GetTagNameAtom(aTagName);
  if (NS_WARN_IF(!tagName)) {
    return NS_ERROR_INVALID_ARG;
  }
  RefPtr<Element> newElement =
      CreateElementWithDefaults(MOZ_KnownLive(*tagName));
  if (!newElement) {
    NS_WARNING("HTMLEditor::CreateElementWithDefaults() failed");
    return NS_ERROR_FAILURE;
  }
  newElement.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::InsertLinkAroundSelection(Element* aAnchorElement) {
  nsresult rv = InsertLinkAroundSelectionAsAction(aAnchorElement);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::InsertLinkAroundSelectionAsAction() failed");
  return rv;
}

nsresult HTMLEditor::InsertLinkAroundSelectionAsAction(
    Element* aAnchorElement, nsIPrincipal* aPrincipal) {
  if (NS_WARN_IF(!aAnchorElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertLinkElement,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (SelectionRef().IsCollapsed()) {
    NS_WARNING("Selection was collapsed");
    return NS_OK;
  }

  // Be sure we were given an anchor element
  RefPtr<HTMLAnchorElement> anchor =
      HTMLAnchorElement::FromNodeOrNull(aAnchorElement);
  if (!anchor) {
    return NS_OK;
  }

  nsAutoString rawHref;
  anchor->GetAttr(kNameSpaceID_None, nsGkAtoms::href, rawHref);
  editActionData.SetData(rawHref);

  nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "MaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  nsAutoString href;
  anchor->GetHref(href);
  if (href.IsEmpty()) {
    return NS_OK;
  }

  AutoPlaceholderBatch treatAsOneTransaction(
      *this, ScrollSelectionIntoView::Yes, __FUNCTION__);

  // Set all attributes found on the supplied anchor element
  RefPtr<nsDOMAttributeMap> attributeMap = anchor->Attributes();
  if (NS_WARN_IF(!attributeMap)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t count = attributeMap->Length();
  nsAutoString value;
  for (uint32_t i = 0; i < count; ++i) {
    // XXX nsDOMAttributeMap::Item() accesses current attribute at the index.
    //     Therefore, if `SetInlinePropertyInternal()` changed the attributes,
    //     this may fail to scan some attributes.  Perhaps, we need to cache
    //     all attributes first.
    RefPtr<Attr> attribute = attributeMap->Item(i);
    if (!attribute) {
      continue;
    }

    // We must clear the string buffers
    //   because GetValue appends to previous string!
    value.Truncate();

    nsAtom* attributeName = attribute->NodeInfo()->NameAtom();

    attribute->GetValue(value);

    nsresult rv = SetInlinePropertyInternal(
        *nsGkAtoms::a, MOZ_KnownLive(attributeName), value);
    if (NS_FAILED(rv)) {
      NS_WARNING("SetInlinePropertyInternal(nsGkAtoms::a) failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::SetHTMLBackgroundColorWithTransaction(
    const nsAString& aColor) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Find a selected or enclosing table element to set background on
  bool isCellSelected = false;
  Result<RefPtr<Element>, nsresult> cellOrRowOrTableElementOrError =
      GetSelectedOrParentTableElement(&isCellSelected);
  if (cellOrRowOrTableElementOrError.isErr()) {
    NS_WARNING("HTMLEditor::GetSelectedOrParentTableElement() failed");
    return cellOrRowOrTableElementOrError.unwrapErr();
  }

  bool setColor = !aColor.IsEmpty();
  RefPtr<Element> rootElementOfBackgroundColor =
      cellOrRowOrTableElementOrError.unwrap();
  if (rootElementOfBackgroundColor) {
    // Needs to set or remove background color of each selected cell elements.
    // Therefore, just the cell contains selection range, we don't need to
    // do this.  Note that users can select each cell, but with Selection API,
    // web apps can select <tr> and <td> at same time. With <table>, looks
    // odd, though.
    if (isCellSelected || rootElementOfBackgroundColor->IsAnyOfHTMLElements(
                              nsGkAtoms::table, nsGkAtoms::tr)) {
      SelectedTableCellScanner scanner(SelectionRef());
      if (scanner.IsInTableCellSelectionMode()) {
        if (setColor) {
          for (const OwningNonNull<Element>& cellElement :
               scanner.ElementsRef()) {
            // `MOZ_KnownLive(cellElement)` is safe because of `scanner`
            // is stack only class and keeps grabbing it until it's destroyed.
            nsresult rv = SetAttributeWithTransaction(
                MOZ_KnownLive(cellElement), *nsGkAtoms::bgcolor, aColor);
            if (NS_WARN_IF(Destroyed())) {
              return NS_ERROR_EDITOR_DESTROYED;
            }
            if (NS_FAILED(rv)) {
              NS_WARNING(
                  "EditorBase::::SetAttributeWithTransaction(nsGkAtoms::"
                  "bgcolor) failed");
              return rv;
            }
          }
          return NS_OK;
        }
        for (const OwningNonNull<Element>& cellElement :
             scanner.ElementsRef()) {
          // `MOZ_KnownLive(cellElement)` is safe because of `scanner`
          // is stack only class and keeps grabbing it until it's destroyed.
          nsresult rv = RemoveAttributeWithTransaction(
              MOZ_KnownLive(cellElement), *nsGkAtoms::bgcolor);
          if (NS_WARN_IF(Destroyed())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "EditorBase::RemoveAttributeWithTransaction(nsGkAtoms::bgcolor)"
                " failed");
            return rv;
          }
        }
        return NS_OK;
      }
    }
    // If we failed to find a cell, fall through to use originally-found element
  } else {
    // No table element -- set the background color on the body tag
    rootElementOfBackgroundColor = GetRoot();
    if (NS_WARN_IF(!rootElementOfBackgroundColor)) {
      return NS_ERROR_FAILURE;
    }
  }
  // Use the editor method that goes through the transaction system
  if (setColor) {
    nsresult rv = SetAttributeWithTransaction(*rootElementOfBackgroundColor,
                                              *nsGkAtoms::bgcolor, aColor);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::SetAttributeWithTransaction(nsGkAtoms::bgcolor) failed");
    return rv;
  }
  nsresult rv = RemoveAttributeWithTransaction(*rootElementOfBackgroundColor,
                                               *nsGkAtoms::bgcolor);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::RemoveAttributeWithTransaction(nsGkAtoms::bgcolor) failed");
  return rv;
}

nsresult HTMLEditor::RemoveEmptyInclusiveAncestorInlineElements(
    nsIContent& aContent) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!aContent.Length());

  Element* editingHost = aContent.GetEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return NS_ERROR_FAILURE;
  }

  if (&aContent == editingHost || HTMLEditUtils::IsBlockElement(aContent) ||
      !EditorUtils::IsEditableContent(aContent, EditorType::HTML) ||
      !aContent.GetParent()) {
    return NS_OK;
  }

  // Don't strip wrappers if this is the only wrapper in the block.  Then we'll
  // add a <br> later, so it won't be an empty wrapper in the end.
  // XXX This is different from Blink.  We should delete empty inline element
  //     even if it's only child of the block element.
  {
    const Element* editableBlockElement = HTMLEditUtils::GetAncestorElement(
        aContent, HTMLEditUtils::ClosestEditableBlockElement);
    if (!editableBlockElement ||
        HTMLEditUtils::IsEmptyNode(
            *editableBlockElement,
            {EmptyCheckOption::TreatSingleBRElementAsVisible})) {
      return NS_OK;
    }
  }

  OwningNonNull<nsIContent> content = aContent;
  for (nsIContent* parentContent : aContent.AncestorsOfType<nsIContent>()) {
    if (HTMLEditUtils::IsBlockElement(*parentContent) ||
        parentContent->Length() != 1 ||
        !EditorUtils::IsEditableContent(*parentContent, EditorType::HTML) ||
        parentContent == editingHost) {
      break;
    }
    content = *parentContent;
  }

  nsresult rv = DeleteNodeWithTransaction(content);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DeleteNodeWithTransaction() failed");
  return rv;
}

nsresult HTMLEditor::DeleteAllChildrenWithTransaction(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Prevent rules testing until we're done
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  while (nsCOMPtr<nsIContent> child = aElement.GetLastChild()) {
    nsresult rv = DeleteNodeWithTransaction(*child);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::DeleteNode(nsINode* aNode) {
  if (NS_WARN_IF(!aNode) || NS_WARN_IF(!aNode->IsContent())) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRemoveNode);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = DeleteNodeWithTransaction(MOZ_KnownLive(*aNode->AsContent()));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DeleteNodeWithTransaction() failed");
  return rv;
}

nsresult HTMLEditor::DeleteTextWithTransaction(Text& aTextNode,
                                               uint32_t aOffset,
                                               uint32_t aLength) {
  if (NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(aTextNode))) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv =
      EditorBase::DeleteTextWithTransaction(aTextNode, aOffset, aLength);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DeleteTextWithTransaction() failed");
  return rv;
}

nsresult HTMLEditor::ReplaceTextWithTransaction(
    Text& aTextNode, uint32_t aOffset, uint32_t aLength,
    const nsAString& aStringToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aLength > 0 || !aStringToInsert.IsEmpty());

  if (aStringToInsert.IsEmpty()) {
    nsresult rv = DeleteTextWithTransaction(aTextNode, aOffset, aLength);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::DeleteTextWithTransaction() failed");
    return rv;
  }

  if (!aLength) {
    RefPtr<Document> document = GetDocument();
    if (NS_WARN_IF(!document)) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    Result<EditorDOMPoint, nsresult> insertTextResult =
        InsertTextWithTransaction(*document, aStringToInsert,
                                  EditorDOMPoint(&aTextNode, aOffset));
    if (MOZ_UNLIKELY(insertTextResult.isErr())) {
      NS_WARNING("HTMLEditor::InsertTextWithTransaction() failed");
      return insertTextResult.unwrapErr();
    }
    return NS_OK;
  }

  if (NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(aTextNode))) {
    return NS_ERROR_FAILURE;
  }

  // This should emulates inserting text for better undo/redo behavior.
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditorBase::ToGenericNSResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // FYI: Create the insertion point before changing the DOM tree because
  //      the point may become invalid offset after that.
  EditorDOMPointInText pointToInsert(&aTextNode, aOffset);

  // `ReplaceTextTransaction()` removes the replaced text first, then,
  // insert new text.  Therefore, if selection is in the text node, the
  // range is moved to start of the range and deletion and never adjusted
  // for the inserting text since the change occurs after the range.
  // Therefore, we might need to save/restore selection here.
  Maybe<AutoSelectionRestorer> restoreSelection;
  if (!AllowsTransactionsToChangeSelection() && !ArePreservingSelection()) {
    const uint32_t rangeCount = SelectionRef().RangeCount();
    for (const uint32_t i : IntegerRange(rangeCount)) {
      MOZ_ASSERT(SelectionRef().RangeCount() == rangeCount);
      const nsRange* range = SelectionRef().GetRangeAt(i);
      if (MOZ_UNLIKELY(!range)) {
        continue;
      }
      if ((range->GetStartContainer() == &aTextNode &&
           range->StartOffset() >= aOffset) ||
          (range->GetEndContainer() == &aTextNode &&
           range->EndOffset() >= aOffset)) {
        restoreSelection.emplace(*this);
        break;
      }
    }
  }

  RefPtr<ReplaceTextTransaction> transaction = ReplaceTextTransaction::Create(
      *this, aStringToInsert, aTextNode, aOffset, aLength);
  MOZ_ASSERT(transaction);

  if (aLength && !mActionListeners.IsEmpty()) {
    for (auto& listener : mActionListeners.Clone()) {
      DebugOnly<nsresult> rvIgnored =
          listener->WillDeleteText(&aTextNode, aOffset, aLength);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::WillDeleteText() failed, but ignored");
    }
  }

  nsresult rv = DoTransactionInternal(transaction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DoTransactionInternal() failed");

  if (pointToInsert.IsSet()) {
    auto [begin, end] = ComputeInsertedRange(pointToInsert, aStringToInsert);
    if (begin.IsSet() && end.IsSet()) {
      TopLevelEditSubActionDataRef().DidDeleteText(
          *this, begin.To<EditorRawDOMPoint>());
      TopLevelEditSubActionDataRef().DidInsertText(
          *this, begin.To<EditorRawDOMPoint>(), end.To<EditorRawDOMPoint>());
    }
  }

  // Now, restores selection for allowing the following listeners to modify
  // selection.
  restoreSelection.reset();

  if (!mActionListeners.IsEmpty()) {
    for (auto& listener : mActionListeners.Clone()) {
      DebugOnly<nsresult> rvIgnored =
          listener->DidInsertText(&aTextNode, aOffset, aStringToInsert, rv);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::DidInsertText() failed, but ignored");
    }
  }

  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : rv;
}

Result<EditorDOMPoint, nsresult> HTMLEditor::InsertTextWithTransaction(
    Document& aDocument, const nsAString& aStringToInsert,
    const EditorDOMPoint& aPointToInsert) {
  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return Err(NS_ERROR_INVALID_ARG);
  }

  // Do nothing if the node is read-only
  if (MOZ_UNLIKELY(NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(
          *aPointToInsert.GetContainer())))) {
    return Err(NS_ERROR_FAILURE);
  }

  return EditorBase::InsertTextWithTransaction(aDocument, aStringToInsert,
                                               aPointToInsert);
}

Result<EditorDOMPoint, nsresult> HTMLEditor::PrepareToInsertBRElement(
    const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return Err(NS_ERROR_FAILURE);
  }

  if (!aPointToInsert.IsInTextNode()) {
    return aPointToInsert;
  }

  if (aPointToInsert.IsStartOfContainer()) {
    // Insert before the text node.
    EditorDOMPoint pointInContainer(aPointToInsert.GetContainer());
    if (!pointInContainer.IsSet()) {
      NS_WARNING("Failed to climb up the DOM tree from text node");
      return Err(NS_ERROR_FAILURE);
    }
    return pointInContainer;
  }

  if (aPointToInsert.IsEndOfContainer()) {
    // Insert after the text node.
    EditorDOMPoint pointInContainer(aPointToInsert.GetContainer());
    if (NS_WARN_IF(!pointInContainer.IsSet())) {
      NS_WARNING("Failed to climb up the DOM tree from text node");
      return Err(NS_ERROR_FAILURE);
    }
    MOZ_ALWAYS_TRUE(pointInContainer.AdvanceOffset());
    return pointInContainer;
  }

  MOZ_DIAGNOSTIC_ASSERT(aPointToInsert.IsSetAndValid());

  // Unfortunately, we need to split the text node at the offset.
  const SplitNodeResult splitTextNodeResult =
      SplitNodeWithTransaction(aPointToInsert);
  if (splitTextNodeResult.isErr()) {
    NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
    return Err(splitTextNodeResult.unwrapErr());
  }
  nsresult rv = splitTextNodeResult.SuggestCaretPointTo(
      *this, {SuggestCaret::OnlyIfTransactionsAllowedToDoIt});
  if (NS_FAILED(rv)) {
    NS_WARNING("SplitNodeResult::SuggestCaretPointTo() failed");
    return Err(rv);
  }

  // Insert new <br> before the right node.
  auto atNextContent = splitTextNodeResult.AtNextContent<EditorDOMPoint>();
  if (MOZ_UNLIKELY(!atNextContent.IsSet())) {
    NS_WARNING("The next node seems not in the DOM tree");
    return Err(NS_ERROR_FAILURE);
  }
  return atNextContent;
}

CreateElementResult HTMLEditor::InsertBRElement(
    WithTransaction aWithTransaction, const EditorDOMPoint& aPointToInsert,
    EDirection aSelect /* = eNone */) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  Result<EditorDOMPoint, nsresult> maybePointToInsert =
      PrepareToInsertBRElement(aPointToInsert);
  if (maybePointToInsert.isErr()) {
    NS_WARNING(
        nsPrintfCString("HTMLEditor::PrepareToInsertBRElement(%s) failed",
                        ToString(aWithTransaction).c_str())
            .get());
    return CreateElementResult(maybePointToInsert.unwrapErr());
  }
  MOZ_ASSERT(maybePointToInsert.inspect().IsSetAndValid());

  CreateElementResult createNewBRElementResult = CreateAndInsertElement(
      aWithTransaction, *nsGkAtoms::br, maybePointToInsert.inspect());
  if (createNewBRElementResult.isErr()) {
    NS_WARNING(nsPrintfCString("HTMLEditor::CreateAndInsertElement(%s) failed",
                               ToString(aWithTransaction).c_str())
                   .get());
    return CreateElementResult(createNewBRElementResult.unwrapErr());
  }
  RefPtr<Element> newBRElement = createNewBRElementResult.UnwrapNewNode();
  MOZ_ASSERT(newBRElement);

  createNewBRElementResult.IgnoreCaretPointSuggestion();
  switch (aSelect) {
    case eNext: {
      const auto pointToPutCaret = EditorDOMPoint::After(
          *newBRElement, Selection::InterlinePosition::StartOfNextLine);
      return CreateElementResult(std::move(newBRElement), pointToPutCaret);
    }
    case ePrevious: {
      const auto pointToPutCaret = EditorDOMPoint(
          newBRElement, Selection::InterlinePosition::StartOfNextLine);
      return CreateElementResult(std::move(newBRElement), pointToPutCaret);
    }
    default:
      NS_WARNING(
          "aSelect has invalid value, the caller need to set selection "
          "by itself");
      [[fallthrough]];
    case eNone:
      return CreateElementResult(std::move(newBRElement),
                                 createNewBRElementResult.UnwrapCaretPoint());
  }
}

already_AddRefed<Element> HTMLEditor::InsertContainerWithTransactionInternal(
    nsIContent& aContent, nsAtom& aTagName, nsAtom& aAttribute,
    const nsAString& aAttributeValue) {
  EditorDOMPoint pointToInsertNewContainer(&aContent);
  if (NS_WARN_IF(!pointToInsertNewContainer.IsSet())) {
    return nullptr;
  }
  // aContent will be moved to the new container before inserting the new
  // container.  So, when we insert the container, the insertion point
  // is before the next sibling of aContent.
  // XXX If pointerToInsertNewContainer stores offset here, the offset and
  //     referring child node become mismatched.  Although, currently this
  //     is not a problem since InsertNodeTransaction refers only child node.
  DebugOnly<bool> advanced = pointToInsertNewContainer.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced, "Failed to advance offset to after aContent");

  // Create new container.
  RefPtr<Element> newContainer = CreateHTMLContent(&aTagName);
  if (NS_WARN_IF(!newContainer)) {
    return nullptr;
  }

  // Set attribute if needed.
  if (&aAttribute != nsGkAtoms::_empty) {
    nsresult rv = newContainer->SetAttr(kNameSpaceID_None, &aAttribute,
                                        aAttributeValue, true);
    if (NS_FAILED(rv)) {
      NS_WARNING("Element::SetAttr() failed");
      return nullptr;
    }
  }

  // Notify our internal selection state listener
  AutoInsertContainerSelNotify selNotify(RangeUpdaterRef());

  // Put aNode in the new container, first.
  // XXX Perhaps, we should not remove the container if it's not editable.
  nsresult rv = DeleteNodeWithTransaction(aContent);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return nullptr;
  }

  {
    // TODO: Remove AutoTransactionsConserveSelection here.  It's not necessary
    //       in normal cases.  However, it may be required for nested edit
    //       actions which may be caused by legacy mutation event listeners or
    //       chrome script.
    AutoTransactionsConserveSelection conserveSelection(*this);
    CreateContentResult insertContentNodeResult =
        InsertNodeWithTransaction(aContent, EditorDOMPoint(newContainer, 0u));
    if (insertContentNodeResult.isErr()) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return nullptr;
    }
    insertContentNodeResult.IgnoreCaretPointSuggestion();
  }

  // Put the new container where aNode was.
  CreateElementResult insertNewContainerElementResult =
      InsertNodeWithTransaction<Element>(*newContainer,
                                         pointToInsertNewContainer);
  if (insertNewContainerElementResult.isErr()) {
    NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
    return nullptr;
  }
  rv = insertNewContainerElementResult.SuggestCaretPointTo(
      *this, {SuggestCaret::OnlyIfHasSuggestion,
              SuggestCaret::OnlyIfTransactionsAllowedToDoIt,
              SuggestCaret::AndIgnoreTrivialError});
  if (NS_FAILED(rv)) {
    NS_WARNING("CreateElementResult::SuggestCaretPointTo() failed");
    return nullptr;
  }
  NS_WARNING_ASSERTION(
      rv != NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR,
      "CreateElementResult::SuggestCaretPointTo() failed, but ignored");
  return newContainer.forget();
}

CreateElementResult HTMLEditor::ReplaceContainerWithTransactionInternal(
    Element& aOldContainer, nsAtom& aTagName, nsAtom& aAttribute,
    const nsAString& aAttributeValue, bool aCloneAllAttributes) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!HTMLEditUtils::IsRemovableNode(aOldContainer)) ||
      NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(aOldContainer))) {
    return CreateElementResult(NS_ERROR_FAILURE);
  }

  const RefPtr<Element> newContainer = CreateHTMLContent(&aTagName);
  if (NS_WARN_IF(!newContainer)) {
    return CreateElementResult(NS_ERROR_FAILURE);
  }

  // Set or clone attribute if needed.
  if (aCloneAllAttributes) {
    MOZ_ASSERT(&aAttribute == nsGkAtoms::_empty);
    CloneAttributesWithTransaction(*newContainer, aOldContainer);
  } else if (&aAttribute != nsGkAtoms::_empty) {
    nsresult rv = newContainer->SetAttr(kNameSpaceID_None, &aAttribute,
                                        aAttributeValue, true);
    if (NS_FAILED(rv)) {
      NS_WARNING("Element::SetAttr() failed");
      return CreateElementResult(NS_ERROR_FAILURE);
    }
  }

  const OwningNonNull<nsINode> parentNode = *aOldContainer.GetParentNode();
  const nsCOMPtr<nsINode> referenceNode = aOldContainer.GetNextSibling();
  AutoReplaceContainerSelNotify selStateNotify(RangeUpdaterRef(), aOldContainer,
                                               *newContainer);
  {
    AutoTArray<OwningNonNull<nsIContent>, 32> arrayOfChildren;
    HTMLEditUtils::CollectChildren(
        aOldContainer, arrayOfChildren, 0u,
        // Move non-editable children too because its container, aElement, is
        // editable so that all children must be removable node.
        {});
    // TODO: Remove AutoTransactionsConserveSelection here.  It's not necessary
    //       in normal cases.  However, it may be required for nested edit
    //       actions which may be caused by legacy mutation event listeners or
    //       chrome script.
    AutoTransactionsConserveSelection conserveSelection(*this);
    // Move all children from the old container to the new container.
    // For making all MoveNodeTransactions have a referenc node in the current
    // parent, move nodes from last one to preceding ones.
    for (const OwningNonNull<nsIContent>& child : Reversed(arrayOfChildren)) {
      MoveNodeResult moveChildResult =
          MoveNodeWithTransaction(MOZ_KnownLive(child),  // due to bug 1622253.
                                  EditorDOMPoint(newContainer, 0u));
      if (moveChildResult.isErr()) {
        NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
        return CreateElementResult(moveChildResult.unwrapErr());
      }
      // We'll suggest new caret point which is suggested by new container
      // element insertion result.  Therefore, we need to do nothing here.
      moveChildResult.IgnoreCaretPointSuggestion();
    }
  }

  // Delete aOldContainer from the DOM tree to make it not referred by
  // InsertNodeTransaction.
  nsresult rv = DeleteNodeWithTransaction(aOldContainer);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return CreateElementResult(rv);
  }

  if (referenceNode && (!referenceNode->GetParentNode() ||
                        parentNode != referenceNode->GetParentNode())) {
    NS_WARNING(
        "The reference node for insertion has been moved to different parent, "
        "so we got lost the insertion point");
    return CreateElementResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  // Finally, insert the new node to where probably aOldContainer was.
  CreateElementResult insertNewContainerElementResult =
      InsertNodeWithTransaction<Element>(
          *newContainer, referenceNode ? EditorDOMPoint(referenceNode)
                                       : EditorDOMPoint::AtEndOf(*parentNode));
  NS_WARNING_ASSERTION(insertNewContainerElementResult.isOk(),
                       "EditorBase::InsertNodeWithTransaction() failed");
  return insertNewContainerElementResult;
}

Result<EditorDOMPoint, nsresult> HTMLEditor::RemoveContainerWithTransaction(
    Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!HTMLEditUtils::IsRemovableNode(aElement)) ||
      NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(aElement))) {
    return Err(NS_ERROR_FAILURE);
  }

  // Notify our internal selection state listener.
  AutoRemoveContainerSelNotify selNotify(RangeUpdaterRef(),
                                         EditorRawDOMPoint(&aElement));

  AutoTArray<OwningNonNull<nsIContent>, 32> arrayOfChildren;
  HTMLEditUtils::CollectChildren(
      aElement, arrayOfChildren, 0u,
      // Move non-editable children too because its container, aElement, is
      // editable so that all children must be removable node.
      {});
  const OwningNonNull<nsINode> parentNode = *aElement.GetParentNode();
  nsCOMPtr<nsIContent> previousChild = aElement.GetPreviousSibling();
  // For making all MoveNodeTransactions have a referenc node in the current
  // parent, move nodes from last one to preceding ones.
  for (const OwningNonNull<nsIContent>& child : Reversed(arrayOfChildren)) {
    MoveNodeResult moveChildResult = MoveNodeWithTransaction(
        MOZ_KnownLive(child),  // due to bug 1622253.
        previousChild ? EditorDOMPoint::After(previousChild)
                      : EditorDOMPoint(parentNode, 0u));
    if (moveChildResult.isErr()) {
      NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
      return Err(moveChildResult.unwrapErr());
    }
    // If the reference node was moved to different container, try to recover
    // the original position.
    if (previousChild &&
        MOZ_UNLIKELY(previousChild->GetParentNode() != parentNode)) {
      if (MOZ_UNLIKELY(child->GetParentNode() != parentNode)) {
        NS_WARNING(
            "Neither the reference (previous) sibling nor the moved child was "
            "in the expected parent node");
        moveChildResult.IgnoreCaretPointSuggestion();
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
      previousChild = child->GetPreviousSibling();
    }
    // We'll need to put caret at next sibling of aElement if nobody moves
    // content nodes under the parent node except us.
    moveChildResult.IgnoreCaretPointSuggestion();
  }

  if (aElement.GetParentNode() && aElement.GetParentNode() != parentNode) {
    NS_WARNING(
        "The removing element has already been moved to another element");
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  auto GetNextSiblingOf =
      [](const nsTArray<OwningNonNull<nsIContent>>& aArrayOfMovedContent,
         const nsINode& aExpectedParentNode) -> nsIContent* {
    for (const OwningNonNull<nsIContent>& movedChild :
         Reversed(aArrayOfMovedContent)) {
      if (movedChild != &aExpectedParentNode) {
        continue;  // Ignore moved node which was moved to different place
      }
      return movedChild->GetNextSibling();
    }
    // XXX If all nodes were moved by web apps, we cannot suggest "collect"
    //     position without computing the index of aElement.  However, I
    //     don't think that it's necessary for the web apps in the wild.
    return nullptr;
  };

  nsCOMPtr<nsIContent> nextSibling =
      aElement.GetParentNode() ? aElement.GetNextSibling()
                               : GetNextSiblingOf(arrayOfChildren, *parentNode);

  nsresult rv = DeleteNodeWithTransaction(aElement);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteNodeTransaction() failed");
    return Err(rv);
  }

  if (nextSibling && nextSibling->GetParentNode() != parentNode) {
    nextSibling = GetNextSiblingOf(arrayOfChildren, *parentNode);
  }
  return nextSibling ? EditorDOMPoint(nextSibling)
                     : EditorDOMPoint::AtEndOf(*parentNode);
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void HTMLEditor::ContentAppended(
    nsIContent* aFirstNewContent) {
  DoContentInserted(aFirstNewContent, eAppended);
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void HTMLEditor::ContentInserted(
    nsIContent* aChild) {
  DoContentInserted(aChild, eInserted);
}

bool HTMLEditor::IsInObservedSubtree(nsIContent* aChild) {
  if (!aChild) {
    return false;
  }

  // FIXME(emilio, bug 1596856): This should probably work if the root is in the
  // same shadow tree as the child, probably? I don't know what the
  // contenteditable-in-shadow-dom situation is.
  if (Element* root = GetRoot()) {
    // To be super safe here, check both ChromeOnlyAccess and NAC / Shadow DOM.
    // That catches (also unbound) native anonymous content and ShadowDOM.
    if (root->ChromeOnlyAccess() != aChild->ChromeOnlyAccess() ||
        root->IsInNativeAnonymousSubtree() !=
            aChild->IsInNativeAnonymousSubtree() ||
        root->IsInShadowTree() != aChild->IsInShadowTree()) {
      return false;
    }
  }

  return !aChild->ChromeOnlyAccess() && !aChild->IsInShadowTree() &&
         !aChild->IsInNativeAnonymousSubtree();
}

void HTMLEditor::DoContentInserted(nsIContent* aChild,
                                   InsertedOrAppended aInsertedOrAppended) {
  MOZ_ASSERT(aChild);
  nsINode* container = aChild->GetParentNode();
  MOZ_ASSERT(container);

  if (!IsInObservedSubtree(aChild)) {
    return;
  }

  // XXX Why do we need this? This method is a helper of mutation observer.
  //     So, the callers of mutation observer should guarantee that this won't
  //     be deleted at least during the call.
  RefPtr<HTMLEditor> kungFuDeathGrip(this);

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  if (ShouldReplaceRootElement()) {
    UpdateRootElement();
    if (mPendingRootElementUpdatedRunner) {
      return;
    }
    mPendingRootElementUpdatedRunner = NewRunnableMethod(
        "HTMLEditor::NotifyRootChanged", this, &HTMLEditor::NotifyRootChanged);
    nsContentUtils::AddScriptRunner(
        do_AddRef(mPendingRootElementUpdatedRunner));
    return;
  }

  // We don't need to handle our own modifications
  if (!GetTopLevelEditSubAction() && container->IsEditable()) {
    if (EditorUtils::IsPaddingBRElementForEmptyEditor(*aChild)) {
      // Ignore insertion of the padding <br> element.
      return;
    }
    nsresult rv = OnDocumentModified();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::OnDocumentModified() failed, but ignored");

    // Update spellcheck for only the newly-inserted node (bug 743819)
    if (mInlineSpellChecker) {
      RefPtr<nsRange> range = nsRange::Create(aChild);
      nsIContent* endContent = aChild;
      if (aInsertedOrAppended == eAppended) {
        // Maybe more than 1 child was appended.
        endContent = container->GetLastChild();
      }
      range->SelectNodesInContainer(container, aChild, endContent);
      DebugOnly<nsresult> rvIgnored =
          mInlineSpellChecker->SpellCheckRange(range);
      NS_WARNING_ASSERTION(
          rvIgnored == NS_ERROR_NOT_INITIALIZED || NS_SUCCEEDED(rvIgnored),
          "mozInlineSpellChecker::SpellCheckRange() failed, but ignored");
    }
  }
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void HTMLEditor::ContentRemoved(
    nsIContent* aChild, nsIContent* aPreviousSibling) {
  if (!IsInObservedSubtree(aChild)) {
    return;
  }

  // XXX Why do we need to do this?  This method is a mutation observer's
  //     method.  Therefore, the caller should guarantee that this won't be
  //     deleted during the call.
  RefPtr<HTMLEditor> kungFuDeathGrip(this);

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  if (SameCOMIdentity(aChild, mRootElement)) {
    mRootElement = nullptr;
    if (mPendingRootElementUpdatedRunner) {
      return;
    }
    mPendingRootElementUpdatedRunner = NewRunnableMethod(
        "HTMLEditor::NotifyRootChanged", this, &HTMLEditor::NotifyRootChanged);
    nsContentUtils::AddScriptRunner(
        do_AddRef(mPendingRootElementUpdatedRunner));
    return;
  }

  // We don't need to handle our own modifications
  if (!GetTopLevelEditSubAction() && aChild->GetParentNode()->IsEditable()) {
    if (aChild && EditorUtils::IsPaddingBRElementForEmptyEditor(*aChild)) {
      // Ignore removal of the padding <br> element for empty editor.
      return;
    }

    nsresult rv = OnDocumentModified();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::OnDocumentModified() failed, but ignored");
  }
}

nsresult HTMLEditor::SelectEntireDocument() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mInitSucceeded) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // XXX It's odd to select all of the document body if an contenteditable
  //     element has focus.
  RefPtr<Element> bodyOrDocumentElement = GetRoot();
  if (NS_WARN_IF(!bodyOrDocumentElement)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // If we're empty, don't select all children because that would select the
  // padding <br> element for empty editor.
  if (IsEmpty()) {
    nsresult rv = CollapseSelectionToStartOf(*bodyOrDocumentElement);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::CollapseSelectionToStartOf() failed");
    return rv;
  }

  // Otherwise, select all children.
  ErrorResult error;
  SelectionRef().SelectAllChildren(*bodyOrDocumentElement, error);
  if (NS_WARN_IF(Destroyed())) {
    error.SuppressException();
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(!error.Failed(),
                       "Selection::SelectAllChildren() failed");
  return error.StealNSResult();
}

nsresult HTMLEditor::SelectAllInternal() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  CommitComposition();
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  // XXX Perhaps, we should check whether we still have focus since composition
  //     event listener may have already moved focus to different editing
  //     host or other element.  So, perhaps, we need to retrieve anchor node
  //     before committing composition and check if selection is still in
  //     same editing host.

  nsINode* anchorNode = SelectionRef().GetAnchorNode();
  if (NS_WARN_IF(!anchorNode) || NS_WARN_IF(!anchorNode->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> anchorContent = anchorNode->AsContent();
  nsCOMPtr<nsIContent> rootContent;
  if (anchorContent->HasIndependentSelection()) {
    SelectionRef().SetAncestorLimiter(nullptr);
    rootContent = mRootElement;
    if (NS_WARN_IF(!rootContent)) {
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    RefPtr<PresShell> presShell = GetPresShell();
    rootContent = anchorContent->GetSelectionRootContent(presShell);
    if (NS_WARN_IF(!rootContent)) {
      return NS_ERROR_UNEXPECTED;
    }
    // If the document is HTML document (not XHTML document), we should
    // select all children of the `<body>` element instead of `<html>`
    // element.
    if (Document* document = GetDocument()) {
      if (document->IsHTMLDocument()) {
        if (HTMLBodyElement* bodyElement = document->GetBodyElement()) {
          if (nsContentUtils::ContentIsFlattenedTreeDescendantOf(bodyElement,
                                                                 rootContent)) {
            rootContent = bodyElement;
          }
        }
      }
    }
  }

  Maybe<Selection::AutoUserInitiated> userSelection;
  if (!rootContent->IsEditable()) {
    userSelection.emplace(SelectionRef());
  }
  ErrorResult error;
  SelectionRef().SelectAllChildren(*rootContent, error);
  NS_WARNING_ASSERTION(!error.Failed(),
                       "Selection::SelectAllChildren() failed");
  return error.StealNSResult();
}

bool HTMLEditor::SetCaretInTableCell(Element* aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!aElement || !aElement->IsHTMLElement() ||
      !HTMLEditUtils::IsAnyTableElement(aElement)) {
    return false;
  }
  const RefPtr<Element> editingHost = ComputeEditingHost();
  if (!editingHost || !aElement->IsInclusiveDescendantOf(editingHost)) {
    return false;
  }

  nsCOMPtr<nsIContent> deepestFirstChild = aElement;
  while (deepestFirstChild->HasChildren()) {
    deepestFirstChild = deepestFirstChild->GetFirstChild();
  }

  // Set selection at beginning of the found node
  nsresult rv = CollapseSelectionToStartOf(*deepestFirstChild);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CollapseSelectionToStartOf() failed");
  return NS_SUCCEEDED(rv);
}

/**
 * This method scans the selection for adjacent text nodes
 * and collapses them into a single text node.
 * "adjacent" means literally adjacent siblings of the same parent.
 * Uses HTMLEditor::JoinNodesWithTransaction() so action is undoable.
 * Should be called within the context of a batch transaction.
 */
nsresult HTMLEditor::CollapseAdjacentTextNodes(nsRange& aInRange) {
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  // we can't actually do anything during iteration, so store the text nodes in
  // an array first.
  DOMSubtreeIterator subtreeIter;
  if (NS_FAILED(subtreeIter.Init(aInRange))) {
    NS_WARNING("DOMSubtreeIterator::Init() failed");
    return NS_ERROR_FAILURE;
  }
  AutoTArray<OwningNonNull<Text>, 8> textNodes;
  subtreeIter.AppendNodesToArray(
      +[](nsINode& aNode, void*) -> bool {
        return EditorUtils::IsEditableContent(*aNode.AsText(),
                                              EditorType::HTML);
      },
      textNodes);

  // now that I have a list of text nodes, collapse adjacent text nodes
  while (textNodes.Length() > 1u) {
    OwningNonNull<Text>& leftTextNode = textNodes[0u];
    OwningNonNull<Text>& rightTextNode = textNodes[1u];

    // If the text nodes are not direct siblings, we shouldn't join them, and
    // we don't need to handle the left one anymore.
    if (rightTextNode->GetPreviousSibling() != leftTextNode) {
      textNodes.RemoveElementAt(0u);
      continue;
    }

    JoinNodesResult result = JoinNodesWithTransaction(
        MOZ_KnownLive(*leftTextNode), MOZ_KnownLive(*rightTextNode));
    if (MOZ_UNLIKELY(result.Failed())) {
      NS_WARNING("HTMLEditor::JoinNodesWithTransaction() failed");
      return result.Rv();
    }
    if (MOZ_LIKELY(result.RemovedContent() == leftTextNode)) {
      textNodes.RemoveElementAt(0u);
    } else if (MOZ_LIKELY(result.RemovedContent() == rightTextNode)) {
      textNodes.RemoveElementAt(1u);
    } else {
      MOZ_ASSERT_UNREACHABLE(
          "HTMLEditor::JoinNodesWithTransaction() removed unexpected node");
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::SetSelectionAtDocumentStart() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = CollapseSelectionToStartOf(*rootElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CollapseSelectionToStartOf() failed");
  return rv;
}

/**
 * Remove aNode, reparenting any children into the parent of aNode.  In
 * addition, insert any br's needed to preserve identity of removed block.
 */
Result<EditorDOMPoint, nsresult>
HTMLEditor::RemoveBlockContainerWithTransaction(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Two possibilities: the container could be empty of editable content.  If
  // that is the case, we need to compare what is before and after aNode to
  // determine if we need a br.
  //
  // Or it could be not empty, in which case we have to compare previous
  // sibling and first child to determine if we need a leading br, and compare
  // following sibling and last child to determine if we need a trailing br.

  EditorDOMPoint pointToPutCaret;
  if (nsCOMPtr<nsIContent> child = HTMLEditUtils::GetFirstChild(
          aElement, {WalkTreeOption::IgnoreNonEditableNode})) {
    // The case of aNode not being empty.  We need a br at start unless:
    // 1) previous sibling of aNode is a block, OR
    // 2) previous sibling of aNode is a br, OR
    // 3) first child of aNode is a block OR
    // 4) either is null

    if (nsIContent* previousSibling = HTMLEditUtils::GetPreviousSibling(
            aElement, {WalkTreeOption::IgnoreNonEditableNode})) {
      if (!HTMLEditUtils::IsBlockElement(*previousSibling) &&
          !previousSibling->IsHTMLElement(nsGkAtoms::br) &&
          !HTMLEditUtils::IsBlockElement(*child)) {
        // Insert br node
        CreateElementResult insertBRElementResult = InsertBRElement(
            WithTransaction::Yes, EditorDOMPoint(&aElement, 0u));
        if (insertBRElementResult.isErr()) {
          NS_WARNING(
              "HTMLEditor::InsertBRElement(WithTransaction::Yes) failed");
          return Err(insertBRElementResult.unwrapErr());
        }
        insertBRElementResult.MoveCaretPointTo(
            pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
        MOZ_ASSERT(insertBRElementResult.GetNewNode());
      }
    }

    // We need a br at end unless:
    // 1) following sibling of aNode is a block, OR
    // 2) last child of aNode is a block, OR
    // 3) last child of aNode is a br OR
    // 4) either is null

    if (nsIContent* nextSibling = HTMLEditUtils::GetNextSibling(
            aElement, {WalkTreeOption::IgnoreNonEditableNode})) {
      if (nextSibling && !HTMLEditUtils::IsBlockElement(*nextSibling)) {
        if (nsIContent* lastChild = HTMLEditUtils::GetLastChild(
                aElement, {WalkTreeOption::IgnoreNonEditableNode})) {
          if (!HTMLEditUtils::IsBlockElement(*lastChild) &&
              !lastChild->IsHTMLElement(nsGkAtoms::br)) {
            CreateElementResult insertBRElementResult = InsertBRElement(
                WithTransaction::Yes, EditorDOMPoint::AtEndOf(aElement));
            if (insertBRElementResult.isErr()) {
              NS_WARNING(
                  "HTMLEditor::InsertBRElement(WithTransaction::Yes) failed");
              return Err(insertBRElementResult.unwrapErr());
            }
            insertBRElementResult.MoveCaretPointTo(
                pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
            MOZ_ASSERT(insertBRElementResult.GetNewNode());
          }
        }
      }
    }
  } else if (nsIContent* previousSibling = HTMLEditUtils::GetPreviousSibling(
                 aElement, {WalkTreeOption::IgnoreNonEditableNode})) {
    // The case of aNode being empty.  We need a br at start unless:
    // 1) previous sibling of aNode is a block, OR
    // 2) previous sibling of aNode is a br, OR
    // 3) following sibling of aNode is a block, OR
    // 4) following sibling of aNode is a br OR
    // 5) either is null
    if (!HTMLEditUtils::IsBlockElement(*previousSibling) &&
        !previousSibling->IsHTMLElement(nsGkAtoms::br)) {
      if (nsIContent* nextSibling = HTMLEditUtils::GetNextSibling(
              aElement, {WalkTreeOption::IgnoreNonEditableNode})) {
        if (!HTMLEditUtils::IsBlockElement(*nextSibling) &&
            !nextSibling->IsHTMLElement(nsGkAtoms::br)) {
          CreateElementResult insertBRElementResult = InsertBRElement(
              WithTransaction::Yes, EditorDOMPoint(&aElement, 0u));
          if (insertBRElementResult.isErr()) {
            NS_WARNING(
                "HTMLEditor::InsertBRElement(WithTransaction::Yes) failed");
            return Err(insertBRElementResult.unwrapErr());
          }
          insertBRElementResult.MoveCaretPointTo(
              pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
          MOZ_ASSERT(insertBRElementResult.GetNewNode());
        }
      }
    }
  }

  // Now remove container
  Result<EditorDOMPoint, nsresult> unwrapBlockElementResult =
      RemoveContainerWithTransaction(aElement);
  if (MOZ_UNLIKELY(unwrapBlockElementResult.isErr())) {
    NS_WARNING("HTMLEditor::RemoveContainerWithTransaction() failed");
    return unwrapBlockElementResult;
  }
  if (AllowsTransactionsToChangeSelection() &&
      unwrapBlockElementResult.inspect().IsSet()) {
    pointToPutCaret = unwrapBlockElementResult.unwrap();
  }
  return pointToPutCaret;  // May be unset
}

SplitNodeResult HTMLEditor::SplitNodeWithTransaction(
    const EditorDOMPoint& aStartOfRightNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (MOZ_UNLIKELY(NS_WARN_IF(!aStartOfRightNode.IsInContentNode()))) {
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }
  MOZ_ASSERT(aStartOfRightNode.IsSetAndValid());

  if (MOZ_UNLIKELY(NS_WARN_IF(!HTMLEditUtils::IsSplittableNode(
          *aStartOfRightNode.ContainerAsContent())))) {
    return SplitNodeResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eSplitNode, nsIEditor::eNext, ignoredError);
  if (MOZ_UNLIKELY(
          NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED)))) {
    return SplitNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  RefPtr<SplitNodeTransaction> transaction =
      SplitNodeTransaction::Create(*this, aStartOfRightNode);
  nsresult rv = DoTransactionInternal(transaction);
  if (NS_WARN_IF(Destroyed())) {
    NS_WARNING(
        "EditorBase::DoTransactionInternal() caused destroying the editor");
    return SplitNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DoTransactionInternal() failed");
    return SplitNodeResult(rv);
  }

  nsCOMPtr<nsIContent> newContent = transaction->GetNewContent();
  nsCOMPtr<nsIContent> splitContent = transaction->GetSplitContent();
  if (NS_WARN_IF(!newContent) || NS_WARN_IF(!splitContent)) {
    return SplitNodeResult(NS_ERROR_FAILURE);
  }
  TopLevelEditSubActionDataRef().DidSplitContent(
      *this, *splitContent, *newContent, SplitNodeDirection::LeftNodeIsNewOne);

  return SplitNodeResult(std::move(newContent), std::move(splitContent),
                         SplitNodeDirection::LeftNodeIsNewOne);
}

SplitNodeResult HTMLEditor::SplitNodeDeepWithTransaction(
    nsIContent& aMostAncestorToSplit,
    const EditorDOMPoint& aStartOfDeepestRightNode,
    SplitAtEdges aSplitAtEdges) {
  MOZ_ASSERT(aStartOfDeepestRightNode.IsSetAndValid());
  MOZ_ASSERT(
      aStartOfDeepestRightNode.GetContainer() == &aMostAncestorToSplit ||
      EditorUtils::IsDescendantOf(*aStartOfDeepestRightNode.GetContainer(),
                                  aMostAncestorToSplit));

  if (NS_WARN_IF(!aStartOfDeepestRightNode.IsSet())) {
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }

  nsCOMPtr<nsIContent> newLeftNodeOfMostAncestor;
  EditorDOMPoint atStartOfRightNode(aStartOfDeepestRightNode);
  // lastResult is as explained by its name, the last result which may not be
  // split a node actually.
  SplitNodeResult lastResult = SplitNodeResult::NotHandled(
      atStartOfRightNode, SplitNodeDirection::LeftNodeIsNewOne);

  while (true) {
    // Need to insert rules code call here to do things like not split a list
    // if you are after the last <li> or before the first, etc.  For now we
    // just have some smarts about unnecessarily splitting text nodes, which
    // should be universal enough to put straight in this EditorBase routine.
    nsIContent* splittingContent = atStartOfRightNode.GetContainerAsContent();
    if (NS_WARN_IF(!splittingContent)) {
      lastResult.IgnoreCaretPointSuggestion();
      return SplitNodeResult(NS_ERROR_FAILURE);
    }
    // If we meet an orphan node before meeting aMostAncestorToSplit, we need
    // to stop splitting.  This is a bug of the caller.
    if (NS_WARN_IF(splittingContent != &aMostAncestorToSplit &&
                   !atStartOfRightNode.GetContainerParentAsContent())) {
      lastResult.IgnoreCaretPointSuggestion();
      return SplitNodeResult(NS_ERROR_FAILURE);
    }
    // If the container is not splitable node such as comment node, atomic
    // element, etc, we should keep it as-is, and try to split its parents.
    if (!HTMLEditUtils::IsSplittableNode(*splittingContent)) {
      if (splittingContent == &aMostAncestorToSplit) {
        return lastResult;
      }
      atStartOfRightNode.Set(splittingContent);
      continue;
    }

    // If the split point is middle of the node or the node is not a text node
    // and we're allowed to create empty element node, split it.
    if ((aSplitAtEdges == SplitAtEdges::eAllowToCreateEmptyContainer &&
         !atStartOfRightNode.GetContainerAsText()) ||
        (!atStartOfRightNode.IsStartOfContainer() &&
         !atStartOfRightNode.IsEndOfContainer())) {
      lastResult = SplitNodeResult::MergeWithDeeperSplitNodeResult(
          SplitNodeWithTransaction(atStartOfRightNode), lastResult);
      if (lastResult.isErr()) {
        NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
        return lastResult;
      }
      MOZ_ASSERT(lastResult.HasCaretPointSuggestion());
      MOZ_ASSERT(lastResult.GetOriginalContent() == splittingContent);
      if (splittingContent == &aMostAncestorToSplit) {
        // Actually, we split aMostAncestorToSplit.
        return lastResult;
      }

      // Then, try to split its parent before current node.
      atStartOfRightNode = lastResult.AtNextContent<EditorDOMPoint>();
    }
    // If the split point is end of the node and it is a text node or we're not
    // allowed to create empty container node, try to split its parent after it.
    else if (!atStartOfRightNode.IsStartOfContainer()) {
      lastResult = SplitNodeResult::HandledButDidNotSplitDueToEndOfContainer(
          *splittingContent, SplitNodeDirection::LeftNodeIsNewOne, &lastResult);
      if (splittingContent == &aMostAncestorToSplit) {
        return lastResult;
      }

      // Try to split its parent after current node.
      atStartOfRightNode.SetAfter(splittingContent);
    }
    // If the split point is start of the node and it is a text node or we're
    // not allowed to create empty container node, try to split its parent.
    else {
      if (splittingContent == &aMostAncestorToSplit) {
        return SplitNodeResult::HandledButDidNotSplitDueToStartOfContainer(
            *splittingContent, SplitNodeDirection::LeftNodeIsNewOne,
            &lastResult);
      }

      // Try to split its parent before current node.
      // XXX This is logically wrong.  If we've already split something but
      //     this is the last splitable content node in the limiter, this
      //     method will return "not handled".
      lastResult = SplitNodeResult::NotHandled(
          atStartOfRightNode, SplitNodeDirection::LeftNodeIsNewOne,
          &lastResult);
      atStartOfRightNode.Set(splittingContent);
    }
  }

  // Not reached because while (true) loop never breaks.
}

SplitNodeResult HTMLEditor::DoSplitNode(const EditorDOMPoint& aStartOfRightNode,
                                        nsIContent& aNewNode) {
  // Ensure computing the offset if it's intialized with a child content node.
  Unused << aStartOfRightNode.Offset();

  // XXX Perhaps, aStartOfRightNode may be invalid if this is a redo
  //     operation after modifying DOM node with JS.
  if (MOZ_UNLIKELY(NS_WARN_IF(!aStartOfRightNode.IsInContentNode()))) {
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }
  MOZ_ASSERT(aStartOfRightNode.IsSetAndValid());

  // Remember all selection points.
  AutoTArray<SavedRange, 10> savedRanges;
  for (SelectionType selectionType : kPresentSelectionTypes) {
    SavedRange savingRange;
    savingRange.mSelection = GetSelection(selectionType);
    if (MOZ_UNLIKELY(NS_WARN_IF(!savingRange.mSelection &&
                                selectionType == SelectionType::eNormal))) {
      return SplitNodeResult(NS_ERROR_FAILURE);
    }
    if (!savingRange.mSelection) {
      // For non-normal selections, skip over the non-existing ones.
      continue;
    }

    for (uint32_t j : IntegerRange(savingRange.mSelection->RangeCount())) {
      const nsRange* r = savingRange.mSelection->GetRangeAt(j);
      MOZ_ASSERT(r);
      MOZ_ASSERT(r->IsPositioned());
      // XXX Looks like that SavedRange should have mStart and mEnd which
      //     are RangeBoundary.  Then, we can avoid to compute offset here.
      savingRange.mStartContainer = r->GetStartContainer();
      savingRange.mStartOffset = r->StartOffset();
      savingRange.mEndContainer = r->GetEndContainer();
      savingRange.mEndOffset = r->EndOffset();

      savedRanges.AppendElement(savingRange);
    }
  }

  nsCOMPtr<nsINode> parent = aStartOfRightNode.GetContainerParent();
  if (MOZ_UNLIKELY(NS_WARN_IF(!parent))) {
    return SplitNodeResult(NS_ERROR_FAILURE);
  }

  // Fix the child before mutation observer may touch the DOM tree.
  nsIContent* firstChildOfRightNode = aStartOfRightNode.GetChild();
  IgnoredErrorResult error;
  parent->InsertBefore(aNewNode, aStartOfRightNode.GetContainer(), error);
  if (MOZ_UNLIKELY(error.Failed())) {
    NS_WARNING("nsINode::InsertBefore() failed");
    return SplitNodeResult(error.StealNSResult());
  }

  // At this point, the existing right node has all the children.  Move all
  // the children which are before aStartOfRightNode.
  if (!aStartOfRightNode.IsStartOfContainer()) {
    // If it's a text node, just shuffle around some text
    Text* rightAsText = aStartOfRightNode.GetContainerAsText();
    Text* leftAsText = aNewNode.GetAsText();
    if (rightAsText && leftAsText) {
      // Fix right node
      nsAutoString leftText;
      IgnoredErrorResult ignoredError;
      rightAsText->SubstringData(0, aStartOfRightNode.Offset(), leftText,
                                 ignoredError);
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "Text::SubstringData() failed, but ignored");
      ignoredError.SuppressException();

      // XXX This call may destroy us.
      DoDeleteText(MOZ_KnownLive(*rightAsText), 0, aStartOfRightNode.Offset(),
                   ignoredError);
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "EditorBase::DoDeleteText() failed, but ignored");
      ignoredError.SuppressException();

      // Fix left node
      // XXX This call may destroy us.
      DoSetText(MOZ_KnownLive(*leftAsText), leftText, ignoredError);
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "EditorBase::DoSetText() failed, but ignored");
    } else {
      MOZ_DIAGNOSTIC_ASSERT(!rightAsText && !leftAsText);
      // Otherwise it's an interior node, so shuffle around the children. Go
      // through list backwards so deletes don't interfere with the iteration.
      if (!firstChildOfRightNode) {
        // XXX Why do we ignore an error while moving nodes from the right node
        //     to the left node?
        IgnoredErrorResult ignoredError;
        MoveAllChildren(*aStartOfRightNode.GetContainer(),
                        EditorRawDOMPoint(&aNewNode, 0u), ignoredError);
        NS_WARNING_ASSERTION(
            !ignoredError.Failed(),
            "HTMLEditor::MoveAllChildren() failed, but ignored");
      } else if (NS_WARN_IF(aStartOfRightNode.GetContainer() !=
                            firstChildOfRightNode->GetParentNode())) {
        // firstChildOfRightNode has been moved by mutation observer.
        // In this case, we what should we do?  Use offset?  But we cannot
        // check if the offset is still expected.
      } else {
        // XXX Why do we ignore an error while moving nodes from the right node
        //     to the left node?
        IgnoredErrorResult ignoredError;
        MovePreviousSiblings(*firstChildOfRightNode,
                             EditorRawDOMPoint(&aNewNode, 0u), ignoredError);
        NS_WARNING_ASSERTION(
            !ignoredError.Failed(),
            "HTMLEditor::MovePreviousSiblings() failed, but ignored");
      }
    }
  }

  // Handle selection
  // TODO: Stop doing this, this shouldn't be necessary to update selection.
  if (RefPtr<PresShell> presShell = GetPresShell()) {
    presShell->FlushPendingNotifications(FlushType::Frames);
  }
  NS_WARNING_ASSERTION(!Destroyed(),
                       "The editor is destroyed during splitting a node");

  const bool allowedTransactionsToChangeSelection =
      AllowsTransactionsToChangeSelection();

  RefPtr<Selection> previousSelection;
  for (SavedRange& savedRange : savedRanges) {
    // If we have not seen the selection yet, clear all of its ranges.
    if (savedRange.mSelection != previousSelection) {
      MOZ_KnownLive(savedRange.mSelection)->RemoveAllRanges(error);
      if (MOZ_UNLIKELY(error.Failed())) {
        NS_WARNING("Selection::RemoveAllRanges() failed");
        return SplitNodeResult(error.StealNSResult());
      }
      previousSelection = savedRange.mSelection;
    }

    // XXX Looks like that we don't need to modify normal selection here
    //     because selection will be modified by the caller if
    //     AllowsTransactionsToChangeSelection() will return true.
    if (allowedTransactionsToChangeSelection &&
        savedRange.mSelection->Type() == SelectionType::eNormal) {
      // If the editor should adjust the selection, don't bother restoring
      // the ranges for the normal selection here.
      continue;
    }

    // Split the selection into existing node and new node.
    if (savedRange.mStartContainer == aStartOfRightNode.GetContainer()) {
      if (savedRange.mStartOffset < aStartOfRightNode.Offset()) {
        savedRange.mStartContainer = &aNewNode;
      } else if (savedRange.mStartOffset >= aStartOfRightNode.Offset()) {
        savedRange.mStartOffset -= aStartOfRightNode.Offset();
      } else {
        NS_WARNING(
            "The stored start offset was smaller than the right node offset");
        savedRange.mStartOffset = 0u;
      }
    }

    if (savedRange.mEndContainer == aStartOfRightNode.GetContainer()) {
      if (savedRange.mEndOffset < aStartOfRightNode.Offset()) {
        savedRange.mEndContainer = &aNewNode;
      } else if (savedRange.mEndOffset >= aStartOfRightNode.Offset()) {
        savedRange.mEndOffset -= aStartOfRightNode.Offset();
      } else {
        NS_WARNING(
            "The stored end offset was smaller than the right node offset");
        savedRange.mEndOffset = 0u;
      }
    }

    RefPtr<nsRange> newRange =
        nsRange::Create(savedRange.mStartContainer, savedRange.mStartOffset,
                        savedRange.mEndContainer, savedRange.mEndOffset, error);
    if (MOZ_UNLIKELY(error.Failed())) {
      NS_WARNING("nsRange::Create() failed");
      return SplitNodeResult(error.StealNSResult());
    }
    // The `MOZ_KnownLive` annotation is only necessary because of a bug
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1622253) in the
    // static analyzer.
    MOZ_KnownLive(savedRange.mSelection)
        ->AddRangeAndSelectFramesAndNotifyListeners(*newRange, error);
    if (MOZ_UNLIKELY(error.Failed())) {
      NS_WARNING(
          "Selection::AddRangeAndSelectFramesAndNotifyListeners() failed");
      return SplitNodeResult(error.StealNSResult());
    }
  }

  // We don't need to set selection here because the caller should do that
  // in any case.

  // If splitting the node causes running mutation event listener and we've
  // got unexpected result, we should return error because callers will
  // continue to do their work without complicated DOM tree result.
  // NOTE: Perhaps, we shouldn't do this immediately after each DOM tree change
  //       because stopping handling it causes some data loss.  E.g., user
  //       may loose the text which is moved to the new text node.
  // XXX We cannot check all descendants in the right node and the new left
  //     node for performance reason.  I think that if caller needs to access
  //     some of the descendants, they should check by themselves.
  if (MOZ_UNLIKELY(
          NS_WARN_IF(parent !=
                     aStartOfRightNode.GetContainer()->GetParentNode()) ||
          NS_WARN_IF(parent != aNewNode.GetParentNode()) ||
          NS_WARN_IF(aNewNode.GetNextSibling() !=
                     aStartOfRightNode.GetContainer()))) {
    return SplitNodeResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  DebugOnly<nsresult> rvIgnored = RangeUpdaterRef().SelAdjSplitNode(
      *aStartOfRightNode.ContainerAsContent(), aStartOfRightNode.Offset(),
      aNewNode, SplitNodeDirection::LeftNodeIsNewOne);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "RangeUpdater::SelAdjSplitNode() failed, but ignored");

  return SplitNodeResult(aNewNode, *aStartOfRightNode.ContainerAsContent(),
                         SplitNodeDirection::LeftNodeIsNewOne);
}

JoinNodesResult HTMLEditor::JoinNodesWithTransaction(
    nsIContent& aLeftContent, nsIContent& aRightContent) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(&aLeftContent != &aRightContent);
  MOZ_ASSERT(aLeftContent.GetParentNode());
  MOZ_ASSERT(aRightContent.GetParentNode());
  MOZ_ASSERT(aLeftContent.GetParentNode() == aRightContent.GetParentNode());

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eJoinNodes, nsIEditor::ePrevious, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return JoinNodesResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  if (MOZ_UNLIKELY(NS_WARN_IF(!aRightContent.GetParentNode()))) {
    return JoinNodesResult(NS_ERROR_FAILURE);
  }

  RefPtr<JoinNodesTransaction> transaction =
      JoinNodesTransaction::MaybeCreate(*this, aLeftContent, aRightContent);
  if (MOZ_UNLIKELY(!transaction)) {
    NS_WARNING("JoinNodesTransaction::MaybeCreate() failed");
    return JoinNodesResult(NS_ERROR_FAILURE);
  }

  const nsresult rv = DoTransactionInternal(transaction);
  // FYI: Now, DidJoinNodesTransaction() must have been run if succeeded.
  if (MOZ_UNLIKELY(NS_WARN_IF(Destroyed()))) {
    return JoinNodesResult(NS_ERROR_EDITOR_DESTROYED);
  }

  // This shouldn't occur unless the cycle collector runs by chrome script
  // forcibly.
  if (MOZ_UNLIKELY(NS_WARN_IF(!transaction->GetRemovedContent()) ||
                   NS_WARN_IF(!transaction->GetExistingContent()))) {
    return JoinNodesResult(NS_ERROR_UNEXPECTED);
  }

  // If joined node is moved to different place, offset may not have any
  // meaning.  In this case, the web app modified the DOM tree takes on the
  // responsibility for the remaning things.
  if (MOZ_UNLIKELY(NS_WARN_IF(transaction->GetExistingContent()->GetParent() !=
                              transaction->GetParentNode()))) {
    return JoinNodesResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  if (MOZ_UNLIKELY(NS_FAILED(rv))) {
    NS_WARNING("EditorBase::DoTransactionInternal() failed");
    return JoinNodesResult(rv);
  }

  return JoinNodesResult(transaction->CreateJoinedPoint<EditorDOMPoint>(),
                         *transaction->GetRemovedContent(),
                         JoinNodesDirection::LeftNodeIntoRightNode);
}

void HTMLEditor::DidJoinNodesTransaction(
    const JoinNodesTransaction& aTransaction, nsresult aDoJoinNodesResult) {
  // This shouldn't occur unless the cycle collector runs by chrome script
  // forcibly.
  if (MOZ_UNLIKELY(NS_WARN_IF(!aTransaction.GetRemovedContent()) ||
                   NS_WARN_IF(!aTransaction.GetExistingContent()))) {
    return;
  }

  // If joined node is moved to different place, offset may not have any
  // meaning.  In this case, the web app modified the DOM tree takes on the
  // responsibility for the remaning things.
  if (MOZ_UNLIKELY(aTransaction.GetExistingContent()->GetParentNode() !=
                   aTransaction.GetParentNode())) {
    return;
  }

  // Be aware, the joined point should be created for each call because
  // they may refer the child node, but some of them may change the DOM tree
  // after that, thus we need to avoid invalid point (Although it shouldn't
  // occur).
  TopLevelEditSubActionDataRef().DidJoinContents(
      *this, aTransaction.CreateJoinedPoint<EditorRawDOMPoint>());

  if (NS_SUCCEEDED(aDoJoinNodesResult)) {
    if (RefPtr<TextServicesDocument> textServicesDocument =
            mTextServicesDocument) {
      textServicesDocument->DidJoinContents(
          aTransaction.CreateJoinedPoint<EditorRawDOMPoint>(),
          *aTransaction.GetRemovedContent(),
          JoinNodesDirection::LeftNodeIntoRightNode);
    }
  }

  if (!mActionListeners.IsEmpty()) {
    for (auto& listener : mActionListeners.Clone()) {
      DebugOnly<nsresult> rvIgnored = listener->DidJoinContents(
          aTransaction.CreateJoinedPoint<EditorRawDOMPoint>(),
          aTransaction.GetRemovedContent(), true);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::DidJoinContents() failed, but ignored");
    }
  }
}

nsresult HTMLEditor::DoJoinNodes(nsIContent& aContentToKeep,
                                 nsIContent& aContentToRemove) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  const uint32_t removingContentLength = aContentToRemove.Length();
  const Maybe<uint32_t> removingContentIndex =
      aContentToRemove.ComputeIndexInParentNode();

  // Remember all selection points.
  // XXX Do we need to restore all types of selections by ourselves?  Normal
  //     selection should be modified later as result of handling edit action.
  //     IME selections shouldn't be there when nodes are joined.  Spellcheck
  //     selections should be recreated with newer text.  URL selections
  //     shouldn't be there because of used only by the URL bar.
  AutoTArray<SavedRange, 10> savedRanges;
  {
    EditorRawDOMPoint atRemovingNode(&aContentToRemove);
    EditorRawDOMPoint atNodeToKeep(&aContentToKeep);
    for (SelectionType selectionType : kPresentSelectionTypes) {
      SavedRange savingRange;
      savingRange.mSelection = GetSelection(selectionType);
      if (selectionType == SelectionType::eNormal) {
        if (NS_WARN_IF(!savingRange.mSelection)) {
          return NS_ERROR_FAILURE;
        }
      } else if (!savingRange.mSelection) {
        // For non-normal selections, skip over the non-existing ones.
        continue;
      }

      const uint32_t rangeCount = savingRange.mSelection->RangeCount();
      for (const uint32_t j : IntegerRange(rangeCount)) {
        MOZ_ASSERT(savingRange.mSelection->RangeCount() == rangeCount);
        const RefPtr<nsRange> r = savingRange.mSelection->GetRangeAt(j);
        MOZ_ASSERT(r);
        MOZ_ASSERT(r->IsPositioned());
        savingRange.mStartContainer = r->GetStartContainer();
        savingRange.mStartOffset = r->StartOffset();
        savingRange.mEndContainer = r->GetEndContainer();
        savingRange.mEndOffset = r->EndOffset();

        // If selection endpoint is between the nodes, remember it as being
        // in the one that is going away instead.  This simplifies later
        // selection adjustment logic at end of this method.
        if (savingRange.mStartContainer) {
          if (savingRange.mStartContainer == atNodeToKeep.GetContainer() &&
              atRemovingNode.Offset() < savingRange.mStartOffset &&
              savingRange.mStartOffset <= atNodeToKeep.Offset()) {
            savingRange.mStartContainer = &aContentToRemove;
            savingRange.mStartOffset = removingContentLength;
          }
          if (savingRange.mEndContainer == atNodeToKeep.GetContainer() &&
              atRemovingNode.Offset() < savingRange.mEndOffset &&
              savingRange.mEndOffset <= atNodeToKeep.Offset()) {
            savingRange.mEndContainer = &aContentToRemove;
            savingRange.mEndOffset = removingContentLength;
          }
        }

        savedRanges.AppendElement(savingRange);
      }
    }
  }

  // OK, ready to do join now.
  nsresult rv = [&]() MOZ_CAN_RUN_SCRIPT {
    // If it's a text node, just shuffle around some text.
    if (aContentToKeep.IsText() && aContentToRemove.IsText()) {
      nsAutoString rightText;
      nsAutoString leftText;
      aContentToKeep.AsText()->GetData(rightText);
      aContentToRemove.AsText()->GetData(leftText);
      leftText += rightText;
      IgnoredErrorResult ignoredError;
      DoSetText(MOZ_KnownLive(*aContentToKeep.AsText()), leftText,
                ignoredError);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "EditorBase::DoSetText() failed, but ignored");
      return NS_OK;
    }
    // Otherwise it's an interior node, so shuffle around the children.
    nsCOMPtr<nsINodeList> childNodes = aContentToRemove.ChildNodes();
    MOZ_ASSERT(childNodes);

    // Remember the first child in aContentToKeep, we'll insert all the children
    // of aContentToRemove in front of it GetFirstChild returns nullptr
    // firstChild if aContentToKeep has no children, that's OK.
    nsCOMPtr<nsIContent> firstChild = aContentToKeep.GetFirstChild();

    // Have to go through the list backwards to keep deletes from interfering
    // with iteration.
    for (uint32_t i = childNodes->Length(); i; --i) {
      nsCOMPtr<nsIContent> childNode = childNodes->Item(i - 1);
      if (childNode) {
        // prepend children of aContentToRemove
        IgnoredErrorResult error;
        aContentToKeep.InsertBefore(*childNode, firstChild, error);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (error.Failed()) {
          NS_WARNING("nsINode::InsertBefore() failed");
          return error.StealNSResult();
        }
        firstChild = std::move(childNode);
      }
    }
    return NS_OK;
  }();

  // Delete the extra node.
  if (NS_SUCCEEDED(rv)) {
    aContentToRemove.Remove();
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
  }

  if (MOZ_LIKELY(removingContentIndex.isSome())) {
    DebugOnly<nsresult> rvIgnored = RangeUpdaterRef().SelAdjJoinNodes(
        EditorRawDOMPoint(&aContentToKeep, std::min(removingContentLength,
                                                    aContentToKeep.Length())),
        aContentToRemove, *removingContentIndex,
        JoinNodesDirection::LeftNodeIntoRightNode);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "RangeUpdater::SelAdjJoinNodes() failed, but ignored");
  }
  if (MOZ_UNLIKELY(NS_FAILED(rv))) {
    return rv;
  }

  const bool allowedTransactionsToChangeSelection =
      AllowsTransactionsToChangeSelection();

  // And adjust the selection if needed.
  RefPtr<Selection> previousSelection;
  for (SavedRange& savedRange : savedRanges) {
    // If we have not seen the selection yet, clear all of its ranges.
    if (savedRange.mSelection != previousSelection) {
      IgnoredErrorResult error;
      MOZ_KnownLive(savedRange.mSelection)->RemoveAllRanges(error);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (error.Failed()) {
        NS_WARNING("Selection::RemoveAllRanges() failed");
        return error.StealNSResult();
      }
      previousSelection = savedRange.mSelection;
    }

    if (allowedTransactionsToChangeSelection &&
        savedRange.mSelection->Type() == SelectionType::eNormal) {
      // If the editor should adjust the selection, don't bother restoring
      // the ranges for the normal selection here.
      continue;
    }

    // Check to see if we joined nodes where selection starts.
    if (savedRange.mStartContainer == &aContentToRemove) {
      savedRange.mStartContainer = &aContentToKeep;
    } else if (savedRange.mStartContainer == &aContentToKeep) {
      savedRange.mStartOffset += removingContentLength;
    }

    // Check to see if we joined nodes where selection ends.
    if (savedRange.mEndContainer == &aContentToRemove) {
      savedRange.mEndContainer = &aContentToKeep;
    } else if (savedRange.mEndContainer == &aContentToKeep) {
      savedRange.mEndOffset += removingContentLength;
    }

    const RefPtr<nsRange> newRange = nsRange::Create(
        savedRange.mStartContainer, savedRange.mStartOffset,
        savedRange.mEndContainer, savedRange.mEndOffset, IgnoreErrors());
    if (!newRange) {
      NS_WARNING("nsRange::Create() failed");
      return NS_ERROR_FAILURE;
    }

    IgnoredErrorResult error;
    // The `MOZ_KnownLive` annotation is only necessary because of a bug
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1622253) in the
    // static analyzer.
    MOZ_KnownLive(savedRange.mSelection)
        ->AddRangeAndSelectFramesAndNotifyListeners(*newRange, error);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  if (allowedTransactionsToChangeSelection) {
    // Editor wants us to set selection at join point.
    DebugOnly<nsresult> rvIgnored = CollapseSelectionTo(
        EditorRawDOMPoint(&aContentToKeep, removingContentLength));
    if (MOZ_UNLIKELY(rv == NS_ERROR_EDITOR_DESTROYED)) {
      NS_WARNING(
          "EditorBase::CollapseSelectionTo() caused destroying the editor");
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBases::CollapseSelectionTos() failed, but ignored");
  }

  return NS_OK;
}

MoveNodeResult HTMLEditor::MoveNodeWithTransaction(
    nsIContent& aContentToMove, const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  EditorDOMPoint oldPoint(&aContentToMove);
  if (NS_WARN_IF(!oldPoint.IsSet())) {
    return MoveNodeResult(NS_ERROR_FAILURE);
  }

  // Don't do anything if it's already in right place.
  if (aPointToInsert == oldPoint) {
    return MoveNodeIgnored(aPointToInsert.NextPoint());
  }

  RefPtr<MoveNodeTransaction> moveNodeTransaction =
      MoveNodeTransaction::MaybeCreate(*this, aContentToMove, aPointToInsert);
  if (MOZ_UNLIKELY(!moveNodeTransaction)) {
    NS_WARNING("MoveNodeTransaction::MaybeCreate() failed");
    return MoveNodeResult(NS_ERROR_FAILURE);
  }

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eMoveNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return MoveNodeResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  TopLevelEditSubActionDataRef().WillDeleteContent(*this, aContentToMove);

  nsresult rv = DoTransactionInternal(moveNodeTransaction);
  if (NS_SUCCEEDED(rv)) {
    if (mTextServicesDocument) {
      const OwningNonNull<TextServicesDocument> textServicesDocument =
          *mTextServicesDocument;
      textServicesDocument->DidDeleteContent(aContentToMove);
    }
  }

  if (!mActionListeners.IsEmpty()) {
    for (auto& listener : mActionListeners.Clone()) {
      DebugOnly<nsresult> rvIgnored =
          listener->DidDeleteNode(&aContentToMove, rv);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::DidDeleteNode() failed, but ignored");
    }
  }

  if (MOZ_UNLIKELY(Destroyed())) {
    NS_WARNING(
        "MoveNodeTransaction::DoTransaction() caused destroying the editor");
    return MoveNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("MoveNodeTransaction::DoTransaction() failed");
    return MoveNodeResult(rv);
  }

  TopLevelEditSubActionDataRef().DidInsertContent(*this, aContentToMove);

  return MoveNodeHandled(
      moveNodeTransaction->SuggestNextInsertionPoint<EditorDOMPoint>(),
      moveNodeTransaction->SuggestPointToPutCaret<EditorDOMPoint>());
}

Result<RefPtr<Element>, nsresult> HTMLEditor::DeleteSelectionAndCreateElement(
    nsAtom& aTag, const InitializeInsertingElement& aInitializer) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsresult rv = DeleteSelectionAndPrepareToCreateNode();
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteSelectionAndPrepareToCreateNode() failed");
    return Err(rv);
  }

  EditorDOMPoint pointToInsert(SelectionRef().AnchorRef());
  if (!pointToInsert.IsSet()) {
    return Err(NS_ERROR_FAILURE);
  }
  CreateElementResult createNewElementResult = CreateAndInsertElement(
      WithTransaction::Yes, aTag, pointToInsert, aInitializer);
  if (createNewElementResult.isErr()) {
    NS_WARNING(
        "HTMLEditor::CreateAndInsertElement(WithTransaction::Yes) failed");
    return Err(createNewElementResult.unwrapErr());
  }
  MOZ_ASSERT(createNewElementResult.GetNewNode());

  // We want the selection to be just after the new node
  createNewElementResult.IgnoreCaretPointSuggestion();
  rv = CollapseSelectionTo(
      EditorRawDOMPoint::After(*createNewElementResult.GetNewNode()));
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::CollapseSelectionTo() failed");
    return Err(rv);
  }
  return createNewElementResult.UnwrapNewNode();
}

nsresult HTMLEditor::DeleteSelectionAndPrepareToCreateNode() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!SelectionRef().GetAnchorFocusRange())) {
    return NS_OK;
  }

  if (!SelectionRef().GetAnchorFocusRange()->Collapsed()) {
    nsresult rv =
        DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eStrip);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteSelectionAsSubAction() failed");
      return rv;
    }
    MOZ_ASSERT(SelectionRef().GetAnchorFocusRange() &&
                   SelectionRef().GetAnchorFocusRange()->Collapsed(),
               "Selection not collapsed after delete");
  }

  // If the selection is a chardata node, split it if necessary and compute
  // where to put the new node
  EditorDOMPoint atAnchor(SelectionRef().AnchorRef());
  if (NS_WARN_IF(!atAnchor.IsSet()) || !atAnchor.IsInDataNode()) {
    return NS_OK;
  }

  if (NS_WARN_IF(!atAnchor.GetContainerParent())) {
    return NS_ERROR_FAILURE;
  }

  if (atAnchor.IsStartOfContainer()) {
    const EditorRawDOMPoint atAnchorContainer(atAnchor.GetContainer());
    if (NS_WARN_IF(!atAnchorContainer.IsSetAndValid())) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = CollapseSelectionTo(atAnchorContainer);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::CollapseSelectionTo() failed");
    return rv;
  }

  if (atAnchor.IsEndOfContainer()) {
    EditorRawDOMPoint afterAnchorContainer(atAnchor.GetContainer());
    if (NS_WARN_IF(!afterAnchorContainer.AdvanceOffset())) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = CollapseSelectionTo(afterAnchorContainer);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::CollapseSelectionTo() failed");
    return rv;
  }

  const SplitNodeResult splitAtAnchorResult =
      SplitNodeWithTransaction(atAnchor);
  if (splitAtAnchorResult.isErr()) {
    NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
    return splitAtAnchorResult.unwrapErr();
  }

  const EditorRawDOMPoint& atRightContent =
      splitAtAnchorResult.AtNextContent<EditorRawDOMPoint>();
  if (NS_WARN_IF(!atRightContent.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atRightContent.IsSetAndValid());
  splitAtAnchorResult.IgnoreCaretPointSuggestion();
  nsresult rv = CollapseSelectionTo(atRightContent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CollapseSelectionTo() failed");
  return rv;
}

bool HTMLEditor::IsEmpty() const {
  if (mPaddingBRElementForEmptyEditor) {
    return true;
  }

  // XXX Oddly, we check body or document element's state instead of
  //     active editing host.  Must be a bug.
  Element* bodyOrDocumentElement = GetRoot();
  if (!bodyOrDocumentElement) {
    return true;
  }

  for (nsIContent* childContent = bodyOrDocumentElement->GetFirstChild();
       childContent; childContent = childContent->GetNextSibling()) {
    if (!childContent->IsText() || childContent->Length()) {
      return false;
    }
  }
  return true;
}

// add to aElement the CSS inline styles corresponding to the HTML attribute
// aAttribute with its value aValue
nsresult HTMLEditor::SetAttributeOrEquivalent(Element* aElement,
                                              nsAtom* aAttribute,
                                              const nsAString& aValue,
                                              bool aSuppressTransaction) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aAttribute);

  nsAutoScriptBlocker scriptBlocker;
  nsStyledElement* styledElement = nsStyledElement::FromNodeOrNull(aElement);
  if (!IsCSSEnabled() || !mCSSEditUtils) {
    // we are not in an HTML+CSS editor; let's set the attribute the HTML way
    if (mCSSEditUtils && styledElement) {
      // MOZ_KnownLive(*styledElement): It's aElement and its lifetime must
      // be guaranteed by the caller because of MOZ_CAN_RUN_SCRIPT method.
      nsresult rv =
          aSuppressTransaction
              ? mCSSEditUtils->RemoveCSSEquivalentToHTMLStyleWithoutTransaction(
                    MOZ_KnownLive(*styledElement), nullptr, aAttribute, nullptr)
              : mCSSEditUtils->RemoveCSSEquivalentToHTMLStyleWithTransaction(
                    MOZ_KnownLive(*styledElement), nullptr, aAttribute,
                    nullptr);
      if (rv == NS_ERROR_EDITOR_DESTROYED) {
        NS_WARNING(
            "CSSEditUtils::RemoveCSSEquivalentToHTMLStyle*Transaction() "
            "destroyed the editor");
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "CSSEditUtils::RemoveCSSEquivalentToHTMLStyle*Transaction() "
          "failed, but ignored");
    }
    if (aSuppressTransaction) {
      nsresult rv =
          aElement->SetAttr(kNameSpaceID_None, aAttribute, aValue, true);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Element::SetAttr() failed");
      return rv;
    }
    nsresult rv = SetAttributeWithTransaction(*aElement, *aAttribute, aValue);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::SetAttributeWithTransaction() failed");
    return rv;
  }

  if (styledElement) {
    // MOZ_KnownLive(*styledElement): It's aElement and its lifetime must
    // be guaranteed by the caller because of MOZ_CAN_RUN_SCRIPT method.
    Result<int32_t, nsresult> count =
        aSuppressTransaction
            ? mCSSEditUtils->SetCSSEquivalentToHTMLStyleWithoutTransaction(
                  MOZ_KnownLive(*styledElement), nullptr, aAttribute, &aValue)
            : mCSSEditUtils->SetCSSEquivalentToHTMLStyleWithTransaction(
                  MOZ_KnownLive(*styledElement), nullptr, aAttribute, &aValue);
    if (count.isErr()) {
      if (count.inspectErr() == NS_ERROR_EDITOR_DESTROYED) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING(
          "CSSEditUtils::SetCSSEquivalentToHTMLStyle*Transaction() failed, but "
          "ignored");
    }
    if (count.inspect()) {
      // we found an equivalence ; let's remove the HTML attribute itself if it
      // is set
      nsAutoString existingValue;
      if (!aElement->GetAttr(kNameSpaceID_None, aAttribute, existingValue)) {
        return NS_OK;
      }

      if (aSuppressTransaction) {
        nsresult rv = aElement->UnsetAttr(kNameSpaceID_None, aAttribute, true);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Element::UnsetAttr() failed");
        return rv;
      }
      nsresult rv = RemoveAttributeWithTransaction(*aElement, *aAttribute);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "EditorBase::RemoveAttributeWithTransaction() failed");
      return rv;
    }
  }

  // count is an integer that represents the number of CSS declarations
  // applied to the element. If it is zero, we found no equivalence in this
  // implementation for the attribute
  if (aAttribute == nsGkAtoms::style) {
    // if it is the style attribute, just add the new value to the existing
    // style attribute's value
    nsAutoString existingValue;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::style, existingValue);
    existingValue.Append(HTMLEditUtils::kSpace);
    existingValue.Append(aValue);
    if (aSuppressTransaction) {
      nsresult rv = aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
                                      existingValue, true);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "Element::SetAttr(nsGkAtoms::style) failed");
      return rv;
    }
    nsresult rv = SetAttributeWithTransaction(*aElement, *nsGkAtoms::style,
                                              existingValue);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::SetAttributeWithTransaction(nsGkAtoms::style) failed");
    return rv;
  }

  // we have no CSS equivalence for this attribute and it is not the style
  // attribute; let's set it the good'n'old HTML way
  if (aSuppressTransaction) {
    nsresult rv =
        aElement->SetAttr(kNameSpaceID_None, aAttribute, aValue, true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Element::SetAttr() failed");
    return rv;
  }
  nsresult rv = SetAttributeWithTransaction(*aElement, *aAttribute, aValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::SetAttributeWithTransaction() failed");
  return rv;
}

nsresult HTMLEditor::RemoveAttributeOrEquivalent(Element* aElement,
                                                 nsAtom* aAttribute,
                                                 bool aSuppressTransaction) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aAttribute);

  if (IsCSSEnabled() && mCSSEditUtils &&
      CSSEditUtils::IsCSSEditableProperty(aElement, nullptr, aAttribute)) {
    // XXX It might be keep handling attribute even if aElement is not
    //     an nsStyledElement instance.
    nsStyledElement* styledElement = nsStyledElement::FromNodeOrNull(aElement);
    if (NS_WARN_IF(!styledElement)) {
      return NS_ERROR_INVALID_ARG;
    }
    // MOZ_KnownLive(*styledElement): It's aElement and its lifetime must
    // be guaranteed by the caller because of MOZ_CAN_RUN_SCRIPT method.
    nsresult rv =
        aSuppressTransaction
            ? mCSSEditUtils->RemoveCSSEquivalentToHTMLStyleWithoutTransaction(
                  MOZ_KnownLive(*styledElement), nullptr, aAttribute, nullptr)
            : mCSSEditUtils->RemoveCSSEquivalentToHTMLStyleWithTransaction(
                  MOZ_KnownLive(*styledElement), nullptr, aAttribute, nullptr);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "CSSEditUtils::RemoveCSSEquivalentToHTMLStyle*Transaction() failed");
      return rv;
    }
  }

  if (!aElement->HasAttr(kNameSpaceID_None, aAttribute)) {
    return NS_OK;
  }

  if (aSuppressTransaction) {
    nsresult rv = aElement->UnsetAttr(kNameSpaceID_None, aAttribute,
                                      /* aNotify = */ true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Element::UnsetAttr() failed");
    return rv;
  }
  nsresult rv = RemoveAttributeWithTransaction(*aElement, *aAttribute);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::RemoveAttributeWithTransaction() failed");
  return rv;
}

NS_IMETHODIMP HTMLEditor::SetIsCSSEnabled(bool aIsCSSPrefChecked) {
  if (!mCSSEditUtils) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eEnableOrDisableCSS);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mCSSEditUtils->SetCSSEnabled(aIsCSSPrefChecked);
  return NS_OK;
}

// Set the block background color
nsresult HTMLEditor::SetCSSBackgroundColorWithTransaction(
    const nsAString& aColor) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CommitComposition();

  // XXX Shouldn't we do this before calling `CommitComposition()`?
  if (IsInPlaintextMode()) {
    return NS_OK;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result.Rv();
  }

  bool selectionIsCollapsed = SelectionRef().IsCollapsed();

  AutoPlaceholderBatch treatAsOneTransaction(
      *this, ScrollSelectionIntoView::Yes, __FUNCTION__);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertElement, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "HTMLEditor::OnStartToHandleTopLevelEditSubAction() "
                       "failed, but ignored");

  {
    AutoSelectionRestorer restoreSelectionLater(*this);
    AutoTransactionsConserveSelection dontChangeMySelection(*this);

    // Loop through the ranges in the selection
    // XXX This is different from `SetInlinePropertyInternal()`.  It uses
    //     AutoSelectionRangeArray to store all ranges first.  The result may be
    //     different if mutation event listener changes the `Selection`.
    // TODO: Store all selection ranges first since this updates the style.
    for (uint32_t i = 0; i < SelectionRef().RangeCount(); i++) {
      RefPtr<nsRange> range = SelectionRef().GetRangeAt(i);
      if (NS_WARN_IF(!range)) {
        return NS_ERROR_FAILURE;
      }

      EditorDOMPoint startOfRange(range->StartRef());
      EditorDOMPoint endOfRange(range->EndRef());
      if (NS_WARN_IF(!startOfRange.IsSet()) ||
          NS_WARN_IF(!endOfRange.IsSet())) {
        continue;
      }

      if (startOfRange.GetContainer() == endOfRange.GetContainer()) {
        // If the range is in a text node, set background color of its parent
        // block.
        if (startOfRange.IsInTextNode()) {
          if (const RefPtr<nsStyledElement> editableBlockStyledElement =
                  nsStyledElement::FromNodeOrNull(
                      HTMLEditUtils::GetAncestorElement(
                          *startOfRange.ContainerAsText(),
                          HTMLEditUtils::ClosestEditableBlockElement))) {
            Result<int32_t, nsresult> result =
                mCSSEditUtils->SetCSSEquivalentToHTMLStyleWithTransaction(
                    *editableBlockStyledElement, nullptr, nsGkAtoms::bgcolor,
                    &aColor);
            if (result.isErr()) {
              if (result.inspectErr() == NS_ERROR_EDITOR_DESTROYED) {
                NS_WARNING(
                    "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                    "nsGkAtoms::bgcolor) destroyed the editor");
                return NS_ERROR_EDITOR_DESTROYED;
              }
              NS_WARNING(
                  "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                  "nsGkAtoms::bgcolor) failed, but ignored");
            }
          }
          continue;
        }

        // If `Selection` is collapsed in a `<body>` element, set background
        // color of the `<body>` element.
        // XXX Why do we refer whether the `Selection` is collapsed rather
        //     than the `nsRange` is collapsed?
        if (startOfRange.GetContainer()->IsHTMLElement(nsGkAtoms::body) &&
            selectionIsCollapsed) {
          if (RefPtr<nsStyledElement> styledElement =
                  startOfRange.GetContainerAsStyledElement()) {
            Result<int32_t, nsresult> result =
                mCSSEditUtils->SetCSSEquivalentToHTMLStyleWithTransaction(
                    *styledElement, nullptr, nsGkAtoms::bgcolor, &aColor);
            if (result.isErr()) {
              if (result.inspectErr() == NS_ERROR_EDITOR_DESTROYED) {
                NS_WARNING(
                    "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                    "nsGkAtoms::bgcolor) destroyed the editor");
                return NS_ERROR_EDITOR_DESTROYED;
              }
              NS_WARNING(
                  "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                  "nsGkAtoms::bgcolor) failed, but ignored");
            }
          }
          continue;
        }
        // If one node is selected, set background color of it if it's a
        // block, or of its parent block otherwise.
        if ((startOfRange.IsStartOfContainer() &&
             endOfRange.IsStartOfContainer()) ||
            startOfRange.Offset() + 1 == endOfRange.Offset()) {
          if (NS_WARN_IF(startOfRange.IsInDataNode())) {
            continue;
          }
          if (const RefPtr<nsStyledElement> editableBlockStyledElement =
                  nsStyledElement::FromNodeOrNull(
                      HTMLEditUtils::GetInclusiveAncestorElement(
                          *startOfRange.GetChild(),
                          HTMLEditUtils::ClosestEditableBlockElement))) {
            Result<int32_t, nsresult> result =
                mCSSEditUtils->SetCSSEquivalentToHTMLStyleWithTransaction(
                    *editableBlockStyledElement, nullptr, nsGkAtoms::bgcolor,
                    &aColor);
            if (result.isErr()) {
              if (result.inspectErr() == NS_ERROR_EDITOR_DESTROYED) {
                NS_WARNING(
                    "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                    "nsGkAtoms::bgcolor) destroyed the editor");
                return NS_ERROR_EDITOR_DESTROYED;
              }
              NS_WARNING(
                  "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                  "nsGkAtoms::bgcolor) failed, but ignored");
            }
          }
          continue;
        }
      }

      // Collect editable nodes which are entirely contained in the range.
      AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
      ContentSubtreeIterator subtreeIter;
      // If there is no node which is entirely in the range,
      // `ContentSubtreeIterator::Init()` fails, but this is possible case,
      // don't warn it.
      nsresult rv = subtreeIter.Init(range);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "ContentSubtreeIterator::Init() failed, but ignored");
      if (NS_SUCCEEDED(rv)) {
        for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
          nsINode* node = subtreeIter.GetCurrentNode();
          if (NS_WARN_IF(!node)) {
            return NS_ERROR_FAILURE;
          }
          if (node->IsContent() && EditorUtils::IsEditableContent(
                                       *node->AsContent(), EditorType::HTML)) {
            arrayOfContents.AppendElement(*node->AsContent());
          }
        }
      }

      // This caches block parent if we set its background color.
      RefPtr<Element> handledBlockParent;

      // If start node is a text node, set background color of its parent
      // block.
      if (startOfRange.IsInTextNode() &&
          EditorUtils::IsEditableContent(*startOfRange.ContainerAsText(),
                                         EditorType::HTML)) {
        Element* const editableBlockElement = HTMLEditUtils::GetAncestorElement(
            *startOfRange.ContainerAsText(),
            HTMLEditUtils::ClosestEditableBlockElement);
        if (editableBlockElement &&
            handledBlockParent != editableBlockElement) {
          handledBlockParent = editableBlockElement;
          if (nsStyledElement* blockStyledElement =
                  nsStyledElement::FromNode(handledBlockParent)) {
            // MOZ_KnownLive(*blockStyledElement): It's handledBlockParent
            // whose type is RefPtr.
            Result<int32_t, nsresult> result =
                mCSSEditUtils->SetCSSEquivalentToHTMLStyleWithTransaction(
                    MOZ_KnownLive(*blockStyledElement), nullptr,
                    nsGkAtoms::bgcolor, &aColor);
            if (result.isErr()) {
              if (result.inspectErr() == NS_ERROR_EDITOR_DESTROYED) {
                NS_WARNING(
                    "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                    "nsGkAtoms::bgcolor) destroyed the editor");
                return NS_ERROR_EDITOR_DESTROYED;
              }
              NS_WARNING(
                  "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                  "nsGkAtoms::bgcolor) failed, but ignored");
            }
          }
        }
      }

      // Then, set background color of each block or block parent of all nodes
      // in the range entirely.
      for (OwningNonNull<nsIContent>& content : arrayOfContents) {
        Element* const editableBlockElement =
            HTMLEditUtils::GetInclusiveAncestorElement(
                content, HTMLEditUtils::ClosestEditableBlockElement);
        if (editableBlockElement &&
            handledBlockParent != editableBlockElement) {
          handledBlockParent = editableBlockElement;
          if (nsStyledElement* blockStyledElement =
                  nsStyledElement::FromNode(handledBlockParent)) {
            // MOZ_KnownLive(*blockStyledElement): It's handledBlockParent whose
            // type is RefPtr.
            Result<int32_t, nsresult> result =
                mCSSEditUtils->SetCSSEquivalentToHTMLStyleWithTransaction(
                    MOZ_KnownLive(*blockStyledElement), nullptr,
                    nsGkAtoms::bgcolor, &aColor);
            if (result.isErr()) {
              if (result.inspectErr() == NS_ERROR_EDITOR_DESTROYED) {
                NS_WARNING(
                    "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                    "nsGkAtoms::bgcolor) destroyed the editor");
                return NS_ERROR_EDITOR_DESTROYED;
              }
              NS_WARNING(
                  "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                  "nsGkAtoms::bgcolor) failed, but ignored");
            }
          }
        }
      }

      // Finally, if end node is a text node, set background color of its
      // parent block.
      if (endOfRange.IsInTextNode() &&
          EditorUtils::IsEditableContent(*endOfRange.ContainerAsText(),
                                         EditorType::HTML)) {
        Element* const editableBlockElement = HTMLEditUtils::GetAncestorElement(
            *endOfRange.ContainerAsText(),
            HTMLEditUtils::ClosestEditableBlockElement);
        if (editableBlockElement &&
            handledBlockParent != editableBlockElement) {
          if (RefPtr<nsStyledElement> blockStyledElement =
                  nsStyledElement::FromNode(editableBlockElement)) {
            Result<int32_t, nsresult> result =
                mCSSEditUtils->SetCSSEquivalentToHTMLStyleWithTransaction(
                    *blockStyledElement, nullptr, nsGkAtoms::bgcolor, &aColor);
            if (result.isErr()) {
              if (result.inspectErr() == NS_ERROR_EDITOR_DESTROYED) {
                NS_WARNING(
                    "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                    "nsGkAtoms::bgcolor) destroyed the editor");
                return NS_ERROR_EDITOR_DESTROYED;
              }
              NS_WARNING(
                  "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                  "nsGkAtoms::bgcolor) failed, but ignored");
            }
          }
        }
      }
    }
  }

  // Restoring `Selection` may cause destroying us.
  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

NS_IMETHODIMP HTMLEditor::SetBackgroundColor(const nsAString& aColor) {
  nsresult rv = SetBackgroundColorAsAction(aColor);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::SetBackgroundColorAsAction() failed");
  return rv;
}

nsresult HTMLEditor::SetBackgroundColorAsAction(const nsAString& aColor,
                                                nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eSetBackgroundColor, aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (IsCSSEnabled()) {
    // if we are in CSS mode, we have to apply the background color to the
    // containing block (or the body if we have no block-level element in
    // the document)
    nsresult rv = SetCSSBackgroundColorWithTransaction(aColor);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::SetCSSBackgroundColorWithTransaction() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // but in HTML mode, we can only set the document's background color
  rv = SetHTMLBackgroundColorWithTransaction(aColor);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::SetHTMLBackgroundColorWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

Result<EditorDOMPoint, nsresult>
HTMLEditor::CopyLastEditableChildStylesWithTransaction(
    Element& aPreviousBlock, Element& aNewBlock, const Element& aEditingHost) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // First, clear out aNewBlock.  Contract is that we want only the styles
  // from aPreviousBlock.
  AutoTArray<OwningNonNull<nsIContent>, 32> newBlockChildren;
  HTMLEditor::GetChildNodesOf(aNewBlock, newBlockChildren);
  for (const OwningNonNull<nsIContent>& child : newBlockChildren) {
    // MOZ_KNownLive(child) because of bug 1622253
    nsresult rv = DeleteNodeWithTransaction(MOZ_KnownLive(child));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
  }
  if (MOZ_UNLIKELY(aNewBlock.GetFirstChild())) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  // XXX aNewBlock may be moved or removed.  Even in such case, we should
  //     keep cloning the styles?

  // Look for the deepest last editable leaf node in aPreviousBlock.
  // Then, if found one is a <br> element, look for non-<br> element.
  nsIContent* deepestEditableContent = nullptr;
  for (nsCOMPtr<nsIContent> child = &aPreviousBlock; child;
       child = HTMLEditUtils::GetLastChild(
           *child, {WalkTreeOption::IgnoreNonEditableNode})) {
    deepestEditableContent = child;
  }
  while (deepestEditableContent &&
         deepestEditableContent->IsHTMLElement(nsGkAtoms::br)) {
    deepestEditableContent = HTMLEditUtils::GetPreviousContent(
        *deepestEditableContent, {WalkTreeOption::IgnoreNonEditableNode},
        &aEditingHost);
  }
  if (!deepestEditableContent) {
    return EditorDOMPoint(&aNewBlock, 0u);
  }

  Element* deepestVisibleEditableElement =
      deepestEditableContent->GetAsElementOrParentElement();
  if (!deepestVisibleEditableElement) {
    return EditorDOMPoint(&aNewBlock, 0u);
  }

  // Clone inline elements to keep current style in the new block.
  // XXX Looks like that this is really slow if lastEditableDescendant is
  //     far from aPreviousBlock.  Probably, we should clone inline containers
  //     from ancestor to descendants without transactions, then, insert it
  //     after that with transaction.
  RefPtr<Element> lastClonedElement, firstClonedElement;
  for (RefPtr<Element> elementInPreviousBlock = deepestVisibleEditableElement;
       elementInPreviousBlock && elementInPreviousBlock != &aPreviousBlock;
       elementInPreviousBlock = elementInPreviousBlock->GetParentElement()) {
    if (!HTMLEditUtils::IsInlineStyle(elementInPreviousBlock) &&
        !elementInPreviousBlock->IsHTMLElement(nsGkAtoms::span)) {
      continue;
    }
    OwningNonNull<nsAtom> tagName =
        *elementInPreviousBlock->NodeInfo()->NameAtom();
    // At first time, just create the most descendant inline container
    // element.
    if (!firstClonedElement) {
      CreateElementResult createNewElementResult = CreateAndInsertElement(
          WithTransaction::Yes, tagName, EditorDOMPoint(&aNewBlock, 0u),
          // MOZ_CAN_RUN_SCRIPT_BOUNDARY due to bug 1758868
          [&elementInPreviousBlock](HTMLEditor& aHTMLEditor,
                                    Element& aNewElement, const EditorDOMPoint&)
              MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                // Clone all attributes.  Note that despite the method name,
                // CloneAttributesWithTransaction does not create
                // transactions in this case because aNewElement has not
                // been connected yet.
                // XXX Looks like that this clones id attribute too.
                aHTMLEditor.CloneAttributesWithTransaction(
                    aNewElement, *elementInPreviousBlock);
                return NS_OK;
              });
      if (createNewElementResult.isErr()) {
        NS_WARNING(
            "HTMLEditor::CreateAndInsertElement(WithTransaction::Yes) failed");
        return Err(createNewElementResult.unwrapErr());
      }
      // We'll return with a point suggesting new caret position and the
      // following path does not require an update of selection here.
      // Therefore, we don't need to update selection here.
      createNewElementResult.IgnoreCaretPointSuggestion();
      firstClonedElement = lastClonedElement =
          createNewElementResult.UnwrapNewNode();
      continue;
    }
    // Otherwise, inserts new parent inline container to the previous inserted
    // inline container.
    lastClonedElement =
        InsertContainerWithTransaction(*lastClonedElement, tagName);
    if (MOZ_UNLIKELY(!lastClonedElement)) {
      NS_WARNING("HTMLEditor::InsertContainerWithTransaction() failed");
      return Err(NS_ERROR_FAILURE);
    }
    CloneAttributesWithTransaction(*lastClonedElement, *elementInPreviousBlock);
    if (NS_WARN_IF(Destroyed())) {
      return Err(NS_ERROR_EDITOR_DESTROYED);
    }
  }

  if (!firstClonedElement) {
    // XXX Even if no inline elements are cloned, shouldn't we create new
    //     <br> element for aNewBlock?
    return EditorDOMPoint(&aNewBlock, 0u);
  }

  CreateElementResult insertBRElementResult = InsertBRElement(
      WithTransaction::Yes, EditorDOMPoint(firstClonedElement, 0u));
  if (insertBRElementResult.isErr()) {
    NS_WARNING("HTMLEditor::InsertBRElement(WithTransaction::Yes) failed");
    return Err(insertBRElementResult.unwrapErr());
  }
  insertBRElementResult.IgnoreCaretPointSuggestion();
  MOZ_ASSERT(insertBRElementResult.GetNewNode());
  return EditorDOMPoint(insertBRElementResult.GetNewNode());
}

nsresult HTMLEditor::GetElementOrigin(Element& aElement, int32_t& aX,
                                      int32_t& aY) {
  aX = 0;
  aY = 0;

  if (NS_WARN_IF(!IsInitialized())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  PresShell* presShell = GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsIFrame* frame = aElement.GetPrimaryFrame();
  if (NS_WARN_IF(!frame)) {
    return NS_OK;
  }

  nsIFrame* absoluteContainerBlockFrame =
      presShell->GetAbsoluteContainingBlock(frame);
  if (NS_WARN_IF(!absoluteContainerBlockFrame)) {
    return NS_OK;
  }
  nsPoint off = frame->GetOffsetTo(absoluteContainerBlockFrame);
  aX = nsPresContext::AppUnitsToIntCSSPixels(off.x);
  aY = nsPresContext::AppUnitsToIntCSSPixels(off.y);

  return NS_OK;
}

Element* HTMLEditor::GetSelectionContainerElement() const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsINode* focusNode = nullptr;
  if (SelectionRef().IsCollapsed()) {
    focusNode = SelectionRef().GetFocusNode();
    if (NS_WARN_IF(!focusNode)) {
      return nullptr;
    }
  } else {
    const uint32_t rangeCount = SelectionRef().RangeCount();
    MOZ_ASSERT(rangeCount, "If 0, Selection::IsCollapsed() should return true");

    if (rangeCount == 1) {
      const nsRange* range = SelectionRef().GetRangeAt(0);

      const RangeBoundary& startRef = range->StartRef();
      const RangeBoundary& endRef = range->EndRef();

      // This method called GetSelectedElement() to retrieve proper container
      // when only one node is selected.  However, it simply returns start
      // node of Selection with additional cost.  So, we do not need to call
      // it anymore.
      if (startRef.Container()->IsElement() &&
          startRef.Container() == endRef.Container() &&
          startRef.GetChildAtOffset() &&
          startRef.GetChildAtOffset()->GetNextSibling() ==
              endRef.GetChildAtOffset()) {
        focusNode = startRef.GetChildAtOffset();
        MOZ_ASSERT(focusNode, "Start container must not be nullptr");
      } else {
        focusNode = range->GetClosestCommonInclusiveAncestor();
        if (!focusNode) {
          NS_WARNING(
              "AbstractRange::GetClosestCommonInclusiveAncestor() returned "
              "nullptr");
          return nullptr;
        }
      }
    } else {
      for (const uint32_t i : IntegerRange(rangeCount)) {
        MOZ_ASSERT(SelectionRef().RangeCount() == rangeCount);
        const nsRange* range = SelectionRef().GetRangeAt(i);
        MOZ_ASSERT(range);
        nsINode* startContainer = range->GetStartContainer();
        if (!focusNode) {
          focusNode = startContainer;
        } else if (focusNode != startContainer) {
          // XXX Looks odd to use parent of startContainer because previous
          //     range may not be in the parent node of current
          //     startContainer.
          focusNode = startContainer->GetParentNode();
          // XXX Looks odd to break the for-loop here because we refer only
          //     first range and another range which starts from different
          //     container, and the latter range is preferred. Why?
          break;
        }
      }
      if (!focusNode) {
        NS_WARNING("Focused node of selection was not found");
        return nullptr;
      }
    }
  }

  if (focusNode->IsText()) {
    focusNode = focusNode->GetParentNode();
    if (NS_WARN_IF(!focusNode)) {
      return nullptr;
    }
  }

  if (NS_WARN_IF(!focusNode->IsElement())) {
    return nullptr;
  }
  return focusNode->AsElement();
}

NS_IMETHODIMP HTMLEditor::IsAnonymousElement(Element* aElement, bool* aReturn) {
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aReturn = aElement->IsRootOfNativeAnonymousSubtree();
  return NS_OK;
}

nsresult HTMLEditor::SetReturnInParagraphCreatesNewParagraph(
    bool aCreatesNewParagraph) {
  mCRInParagraphCreatesParagraph = aCreatesNewParagraph;
  return NS_OK;
}

bool HTMLEditor::GetReturnInParagraphCreatesNewParagraph() const {
  return mCRInParagraphCreatesParagraph;
}

nsresult HTMLEditor::GetReturnInParagraphCreatesNewParagraph(
    bool* aCreatesNewParagraph) {
  *aCreatesNewParagraph = mCRInParagraphCreatesParagraph;
  return NS_OK;
}

Element* HTMLEditor::GetFocusedElement() const {
  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (NS_WARN_IF(!focusManager)) {
    return nullptr;
  }

  Element* const focusedElement = focusManager->GetFocusedElement();

  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  const bool inDesignMode = IsInDesignMode();
  if (!focusedElement) {
    // in designMode, nobody gets focus in most cases.
    if (inDesignMode && OurWindowHasFocus()) {
      return document->GetRootElement();
    }
    return nullptr;
  }

  if (inDesignMode) {
    return OurWindowHasFocus() &&
                   focusedElement->IsInclusiveDescendantOf(document)
               ? focusedElement
               : nullptr;
  }

  // We're HTML editor for contenteditable

  // If the focused content isn't editable, or it has independent selection,
  // we don't have focus.
  if (!focusedElement->HasFlag(NODE_IS_EDITABLE) ||
      focusedElement->HasIndependentSelection()) {
    return nullptr;
  }
  // If our window is focused, we're focused.
  return OurWindowHasFocus() ? focusedElement : nullptr;
}

bool HTMLEditor::IsActiveInDOMWindow() const {
  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (NS_WARN_IF(!focusManager)) {
    return false;
  }

  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return false;
  }
  const bool inDesignMode = IsInDesignMode();

  // If we're in designMode, we're always active in the DOM window.
  if (inDesignMode) {
    return true;
  }

  nsPIDOMWindowOuter* ourWindow = document->GetWindow();
  nsCOMPtr<nsPIDOMWindowOuter> win;
  nsIContent* content = nsFocusManager::GetFocusedDescendant(
      ourWindow, nsFocusManager::eOnlyCurrentWindow, getter_AddRefs(win));
  if (!content) {
    return false;
  }

  // We're HTML editor for contenteditable

  // If the active content isn't editable, or it has independent selection,
  // we're not active).
  if (!content->HasFlag(NODE_IS_EDITABLE) ||
      content->HasIndependentSelection()) {
    return false;
  }
  return true;
}

Element* HTMLEditor::ComputeEditingHostInternal(
    const nsIContent* aContent, LimitInBodyElement aLimitInBodyElement) const {
  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }

  auto MaybeLimitInBodyElement =
      [&](const Element* aCandidiateEditingHost) -> Element* {
    if (!aCandidiateEditingHost) {
      return nullptr;
    }
    if (aLimitInBodyElement != LimitInBodyElement::Yes) {
      return const_cast<Element*>(aCandidiateEditingHost);
    }
    // By default, we should limit editing host to the <body> element for
    // avoiding deleting or creating unexpected elements outside the <body>.
    // However, this is incompatible with Chrome so that we should stop
    // doing this with adding safety checks more.
    if (document->GetBodyElement() &&
        nsContentUtils::ContentIsFlattenedTreeDescendantOf(
            aCandidiateEditingHost, document->GetBodyElement())) {
      return const_cast<Element*>(aCandidiateEditingHost);
    }
    // XXX If aContent is an editing host and has no parent node, we reach here,
    //     but returing the <body> which is not connected to aContent is odd.
    return document->GetBodyElement();
  };

  if (IsInDesignMode()) {
    // TODO: In this case, we need to compute editing host from aContent or the
    //       focus node of selection, and it may be in an editing host in a
    //       shadow DOM tree etc.  We need to do more complicated things.
    //       See also InDesignMode().
    return document->GetBodyElement();
  }

  // We're HTML editor for contenteditable
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return nullptr;
  }

  const nsIContent* const content =
      aContent ? aContent
               : nsIContent::FromNodeOrNull(SelectionRef().GetFocusNode());
  if (NS_WARN_IF(!content)) {
    return nullptr;
  }

  // If the active content isn't editable, we're not active.
  if (!content->HasFlag(NODE_IS_EDITABLE)) {
    return nullptr;
  }

  // Although the content shouldn't be in a native anonymous subtree, but
  // perhaps due to a bug of Selection or Range API, it may occur.  HTMLEditor
  // shouldn't touch native anonymous subtree so that return nullptr in such
  // case.
  if (MOZ_UNLIKELY(content->IsInNativeAnonymousSubtree())) {
    return nullptr;
  }

  // Note that `Selection` can be in <input> or <textarea>.  In the case, we
  // need to look for an ancestor which does not have editable parent.
  return MaybeLimitInBodyElement(
      const_cast<nsIContent*>(content)->GetEditingHost());
}

void HTMLEditor::NotifyEditingHostMaybeChanged() {
  // Note that even if the document is in design mode, a contenteditable element
  // in a shadow tree is focusable.   Therefore, we may need to update editing
  // host even when the document is in design mode.
  if (MOZ_UNLIKELY(NS_WARN_IF(!GetDocument()))) {
    return;
  }

  // We're HTML editor for contenteditable
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  // Get selection ancestor limit which may be old editing host.
  nsIContent* ancestorLimiter = SelectionRef().GetAncestorLimiter();
  if (!ancestorLimiter) {
    // If we've not initialized selection ancestor limit, we should wait focus
    // event to set proper limiter.
    return;
  }

  // Compute current editing host.
  nsIContent* editingHost = ComputeEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return;
  }

  // Update selection ancestor limit if current editing host includes the
  // previous editing host.
  // Additionally, the editing host may be an element in shadow DOM and the
  // shadow host is in designMode.  In this case, we need to set the editing
  // host as the new selection limiter.
  if (ancestorLimiter->IsInclusiveDescendantOf(editingHost) ||
      (ancestorLimiter->IsInDesignMode() != editingHost->IsInDesignMode())) {
    // Note that don't call HTMLEditor::InitializeSelectionAncestorLimit()
    // here because it may collapse selection to the first editable node.
    EditorBase::InitializeSelectionAncestorLimit(*editingHost);
  }
}

EventTarget* HTMLEditor::GetDOMEventTarget() const {
  // Don't use getDocument here, because we have no way of knowing
  // whether Init() was ever called.  So we need to get the document
  // ourselves, if it exists.
  MOZ_ASSERT(IsInitialized(), "The HTMLEditor has not been initialized yet");
  return GetDocument();
}

bool HTMLEditor::ShouldReplaceRootElement() const {
  if (!mRootElement) {
    // If we don't know what is our root element, we should find our root.
    return true;
  }

  // If we temporary set document root element to mRootElement, but there is
  // body element now, we should replace the root element by the body element.
  return mRootElement != GetBodyElement();
}

void HTMLEditor::NotifyRootChanged() {
  MOZ_ASSERT(mPendingRootElementUpdatedRunner,
             "HTMLEditor::NotifyRootChanged() should be called via a runner");
  mPendingRootElementUpdatedRunner = nullptr;

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  RemoveEventListeners();
  nsresult rv = InstallEventListeners();
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::InstallEventListeners() failed, but ignored");
    return;
  }

  UpdateRootElement();
  if (!mRootElement) {
    return;
  }

  rv = MaybeCollapseSelectionAtFirstEditableNode(false);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::MaybeCollapseSelectionAtFirstEditableNode(false) "
        "failed, "
        "but ignored");
    return;
  }

  // When this editor has focus, we need to reset the selection limiter to
  // new root.  Otherwise, that is going to be done when this gets focus.
  nsCOMPtr<nsINode> node = GetFocusedNode();
  if (node) {
    DebugOnly<nsresult> rvIgnored = InitializeSelection(*node);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "EditorBase::InitializeSelection() failed, but ignored");
  }

  SyncRealTimeSpell();
}

Element* HTMLEditor::GetBodyElement() const {
  MOZ_ASSERT(IsInitialized(), "The HTMLEditor hasn't been initialized yet");
  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  return document->GetBody();
}

nsINode* HTMLEditor::GetFocusedNode() const {
  Element* focusedElement = GetFocusedElement();
  if (!focusedElement) {
    return nullptr;
  }

  // focusedElement might be non-null even focusManager->GetFocusedElement()
  // is null.  That's the designMode case, and in that case our
  // FocusedContent() returns the root element, but we want to return
  // the document.

  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  NS_ASSERTION(focusManager, "Focus manager is null");
  if ((focusedElement = focusManager->GetFocusedElement())) {
    return focusedElement;
  }

  return GetDocument();
}

bool HTMLEditor::OurWindowHasFocus() const {
  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (NS_WARN_IF(!focusManager)) {
    return false;
  }
  nsPIDOMWindowOuter* focusedWindow = focusManager->GetFocusedWindow();
  if (!focusedWindow) {
    return false;
  }
  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return false;
  }
  nsPIDOMWindowOuter* ourWindow = document->GetWindow();
  return ourWindow == focusedWindow;
}

bool HTMLEditor::IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent) const {
  if (!EditorBase::IsAcceptableInputEvent(aGUIEvent)) {
    return false;
  }

  // While there is composition, all composition events in its top level
  // window are always fired on the composing editor.  Therefore, if this
  // editor has composition, the composition events should be handled in this
  // editor.
  if (mComposition && aGUIEvent->AsCompositionEvent()) {
    return true;
  }

  nsCOMPtr<nsINode> eventTargetNode =
      nsINode::FromEventTargetOrNull(aGUIEvent->GetOriginalDOMEventTarget());
  if (NS_WARN_IF(!eventTargetNode)) {
    return false;
  }

  if (eventTargetNode->IsContent()) {
    eventTargetNode =
        eventTargetNode->AsContent()->FindFirstNonChromeOnlyAccessContent();
    if (NS_WARN_IF(!eventTargetNode)) {
      return false;
    }
  }

  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return false;
  }

  if (IsInDesignMode()) {
    // If this editor is in designMode and the event target is the document,
    // the event is for this editor.
    if (eventTargetNode->IsDocument()) {
      return eventTargetNode == document;
    }
    // Otherwise, check whether the event target is in this document or not.
    if (NS_WARN_IF(!eventTargetNode->IsContent())) {
      return false;
    }
    if (document == eventTargetNode->GetUncomposedDoc()) {
      return true;
    }
    // If the event target is in a shadow tree, the content is not editable
    // by default, but if the focused content is an editing host, we need to
    // handle it as contenteditable mode.
    if (!eventTargetNode->IsInShadowTree()) {
      return false;
    }
  }

  // This HTML editor is for contenteditable.  We need to check the validity
  // of the target.
  if (NS_WARN_IF(!eventTargetNode->IsContent())) {
    return false;
  }

  // If the event is a mouse event, we need to check if the target content is
  // the focused editing host or its descendant.
  if (aGUIEvent->AsMouseEventBase()) {
    nsIContent* editingHost = ComputeEditingHost();
    // If there is no active editing host, we cannot handle the mouse event
    // correctly.
    if (!editingHost) {
      return false;
    }
    // If clicked on non-editable root element but the body element is the
    // active editing host, we should assume that the click event is
    // targetted.
    if (eventTargetNode == document->GetRootElement() &&
        !eventTargetNode->HasFlag(NODE_IS_EDITABLE) &&
        editingHost == document->GetBodyElement()) {
      eventTargetNode = editingHost;
    }
    // If the target element is neither the active editing host nor a
    // descendant of it, we may not be able to handle the event.
    if (!eventTargetNode->IsInclusiveDescendantOf(editingHost)) {
      return false;
    }
    // If the clicked element has an independent selection, we shouldn't
    // handle this click event.
    if (eventTargetNode->AsContent()->HasIndependentSelection()) {
      return false;
    }
    // If the target content is editable, we should handle this event.
    return eventTargetNode->HasFlag(NODE_IS_EDITABLE);
  }

  // If the target of the other events which target focused element isn't
  // editable or has an independent selection, this editor shouldn't handle
  // the event.
  if (!eventTargetNode->HasFlag(NODE_IS_EDITABLE) ||
      eventTargetNode->AsContent()->HasIndependentSelection()) {
    return false;
  }

  // Finally, check whether we're actually focused or not.  When we're not
  // focused, we should ignore the dispatched event by script (or something)
  // because content editable element needs selection in itself for editing.
  // However, when we're not focused, it's not guaranteed.
  return IsActiveInDOMWindow();
}

nsresult HTMLEditor::GetPreferredIMEState(IMEState* aState) {
  // HTML editor don't prefer the CSS ime-mode because IE didn't do so too.
  aState->mOpen = IMEState::DONT_CHANGE_OPEN_STATE;
  if (IsReadonly()) {
    aState->mEnabled = IMEEnabled::Disabled;
  } else {
    aState->mEnabled = IMEEnabled::Enabled;
  }
  return NS_OK;
}

already_AddRefed<Element> HTMLEditor::GetInputEventTargetElement() const {
  RefPtr<Element> target = ComputeEditingHost(LimitInBodyElement::No);
  if (target) {
    return target.forget();
  }

  // When there is no active editing host due to focus node is a
  // non-editable node, we should look for its editable parent to
  // dispatch `beforeinput` event.
  nsIContent* focusContent =
      nsIContent::FromNodeOrNull(SelectionRef().GetFocusNode());
  if (!focusContent || focusContent->IsEditable()) {
    return nullptr;
  }
  for (Element* element : focusContent->AncestorsOfType<Element>()) {
    if (element->IsEditable()) {
      target = element->GetEditingHost();
      return target.forget();
    }
  }
  return nullptr;
}

nsresult HTMLEditor::OnModifyDocument() {
  MOZ_ASSERT(mPendingDocumentModifiedRunner,
             "HTMLEditor::OnModifyDocument() should be called via a runner");
  mPendingDocumentModifiedRunner = nullptr;

  if (IsEditActionDataAvailable()) {
    return OnModifyDocumentInternal();
  }

  AutoEditActionDataSetter editActionData(
      *this, EditAction::eCreatePaddingBRElementForEmptyEditor);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = OnModifyDocumentInternal();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::OnModifyDocumentInternal() failed");
  return rv;
}

nsresult HTMLEditor::OnModifyDocumentInternal() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!mPendingDocumentModifiedRunner);

  // EnsureNoPaddingBRElementForEmptyEditor() below may cause a flush, which
  // could destroy the editor
  nsAutoScriptBlockerSuppressNodeRemoved scriptBlocker;

  // Delete our padding <br> element for empty editor, if we have one, since
  // the document might not be empty any more.
  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return rv;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  // Try to recreate the padding <br> element for empty editor if needed.
  rv = MaybeCreatePaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::MaybeCreatePaddingBRElementForEmptyEditor() failed");

  return rv;
}

}  // namespace mozilla
