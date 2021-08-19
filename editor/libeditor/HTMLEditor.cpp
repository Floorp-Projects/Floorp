/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditor.h"

#include "HTMLEditorEventListener.h"
#include "HTMLEditUtils.h"
#include "JoinNodeTransaction.h"
#include "ReplaceTextTransaction.h"
#include "SplitNodeTransaction.h"
#include "TypeInState.h"
#include "WSRunObject.h"

#include "mozilla/ComposerCommandsUpdater.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/Encoding.h"  // for Encoding
#include "mozilla/EventStates.h"
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
#include "nsIEditActionListener.h"
#include "nsIFrame.h"
#include "nsIPrincipal.h"
#include "nsISelectionController.h"
#include "nsIURI.h"
#include "nsIWidget.h"
#include "nsNetUtil.h"
#include "nsPresContext.h"
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

const char16_t kNBSP = 160;

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

HTMLEditor::HTMLEditor()
    : mCRInParagraphCreatesParagraph(false),
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
          Preferences::GetBool("editor.use_div_for_default_newlines", true)
              ? ParagraphSeparator::div
              : ParagraphSeparator::br) {
  mIsHTMLEditorClass = true;
}

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

nsresult HTMLEditor::Init(Document& aDocument, uint32_t aFlags) {
  MOZ_ASSERT(!mInitSucceeded,
             "HTMLEditor::Init() called again without calling PreDestroy()?");

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
  EnableUndoRedo();
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
  Result<RefPtr<Element>, nsresult> maybeNewMetaElement =
      CreateNodeWithTransaction(*nsGkAtoms::meta,
                                EditorDOMPoint(primaryHeadElement, 0));
  if (maybeNewMetaElement.isErr()) {
    NS_WARNING(
        "EditorBase::CreateNodeWithTransaction(nsGkAtoms::meta) failed, but "
        "ignored");
    return NS_OK;
  }
  MOZ_ASSERT(maybeNewMetaElement.inspect());

  // not undoable, undo should undo CreateNodeWithTransaction().
  DebugOnly<nsresult> rvIgnored = NS_OK;
  rvIgnored = maybeNewMetaElement.inspect()->SetAttr(
      kNameSpaceID_None, nsGkAtoms::httpEquiv, u"Content-Type"_ns, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "Element::SetAttr(nsGkAtoms::httpEquiv, Content-Type) "
                       "failed, but ignored");
  rvIgnored = maybeNewMetaElement.inspect()->SetAttr(
      kNameSpaceID_None, nsGkAtoms::content,
      u"text/html;charset="_ns + NS_ConvertASCIItoUTF16(aCharacterSet), true);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "Element::SetAttr(nsGkAtoms::content) failed, but ignored");
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
                                                 int16_t aReason) {
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

  nsresult rv =
      EditorBase::NotifySelectionChanged(aDocument, aSelection, aReason);
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

Element* HTMLEditor::FindSelectionRoot(nsINode* aNode) const {
  if (NS_WARN_IF(!aNode)) {
    return nullptr;
  }

  MOZ_ASSERT(aNode->IsDocument() || aNode->IsContent(),
             "aNode must be content or document node");

  Document* document = aNode->GetComposedDoc();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }

  if (aNode->IsInUncomposedDoc() &&
      (document->HasFlag(NODE_IS_EDITABLE) || !aNode->IsContent())) {
    return document->GetRootElement();
  }

  // XXX If we have readonly flag, shouldn't return the element which has
  // contenteditable="true"?  However, such case isn't there without chrome
  // permission script.
  if (IsReadonly()) {
    // We still want to allow selection in a readonly editor.
    return GetRoot();
  }

  nsIContent* content = aNode->AsContent();
  if (!content->HasFlag(NODE_IS_EDITABLE)) {
    // If the content is in read-write state but is not editable itself,
    // return it as the selection root.
    if (content->IsElement() &&
        content->AsElement()->State().HasState(NS_EVENT_STATE_READWRITE)) {
      return content->AsElement();
    }
    return nullptr;
  }

  // For non-readonly editors we want to find the root of the editable subtree
  // containing aContent.
  return content->GetEditingHost();
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
    Element* editingHost = GetActiveEditingHost();
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

  RefPtr<Element> editingHost = GetActiveEditingHost(LimitInBodyElement::No);
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
                             "HTMLEditor::CollapseSelectionTo() failed");
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
      if (scanResultInTextNode.InNormalWhiteSpacesOrText() &&
          scanResultInTextNode.TextPtr() == text) {
        nsresult rv = CollapseSelectionTo(scanResultInTextNode.Point());
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "HTMLEditor::CollapseSelectionTo() failed");
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
                             "HTMLEditor::CollapseSelectionTo() failed");
        return rv;
      }
      MOZ_ASSERT_UNREACHABLE(
          "How do we reach editable leaf in non-editable element?");
      // But if it's not editable, let's put caret at start of editing host
      // for now.
      nsresult rv = CollapseSelectionTo(EditorDOMPoint(editingHost, 0));
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::CollapseSelectionTo() failed");
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
                       "HTMLEditor::CollapseSelectionTo() failed");
  return rv;
}

bool HTMLEditor::ArePreservingSelection() const {
  return IsEditActionDataAvailable() && !SavedSelectionRef().IsEmpty();
}

void HTMLEditor::PreserveSelectionAcrossActions() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  SavedSelectionRef().SaveSelection(SelectionRef());
  RangeUpdaterRef().RegisterSelectionState(SavedSelectionRef());
}

nsresult HTMLEditor::RestorePreservedSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (SavedSelectionRef().IsEmpty()) {
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
  SavedSelectionRef().Clear();
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

  EditActionResult result = InsertParagraphSeparatorAsSubAction();
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

  // XXX This method may be called by "insertLineBreak" command.  So, using
  //     TypingTxnName here is odd in such case.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName,
                                             ScrollSelectionIntoView::Yes);
  rv = InsertBRElementAtSelectionWithTransaction();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::InsertBRElementAtSelectionWithTransaction() failed");
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

  EditActionResult result = InsertParagraphSeparatorAsSubAction();
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
  Element* cellElement =
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
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditActionHandled(rv);
  }
  rv = InsertTableRowsWithTransaction(1, InsertPosition::eAfterSelectedCell);
  if (NS_WARN_IF(Destroyed())) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::InsertTableRowsWithTransaction(1, "
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
    DebugOnly<nsresult> rvIgnored = CollapseSelectionToStartOf(*cell);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::CollapseSelectionToStartOf() failed, but ignored");
  }
  return EditActionHandled(NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED
                                                   : NS_OK);
}

nsresult HTMLEditor::InsertBRElementAtSelectionWithTransaction() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  // calling it text insertion to trigger moz br treatment by rules
  // XXX Why do we use EditSubAction::eInsertText here?  Looks like
  //     EditSubAction::eInsertLineBreak or EditSubAction::eInsertNode
  //     is better.
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  if (!SelectionRef().IsCollapsed()) {
    nsresult rv = DeleteSelectionAsSubAction(eNone, eStrip);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "EditorBase::DeleteSelectionAsSubAction(eNone, eStrip) failed");
      return rv;
    }
  }

  EditorDOMPoint atStartOfSelection(EditorBase::GetStartPoint(SelectionRef()));
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // InsertBRElementWithTransaction() will set selection after the new <br>
  // element.
  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      InsertBRElementWithTransaction(atStartOfSelection, eNext);
  if (resultOfInsertingBRElement.isErr()) {
    NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
    return resultOfInsertingBRElement.unwrapErr();
  }
  MOZ_ASSERT(resultOfInsertingBRElement.inspect());
  return NS_OK;
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
      "HTMLEditor::CollapseSelectionToStartOf() failed, but ignored");
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

  // Do not use AutoTopLevelEditSubActionNotifier -- rules code won't let us
  // insert in <head>.  Use the head node as a parent and delete/insert
  // directly.
  // XXX We're using AutoTopLevelEditSubActionNotifier above...
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

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);

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
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }

  // Now insert the new nodes
  int32_t offsetOfNewNode = 0;

  // Loop over the contents of the fragment and move into the document
  while (nsCOMPtr<nsIContent> child = documentFragment->GetFirstChild()) {
    nsresult rv = InsertNodeWithTransaction(
        *child, EditorDOMPoint(primaryHeadElement, offsetOfNewNode++));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return rv;
    }
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
  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);

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

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
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
    nsresult rv = EnsureCaretNotAfterPaddingBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterPaddingBRElement() "
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

  Element* editingHost = GetActiveEditingHost(LimitInBodyElement::No);
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

  EditorDOMPoint insertedPoint = InsertNodeIntoProperAncestorWithTransaction(
      *aElement, pointToInsert, SplitAtEdges::eAllowToCreateEmptyContainer);
  if (!insertedPoint.IsSet()) {
    NS_WARNING(
        "HTMLEditor::InsertNodeIntoProperAncestorWithTransaction(SplitAtEdges::"
        "eAllowToCreateEmptyContainer) failed");
    return NS_ERROR_FAILURE;
  }
  // Set caret after element, but check for special case
  //  of inserting table-related elements: set in first cell instead
  if (!SetCaretInTableCell(aElement)) {
    if (NS_WARN_IF(Destroyed())) {
      return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    }
    nsresult rv = CollapseSelectionAfter(*aElement);
    if (NS_WARN_IF(Destroyed())) {
      return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::CollapseSelectionAfter() failed");
      return rv;
    }
  }

  if (NS_WARN_IF(Destroyed())) {
    return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
  }

  // check for inserting a whole table at the end of a block. If so insert
  // a br after it.
  if (!HTMLEditUtils::IsTable(aElement) ||
      !HTMLEditUtils::IsLastChild(*aElement,
                                  {WalkTreeOption::IgnoreNonEditableNode})) {
    return NS_OK;
  }

  DebugOnly<bool> advanced = insertedPoint.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced,
                       "Failed to advance offset from inserted point");
  // Collapse selection to the new `<br>` element node after creating it.
  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      InsertBRElementWithTransaction(insertedPoint, ePrevious);
  if (resultOfInsertingBRElement.isErr()) {
    NS_WARNING("HTMLEditor::InsertBRElementWithTransaction(ePrevious) failed");
    return EditorBase::ToGenericNSResult(
        resultOfInsertingBRElement.unwrapErr());
  }
  MOZ_ASSERT(resultOfInsertingBRElement.inspect());
  return NS_OK;
}

EditorDOMPoint HTMLEditor::InsertNodeIntoProperAncestorWithTransaction(
    nsIContent& aNode, const EditorDOMPoint& aPointToInsert,
    SplitAtEdges aSplitAtEdges) {
  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return EditorDOMPoint();
  }
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  // Search up the parent chain to find a suitable container.
  EditorDOMPoint pointToInsert(aPointToInsert);
  MOZ_ASSERT(pointToInsert.IsSet());
  while (!HTMLEditUtils::CanNodeContain(*pointToInsert.GetContainer(), aNode)) {
    // If the current parent is a root (body or table element)
    // then go no further - we can't insert.
    if (pointToInsert.IsContainerHTMLElement(nsGkAtoms::body) ||
        HTMLEditUtils::IsAnyTableElement(pointToInsert.GetContainer())) {
      return EditorDOMPoint();
    }

    // Get the next point.
    pointToInsert.Set(pointToInsert.GetContainer());
    if (NS_WARN_IF(!pointToInsert.IsSet())) {
      return EditorDOMPoint();
    }

    if (!pointToInsert.IsInContentNode() ||
        !EditorUtils::IsEditableContent(*pointToInsert.ContainerAsContent(),
                                        EditorType::HTML)) {
      // There's no suitable place to put the node in this editing host.
      return EditorDOMPoint();
    }
  }

  if (pointToInsert != aPointToInsert) {
    // We need to split some levels above the original selection parent.
    MOZ_ASSERT(pointToInsert.GetChild());
    SplitNodeResult splitNodeResult =
        SplitNodeDeepWithTransaction(MOZ_KnownLive(*pointToInsert.GetChild()),
                                     aPointToInsert, aSplitAtEdges);
    if (splitNodeResult.Failed()) {
      NS_WARNING("HTMLEditor::SplitNodeDeepWithTransaction() failed");
      return EditorDOMPoint();
    }
    pointToInsert = splitNodeResult.SplitPoint();
    MOZ_ASSERT(pointToInsert.IsSet());
  }

  {
    // After inserting a node, pointToInsert will refer next sibling of
    // the new node but keep referring the new node's offset.
    // This method's result should be the point at insertion, it's useful
    // even if the new node is moved by mutation observer immediately.
    // So, let's lock only the offset and child node should be recomputed
    // when it's necessary.
    AutoEditorDOMPointChildInvalidator lockOffset(pointToInsert);
    // Now we can insert the new node.
    nsresult rv = InsertNodeWithTransaction(aNode, pointToInsert);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return EditorDOMPoint();
    }
  }
  return pointToInsert;
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

  // Must be sure that element is contained in the document body
  if (NS_WARN_IF(!IsDescendantOfEditorRoot(&aContentToSelect))) {
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

nsresult HTMLEditor::CollapseSelectionAfter(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Be sure the element is contained in the document body
  if (NS_WARN_IF(!IsDescendantOfEditorRoot(&aElement))) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!aElement.GetParentNode())) {
    return NS_ERROR_FAILURE;
  }
  // Collapse selection to just after desired element,
  EditorRawDOMPoint afterElement(EditorRawDOMPoint::After(aElement));
  if (NS_WARN_IF(!afterElement.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = CollapseSelectionTo(afterElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed");
  return rv;
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

  ErrorResult error;
  RefPtr<Element> cellOrRowOrTableElement =
      GetSelectedOrParentTableElement(error);
  if (error.Failed()) {
    NS_WARNING(
        "HTMLEditor::GetSelectedOrParentTableElement() returned nullptr");
    return error.StealNSResult();
  }

  for (RefPtr<Element> element = std::move(cellOrRowOrTableElement); element;
       element = element->GetParentElement()) {
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

  rv = RemoveListAtSelectionAsSubAction();
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

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
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
    nsresult rv = EnsureCaretNotAfterPaddingBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterPaddingBRElement() "
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
  rv = FormatBlockContainerWithTransaction(aTagName);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::FormatBlockContainerWithTransaction() failed");
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

  EditActionResult result = IndentAsSubAction();
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

  EditActionResult result = OutdentAsSubAction();
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

  EditActionResult result = AlignAsSubAction(aAlignType);
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
    const nsStaticAtom& aTagName, nsIContent& aContent) const {
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

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);

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
  ErrorResult error;
  bool isCellSelected = false;
  RefPtr<Element> cellOrRowOrTableElement =
      GetSelectedOrParentTableElement(error, &isCellSelected);
  if (error.Failed()) {
    NS_WARNING("HTMLEditor::GetSelectedOrParentTableElement() failed");
    return error.StealNSResult();
  }

  bool setColor = !aColor.IsEmpty();
  RefPtr<Element> rootElementOfBackgroundColor;
  if (cellOrRowOrTableElement) {
    rootElementOfBackgroundColor = std::move(cellOrRowOrTableElement);
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
      !HTMLEditUtils::IsSimplyEditableNode(aContent) || !aContent.GetParent()) {
    return NS_OK;
  }

  // Don't strip wrappers if this is the only wrapper in the block.  Then we'll
  // add a <br> later, so it won't be an empty wrapper in the end.
  Element* blockElement =
      HTMLEditUtils::GetAncestorBlockElement(aContent, editingHost);
  if (!blockElement ||
      HTMLEditUtils::IsEmptyNode(
          *blockElement, {EmptyCheckOption::TreatSingleBRElementAsVisible})) {
    return NS_OK;
  }

  OwningNonNull<nsIContent> content = aContent;
  for (nsIContent* parentContent : aContent.AncestorsOfType<nsIContent>()) {
    if (HTMLEditUtils::IsBlockElement(*parentContent) ||
        parentContent->Length() != 1 ||
        !HTMLEditUtils::IsSimplyEditableNode(*parentContent) ||
        parentContent == editingHost) {
      break;
    }
    content = *parentContent;
  }

  nsresult rv = DeleteNodeWithTransaction(content);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::DeleteNodeWithTransaction() failed");
  return rv;
}

nsresult HTMLEditor::DeleteNodeWithTransaction(nsIContent& aContent) {
  // Do nothing if the node is read-only.
  // XXX This is not a override method of EditorBase's method.  This might
  //     cause not called accidentally.  We need to investigate this issue.
  if (NS_WARN_IF(!HTMLEditUtils::IsRemovableNode(aContent) &&
                 !EditorUtils::IsPaddingBRElementForEmptyEditor(aContent))) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = EditorBase::DeleteNodeWithTransaction(aContent);
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
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
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
                       "HTMLEditor::DeleteNodeWithTransaction() failed");
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
    nsresult rv = InsertTextWithTransaction(
        *document, aStringToInsert, EditorRawDOMPoint(&aTextNode, aOffset));
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::InsertTextWithTransaction() failed");
    return rv;
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
    for (uint32_t i = 0; i < SelectionRef().RangeCount(); i++) {
      const nsRange* range = SelectionRef().GetRangeAt(i);
      if (!range) {
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
    EditorDOMPointInText begin, end;
    Tie(begin, end) = ComputeInsertedRange(pointToInsert, aStringToInsert);
    if (begin.IsSet() && end.IsSet()) {
      TopLevelEditSubActionDataRef().DidDeleteText(*this, begin);
      TopLevelEditSubActionDataRef().DidInsertText(*this, begin, end);
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

nsresult HTMLEditor::InsertTextWithTransaction(
    Document& aDocument, const nsAString& aStringToInsert,
    const EditorRawDOMPoint& aPointToInsert,
    EditorRawDOMPoint* aPointAfterInsertedString) {
  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  // Do nothing if the node is read-only
  if (NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(
          *aPointToInsert.GetContainer()))) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = EditorBase::InsertTextWithTransaction(
      aDocument, aStringToInsert, aPointToInsert, aPointAfterInsertedString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertTextWithTransaction() failed");
  return rv;
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
  ErrorResult error;
  nsCOMPtr<nsIContent> newLeftNode =
      SplitNodeWithTransaction(aPointToInsert, error);
  if (error.Failed()) {
    NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
    return Err(error.StealNSResult());
  }
  Unused << newLeftNode;
  // Insert new <br> before the right node.
  EditorDOMPoint pointInContainer(aPointToInsert.GetContainer());
  if (!pointInContainer.IsSet()) {
    NS_WARNING("Failed to climb up the tree");
    return Err(NS_ERROR_FAILURE);
  }
  return pointInContainer;
}

Result<RefPtr<Element>, nsresult> HTMLEditor::InsertBRElementWithTransaction(
    const EditorDOMPoint& aPointToInsert, EDirection aSelect /* = eNone */) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  Result<EditorDOMPoint, nsresult> maybePointToInsert =
      PrepareToInsertBRElement(aPointToInsert);
  if (maybePointToInsert.isErr()) {
    NS_WARNING("HTMLEditor::PrepareToInsertBRElement() failed");
    return Err(maybePointToInsert.unwrapErr());
  }
  MOZ_ASSERT(maybePointToInsert.inspect().IsSetAndValid());

  Result<RefPtr<Element>, nsresult> maybeNewBRElement =
      CreateNodeWithTransaction(*nsGkAtoms::br, maybePointToInsert.inspect());
  if (maybeNewBRElement.isErr()) {
    NS_WARNING("EditorBase::CreateNodeWithTransaction() failed");
    return maybeNewBRElement;
  }
  MOZ_ASSERT(maybeNewBRElement.inspect());

  switch (aSelect) {
    case eNone:
      break;
    case eNext: {
      ErrorResult error;
      SelectionRef().SetInterlinePosition(true, error);
      if (error.Failed()) {
        NS_WARNING("Selection::SetInterlinePosition(true) failed");
        return Err(error.StealNSResult());
      }
      // Collapse selection after the <br> node.
      EditorRawDOMPoint afterBRElement(
          EditorRawDOMPoint::After(*maybeNewBRElement.inspect()));
      if (!afterBRElement.IsSet()) {
        NS_WARNING("Setting point to after <br> element failed");
        return Err(NS_ERROR_FAILURE);
      }
      CollapseSelectionTo(afterBRElement, error);
      if (error.Failed()) {
        NS_WARNING("HTMLEditor::CollapseSelectionTo() failed, but ignored");
        return Err(error.StealNSResult());
      }
      break;
    }
    case ePrevious: {
      ErrorResult error;
      SelectionRef().SetInterlinePosition(true, error);
      if (error.Failed()) {
        NS_WARNING("Selection::SetInterlinePosition(true) failed");
        return Err(error.StealNSResult());
      }
      // Collapse selection at the <br> node.
      EditorRawDOMPoint atBRElement(maybeNewBRElement.inspect());
      if (!atBRElement.IsSet()) {
        NS_WARNING("Setting point to at <br> element failed");
        return Err(NS_ERROR_FAILURE);
      }
      CollapseSelectionTo(atBRElement, error);
      if (error.Failed()) {
        NS_WARNING("HTMLEditor::CollapseSelectionTo() failed, but ignored");
        return Err(error.StealNSResult());
      }
      break;
    }
    default:
      NS_WARNING(
          "aSelect has invalid value, the caller need to set selection "
          "by itself");
      break;
  }

  return maybeNewBRElement;
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
  nsresult rv = EditorBase::DeleteNodeWithTransaction(aContent);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return nullptr;
  }

  {
    AutoTransactionsConserveSelection conserveSelection(*this);
    rv = InsertNodeWithTransaction(aContent, EditorDOMPoint(newContainer, 0));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return nullptr;
    }
  }

  // Put the new container where aNode was.
  rv = InsertNodeWithTransaction(*newContainer, pointToInsertNewContainer);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
    return nullptr;
  }

  return newContainer.forget();
}

already_AddRefed<Element> HTMLEditor::ReplaceContainerWithTransactionInternal(
    Element& aOldContainer, nsAtom& aTagName, nsAtom& aAttribute,
    const nsAString& aAttributeValue, bool aCloneAllAttributes) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  EditorDOMPoint atOldContainer(&aOldContainer);
  if (NS_WARN_IF(!atOldContainer.IsSet())) {
    return nullptr;
  }

  RefPtr<Element> newContainer = CreateHTMLContent(&aTagName);
  if (NS_WARN_IF(!newContainer)) {
    return nullptr;
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
      return nullptr;
    }
  }

  // Notify our internal selection state listener.
  // Note: An AutoSelectionRestorer object must be created before calling this
  // to initialize RangeUpdaterRef().
  AutoReplaceContainerSelNotify selStateNotify(RangeUpdaterRef(), aOldContainer,
                                               *newContainer);
  {
    AutoTransactionsConserveSelection conserveSelection(*this);
    // Move all children from the old container to the new container.
    while (aOldContainer.HasChildren()) {
      nsCOMPtr<nsIContent> child = aOldContainer.GetFirstChild();
      if (NS_WARN_IF(!child)) {
        return nullptr;
      }
      // HTMLEditor::DeleteNodeWithTransaction() does not move non-editable
      // node, but we need to move non-editable nodes too.  Therefore, call
      // EditorBase's method directly.
      nsresult rv = EditorBase::DeleteNodeWithTransaction(*child);
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
        return nullptr;
      }

      rv = InsertNodeWithTransaction(
          *child, EditorDOMPoint(newContainer, newContainer->Length()));
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
        return nullptr;
      }
    }
  }

  // Insert new container into tree.
  NS_WARNING_ASSERTION(atOldContainer.IsSetAndValid(),
                       "The old container might be moved by mutation observer");
  nsresult rv = InsertNodeWithTransaction(*newContainer, atOldContainer);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
    return nullptr;
  }

  // Delete old container.
  // XXX Perhaps, we should not remove the container if it's not editable.
  rv = EditorBase::DeleteNodeWithTransaction(aOldContainer);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return nullptr;
  }

  return newContainer.forget();
}

nsresult HTMLEditor::RemoveContainerWithTransaction(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  EditorDOMPoint pointToInsertChildren(&aElement);
  if (NS_WARN_IF(!pointToInsertChildren.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Notify our internal selection state listener.
  AutoRemoveContainerSelNotify selNotify(RangeUpdaterRef(),
                                         pointToInsertChildren);

  // Move all children from aNode to its parent.
  while (aElement.HasChildren()) {
    nsCOMPtr<nsIContent> child = aElement.GetLastChild();
    if (NS_WARN_IF(!child)) {
      return NS_ERROR_FAILURE;
    }
    // HTMLEditor::DeleteNodeWithTransaction() does not move non-editable
    // node, but we need to move non-editable nodes too.  Therefore, call
    // EditorBase's method directly.
    nsresult rv = EditorBase::DeleteNodeWithTransaction(*child);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return rv;
    }

    // Insert the last child before the previous last child.  So, we need to
    // use offset here because previous child might have been moved to
    // container.
    rv = InsertNodeWithTransaction(
        *child, EditorDOMPoint(pointToInsertChildren.GetContainer(),
                               pointToInsertChildren.Offset()));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return rv;
    }
  }

  // XXX Perhaps, we should not remove the container if it's not editable.
  nsresult rv = EditorBase::DeleteNodeWithTransaction(aElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::DeleteNodeWithTransaction() failed");
  return rv;
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
                         "HTMLEditor::CollapseSelectionToStartOf() failed");
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
      !HTMLEditUtils::IsAnyTableElement(aElement) ||
      !IsDescendantOfEditorRoot(aElement)) {
    return false;
  }

  nsCOMPtr<nsIContent> deepestFirstChild = aElement;
  while (deepestFirstChild->HasChildren()) {
    deepestFirstChild = deepestFirstChild->GetFirstChild();
  }

  // Set selection at beginning of the found node
  nsresult rv = CollapseSelectionToStartOf(*deepestFirstChild);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionToStartOf() failed");
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
  // NOTE: assumption that JoinNodes keeps the righthand node
  while (textNodes.Length() > 1) {
    // we assume a textNodes entry can't be nullptr
    Text* leftTextNode = textNodes[0];
    Text* rightTextNode = textNodes[1];
    NS_ASSERTION(leftTextNode && rightTextNode,
                 "left or rightTextNode null in CollapseAdjacentTextNodes");

    // get the prev sibling of the right node, and see if its leftTextNode
    nsIContent* previousSiblingOfRightTextNode =
        rightTextNode->GetPreviousSibling();
    if (previousSiblingOfRightTextNode &&
        previousSiblingOfRightTextNode == leftTextNode) {
      nsresult rv = JoinNodesWithTransaction(MOZ_KnownLive(*leftTextNode),
                                             MOZ_KnownLive(*rightTextNode));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::JoinNodesWithTransaction() failed");
        return rv;
      }
    }

    // remove the leftmost text node from the list
    textNodes.RemoveElementAt(0);
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
                       "HTMLEditor::CollapseSelectionToStartOf() failed");
  return rv;
}

/**
 * Remove aNode, reparenting any children into the parent of aNode.  In
 * addition, insert any br's needed to preserve identity of removed block.
 */
nsresult HTMLEditor::RemoveBlockContainerWithTransaction(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Two possibilities: the container could be empty of editable content.  If
  // that is the case, we need to compare what is before and after aNode to
  // determine if we need a br.
  //
  // Or it could be not empty, in which case we have to compare previous
  // sibling and first child to determine if we need a leading br, and compare
  // following sibling and last child to determine if we need a trailing br.

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
        Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
            InsertBRElementWithTransaction(EditorDOMPoint(&aElement, 0));
        if (resultOfInsertingBRElement.isErr()) {
          NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
          return resultOfInsertingBRElement.unwrapErr();
        }
        MOZ_ASSERT(resultOfInsertingBRElement.inspect());
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
            Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
                InsertBRElementWithTransaction(
                    EditorDOMPoint::AtEndOf(aElement));
            if (resultOfInsertingBRElement.isErr()) {
              NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
              return resultOfInsertingBRElement.unwrapErr();
            }
            MOZ_ASSERT(resultOfInsertingBRElement.inspect());
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
          Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
              InsertBRElementWithTransaction(EditorDOMPoint(&aElement, 0));
          if (resultOfInsertingBRElement.isErr()) {
            NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
            return resultOfInsertingBRElement.unwrapErr();
          }
          MOZ_ASSERT(resultOfInsertingBRElement.inspect());
        }
      }
    }
  }

  // Now remove container
  nsresult rv = RemoveContainerWithTransaction(aElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::RemoveContainerWithTransaction() failed");
  return rv;
}

already_AddRefed<nsIContent> HTMLEditor::SplitNodeWithTransaction(
    const EditorDOMPoint& aStartOfRightNode, ErrorResult& aError) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aStartOfRightNode.IsInContentNode())) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }
  MOZ_ASSERT(aStartOfRightNode.IsSetAndValid());

  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eSplitNode, nsIEditor::eNext, aError);
  if (NS_WARN_IF(aError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return nullptr;
  }
  NS_WARNING_ASSERTION(
      !aError.Failed(),
      "OnStartToHandleTopLevelEditSubAction() failed, but ignored");
  aError.SuppressException();

  // XXX Unfortunately, storing offset of the split point in
  //     SplitNodeTransaction is necessary for now.  We should fix this
  //     in a follow up bug.
  Unused << aStartOfRightNode.Offset();

  RefPtr<SplitNodeTransaction> transaction =
      SplitNodeTransaction::Create(*this, aStartOfRightNode);
  aError = DoTransactionInternal(transaction);
  NS_WARNING_ASSERTION(!aError.Failed(),
                       "EditorBase::DoTransactionInternal() failed");

  nsCOMPtr<nsIContent> newLeftContent = transaction->GetNewLeftContent();
  NS_WARNING_ASSERTION(newLeftContent, "Failed to create a new left node");

  if (newLeftContent) {
    // XXX Some other transactions manage range updater by themselves.
    //     Why doesn't SplitNodeTransaction do it?
    DebugOnly<nsresult> rvIgnored = RangeUpdaterRef().SelAdjSplitNode(
        *aStartOfRightNode.GetContainerAsContent(), *newLeftContent);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "RangeUpdater::SelAdjSplitNode() failed, but ignored");
  }
  if (newLeftContent) {
    TopLevelEditSubActionDataRef().DidSplitContent(
        *this, *aStartOfRightNode.GetContainerAsContent(), *newLeftContent);
  }

  if (mInlineSpellChecker) {
    RefPtr<mozInlineSpellChecker> spellChecker = mInlineSpellChecker;
    spellChecker->DidSplitNode(aStartOfRightNode.GetContainer(),
                               newLeftContent);
  }

  if (aError.Failed()) {
    return nullptr;
  }

  return newLeftContent.forget();
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
  SplitNodeResult lastSplitNodeResult(atStartOfRightNode);
  while (true) {
    // Need to insert rules code call here to do things like not split a list
    // if you are after the last <li> or before the first, etc.  For now we
    // just have some smarts about unneccessarily splitting text nodes, which
    // should be universal enough to put straight in this EditorBase routine.
    nsIContent* currentRightNode = atStartOfRightNode.GetContainerAsContent();
    if (NS_WARN_IF(!currentRightNode)) {
      return SplitNodeResult(NS_ERROR_FAILURE);
    }
    // If we meet an orphan node before meeting aMostAncestorToSplit, we need
    // to stop splitting.  This is a bug of the caller.
    if (NS_WARN_IF(currentRightNode != &aMostAncestorToSplit &&
                   !atStartOfRightNode.GetContainerParentAsContent())) {
      return SplitNodeResult(NS_ERROR_FAILURE);
    }
    // If the container is not splitable node such as comment node, atomic
    // element, etc, we should keep it as-is, and try to split its parents.
    if (!HTMLEditUtils::IsSplittableNode(*currentRightNode)) {
      if (currentRightNode == &aMostAncestorToSplit) {
        return lastSplitNodeResult;
      }
      atStartOfRightNode.Set(currentRightNode);
      continue;
    }

    // If the split point is middle of the node or the node is not a text node
    // and we're allowed to create empty element node, split it.
    if ((aSplitAtEdges == SplitAtEdges::eAllowToCreateEmptyContainer &&
         !atStartOfRightNode.GetContainerAsText()) ||
        (!atStartOfRightNode.IsStartOfContainer() &&
         !atStartOfRightNode.IsEndOfContainer())) {
      ErrorResult error;
      nsCOMPtr<nsIContent> newLeftNode =
          SplitNodeWithTransaction(atStartOfRightNode, error);
      if (error.Failed()) {
        NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
        return SplitNodeResult(error.StealNSResult());
      }

      lastSplitNodeResult = SplitNodeResult(newLeftNode, currentRightNode);
      if (currentRightNode == &aMostAncestorToSplit) {
        // Actually, we split aMostAncestorToSplit.
        return lastSplitNodeResult;
      }

      // Then, try to split its parent before current node.
      atStartOfRightNode.Set(currentRightNode);
    }
    // If the split point is end of the node and it is a text node or we're not
    // allowed to create empty container node, try to split its parent after it.
    else if (!atStartOfRightNode.IsStartOfContainer()) {
      lastSplitNodeResult = SplitNodeResult(currentRightNode, nullptr);
      if (currentRightNode == &aMostAncestorToSplit) {
        return lastSplitNodeResult;
      }

      // Try to split its parent after current node.
      atStartOfRightNode.Set(currentRightNode);
      DebugOnly<bool> advanced = atStartOfRightNode.AdvanceOffset();
      NS_WARNING_ASSERTION(advanced,
                           "Failed to advance offset after current node");
    }
    // If the split point is start of the node and it is a text node or we're
    // not allowed to create empty container node, try to split its parent.
    else {
      lastSplitNodeResult = SplitNodeResult(nullptr, currentRightNode);
      if (currentRightNode == &aMostAncestorToSplit) {
        return lastSplitNodeResult;
      }

      // Try to split its parent before current node.
      lastSplitNodeResult = SplitNodeResult(atStartOfRightNode);
      atStartOfRightNode.Set(currentRightNode);
    }
  }

  return SplitNodeResult(NS_ERROR_FAILURE);
}

void HTMLEditor::DoSplitNode(const EditorDOMPoint& aStartOfRightNode,
                             nsIContent& aNewLeftNode, ErrorResult& aError) {
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  // XXX Perhaps, aStartOfRightNode may be invalid if this is a redo
  //     operation after modifying DOM node with JS.
  if (NS_WARN_IF(!aStartOfRightNode.IsSet())) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  MOZ_ASSERT(aStartOfRightNode.IsSetAndValid());

  // Remember all selection points.
  AutoTArray<SavedRange, 10> savedRanges;
  for (SelectionType selectionType : kPresentSelectionTypes) {
    SavedRange range;
    range.mSelection = GetSelection(selectionType);
    if (NS_WARN_IF(!range.mSelection &&
                   selectionType == SelectionType::eNormal)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }
    if (!range.mSelection) {
      // For non-normal selections, skip over the non-existing ones.
      continue;
    }

    for (uint32_t j = 0; j < range.mSelection->RangeCount(); ++j) {
      RefPtr<const nsRange> r = range.mSelection->GetRangeAt(j);
      MOZ_ASSERT(r->IsPositioned());
      // XXX Looks like that SavedRange should have mStart and mEnd which
      //     are RangeBoundary.  Then, we can avoid to compute offset here.
      range.mStartContainer = r->GetStartContainer();
      range.mStartOffset = r->StartOffset();
      range.mEndContainer = r->GetEndContainer();
      range.mEndOffset = r->EndOffset();

      savedRanges.AppendElement(range);
    }
  }

  nsCOMPtr<nsINode> parent = aStartOfRightNode.GetContainerParent();
  if (NS_WARN_IF(!parent)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Fix the child before mutation observer may touch the DOM tree.
  nsIContent* firstChildOfRightNode = aStartOfRightNode.GetChild();
  parent->InsertBefore(aNewLeftNode, aStartOfRightNode.GetContainer(), aError);
  if (aError.Failed()) {
    NS_WARNING("nsINode::InsertBefore() failed");
    return;
  }

  // At this point, the existing right node has all the children.  Move all
  // the children which are before aStartOfRightNode.
  if (!aStartOfRightNode.IsStartOfContainer()) {
    // If it's a text node, just shuffle around some text
    Text* rightAsText = aStartOfRightNode.GetContainerAsText();
    Text* leftAsText = aNewLeftNode.GetAsText();
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
        MoveAllChildren(*aStartOfRightNode.GetContainer(),
                        EditorRawDOMPoint(&aNewLeftNode, 0), aError);
        NS_WARNING_ASSERTION(!aError.Failed(),
                             "HTMLEditor::MoveAllChildren() failed");
      } else if (NS_WARN_IF(aStartOfRightNode.GetContainer() !=
                            firstChildOfRightNode->GetParentNode())) {
        // firstChildOfRightNode has been moved by mutation observer.
        // In this case, we what should we do?  Use offset?  But we cannot
        // check if the offset is still expected.
      } else {
        MovePreviousSiblings(*firstChildOfRightNode,
                             EditorRawDOMPoint(&aNewLeftNode, 0), aError);
        NS_WARNING_ASSERTION(!aError.Failed(),
                             "HTMLEditor::MovePreviousSiblings() failed");
      }
    }
  }

  // XXX Why do we ignore an error while moving nodes from the right node to
  //     the left node?
  NS_WARNING_ASSERTION(!aError.Failed(), "The previous error is ignored");
  aError.SuppressException();

  // Handle selection
  if (RefPtr<PresShell> presShell = GetPresShell()) {
    presShell->FlushPendingNotifications(FlushType::Frames);
  }
  NS_WARNING_ASSERTION(!Destroyed(),
                       "The editor is destroyed during splitting a node");

  bool allowedTransactionsToChangeSelection =
      AllowsTransactionsToChangeSelection();

  RefPtr<Selection> previousSelection;
  for (size_t i = 0; i < savedRanges.Length(); ++i) {
    // Adjust the selection if needed.
    SavedRange& range = savedRanges[i];

    // If we have not seen the selection yet, clear all of its ranges.
    if (range.mSelection != previousSelection) {
      MOZ_KnownLive(range.mSelection)->RemoveAllRanges(aError);
      if (aError.Failed()) {
        NS_WARNING("Selection::RemoveAllRanges() failed");
        return;
      }
      previousSelection = range.mSelection;
    }

    // XXX Looks like that we don't need to modify normal selection here
    //     because selection will be modified by the caller if
    //     AllowsTransactionsToChangeSelection() will return true.
    if (allowedTransactionsToChangeSelection &&
        range.mSelection->Type() == SelectionType::eNormal) {
      // If the editor should adjust the selection, don't bother restoring
      // the ranges for the normal selection here.
      continue;
    }

    // Split the selection into existing node and new node.
    if (range.mStartContainer == aStartOfRightNode.GetContainer()) {
      if (range.mStartOffset < aStartOfRightNode.Offset()) {
        range.mStartContainer = &aNewLeftNode;
      } else if (range.mStartOffset >= aStartOfRightNode.Offset()) {
        range.mStartOffset -= aStartOfRightNode.Offset();
      } else {
        NS_WARNING(
            "The stored start offset was smaller than the right node offset");
        range.mStartOffset = 0;
      }
    }

    if (range.mEndContainer == aStartOfRightNode.GetContainer()) {
      if (range.mEndOffset < aStartOfRightNode.Offset()) {
        range.mEndContainer = &aNewLeftNode;
      } else if (range.mEndOffset >= aStartOfRightNode.Offset()) {
        range.mEndOffset -= aStartOfRightNode.Offset();
      } else {
        NS_WARNING(
            "The stored end offset was smaller than the right node offset");
        range.mEndOffset = 0;
      }
    }

    RefPtr<nsRange> newRange =
        nsRange::Create(range.mStartContainer, range.mStartOffset,
                        range.mEndContainer, range.mEndOffset, aError);
    if (aError.Failed()) {
      NS_WARNING("nsRange::Create() failed");
      return;
    }
    // The `MOZ_KnownLive` annotation is only necessary because of a bug
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1622253) in the
    // static analyzer.
    MOZ_KnownLive(range.mSelection)
        ->AddRangeAndSelectFramesAndNotifyListeners(*newRange, aError);
    if (aError.Failed()) {
      NS_WARNING(
          "Selection::AddRangeAndSelectFramesAndNotifyListeners() failed");
      return;
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
  if (NS_WARN_IF(parent != aStartOfRightNode.GetContainer()->GetParentNode()) ||
      NS_WARN_IF(parent != aNewLeftNode.GetParentNode()) ||
      NS_WARN_IF(aNewLeftNode.GetNextSibling() !=
                 aStartOfRightNode.GetContainer())) {
    aError.Throw(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
}

nsresult HTMLEditor::JoinNodesWithTransaction(nsINode& aLeftNode,
                                              nsINode& aRightNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aLeftNode.IsContent());
  MOZ_ASSERT(aRightNode.IsContent());

  nsCOMPtr<nsINode> parent = aLeftNode.GetParentNode();
  MOZ_ASSERT(parent);

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eJoinNodes, nsIEditor::ePrevious, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // Remember some values; later used for saved selection updating.
  // Find the offset between the nodes to be joined.
  int32_t offset = parent->ComputeIndexOf(&aRightNode);
  if (NS_WARN_IF(offset < 0)) {
    return NS_ERROR_FAILURE;
  }
  // Find the number of children of the lefthand node
  uint32_t oldLeftNodeLen = aLeftNode.Length();

  TopLevelEditSubActionDataRef().WillJoinContents(*this, *aLeftNode.AsContent(),
                                                  *aRightNode.AsContent());

  RefPtr<JoinNodeTransaction> transaction = JoinNodeTransaction::MaybeCreate(
      *this, *aLeftNode.AsContent(), *aRightNode.AsContent());
  NS_WARNING_ASSERTION(
      transaction, "JoinNodeTransaction::MaybeCreate() failed, but ignored");

  nsresult rv = NS_OK;
  if (transaction) {
    rv = DoTransactionInternal(transaction);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::DoTransactionInternal() failed");
  }

  // XXX Some other transactions manage range updater by themselves.
  //     Why doesn't JoinNodeTransaction do it?
  DebugOnly<nsresult> rvIgnored = RangeUpdaterRef().SelAdjJoinNodes(
      aLeftNode, aRightNode, *parent, AssertedCast<uint32_t>(offset),
      oldLeftNodeLen);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "RangeUpdater::SelAdjJoinNodes() failed, but ignored");

  TopLevelEditSubActionDataRef().DidJoinContents(*this, *aLeftNode.AsContent(),
                                                 *aRightNode.AsContent());

  if (mInlineSpellChecker) {
    RefPtr<mozInlineSpellChecker> spellChecker = mInlineSpellChecker;
    spellChecker->DidJoinNodes(aLeftNode, aRightNode);
  }

  if (mTextServicesDocument && NS_SUCCEEDED(rv)) {
    RefPtr<TextServicesDocument> textServicesDocument = mTextServicesDocument;
    textServicesDocument->DidJoinNodes(*aLeftNode.AsContent(),
                                       *aRightNode.AsContent());
  }

  if (!mActionListeners.IsEmpty()) {
    for (auto& listener : mActionListeners.Clone()) {
      DebugOnly<nsresult> rvIgnored =
          listener->DidJoinNodes(&aLeftNode, &aRightNode, parent, rv);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "nsIEditActionListener::DidJoinNodes() failed, but ignored");
    }
  }

  return rv;
}

nsresult HTMLEditor::DoJoinNodes(nsIContent& aContentToKeep,
                                 nsIContent& aContentToJoin) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  uint32_t firstNodeLength = aContentToJoin.Length();

  EditorRawDOMPoint atNodeToJoin(&aContentToJoin);
  EditorRawDOMPoint atNodeToKeep(&aContentToKeep);

  // Remember all selection points.
  // XXX Do we need to restore all types of selections by ourselves?  Normal
  //     selection should be modified later as result of handling edit action.
  //     IME selections shouldn't be there when nodes are joined.  Spellcheck
  //     selections should be recreated with newer text.  URL selections
  //     shouldn't be there because of used only by the URL bar.
  AutoTArray<SavedRange, 10> savedRanges;
  for (SelectionType selectionType : kPresentSelectionTypes) {
    SavedRange range;
    range.mSelection = GetSelection(selectionType);
    if (selectionType == SelectionType::eNormal) {
      if (NS_WARN_IF(!range.mSelection)) {
        return NS_ERROR_FAILURE;
      }
    } else if (!range.mSelection) {
      // For non-normal selections, skip over the non-existing ones.
      continue;
    }

    for (uint32_t j = 0; j < range.mSelection->RangeCount(); ++j) {
      const RefPtr<nsRange> r = range.mSelection->GetRangeAt(j);
      MOZ_ASSERT(r->IsPositioned());
      range.mStartContainer = r->GetStartContainer();
      range.mStartOffset = r->StartOffset();
      range.mEndContainer = r->GetEndContainer();
      range.mEndOffset = r->EndOffset();

      // If selection endpoint is between the nodes, remember it as being
      // in the one that is going away instead.  This simplifies later selection
      // adjustment logic at end of this method.
      if (range.mStartContainer) {
        if (range.mStartContainer == atNodeToKeep.GetContainer() &&
            atNodeToJoin.Offset() < range.mStartOffset &&
            range.mStartOffset <= atNodeToKeep.Offset()) {
          range.mStartContainer = &aContentToJoin;
          range.mStartOffset = firstNodeLength;
        }
        if (range.mEndContainer == atNodeToKeep.GetContainer() &&
            atNodeToJoin.Offset() < range.mEndOffset &&
            range.mEndOffset <= atNodeToKeep.Offset()) {
          range.mEndContainer = &aContentToJoin;
          range.mEndOffset = firstNodeLength;
        }
      }

      savedRanges.AppendElement(range);
    }
  }

  // OK, ready to do join now.
  // If it's a text node, just shuffle around some text.
  if (aContentToKeep.IsText() && aContentToJoin.IsText()) {
    nsAutoString rightText;
    nsAutoString leftText;
    aContentToKeep.AsText()->GetData(rightText);
    aContentToJoin.AsText()->GetData(leftText);
    leftText += rightText;
    IgnoredErrorResult ignoredError;
    DoSetText(MOZ_KnownLive(*aContentToKeep.AsText()), leftText, ignoredError);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "EditorBase::DoSetText() failed, but ignored");
  } else {
    // Otherwise it's an interior node, so shuffle around the children.
    nsCOMPtr<nsINodeList> childNodes = aContentToJoin.ChildNodes();
    MOZ_ASSERT(childNodes);

    // Remember the first child in aContentToKeep, we'll insert all the children
    // of aContentToJoin in front of it GetFirstChild returns nullptr firstNode
    // if aContentToKeep has no children, that's OK.
    nsCOMPtr<nsIContent> firstNode = aContentToKeep.GetFirstChild();

    // Have to go through the list backwards to keep deletes from interfering
    // with iteration.
    for (uint32_t i = childNodes->Length(); i; --i) {
      nsCOMPtr<nsIContent> childNode = childNodes->Item(i - 1);
      if (childNode) {
        // prepend children of aContentToJoin
        ErrorResult error;
        aContentToKeep.InsertBefore(*childNode, firstNode, error);
        if (NS_WARN_IF(Destroyed())) {
          error.SuppressException();
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (error.Failed()) {
          NS_WARNING("nsINode::InsertBefore() failed");
          return error.StealNSResult();
        }
        firstNode = std::move(childNode);
      }
    }
  }

  // Delete the extra node.
  aContentToJoin.Remove();
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  bool allowedTransactionsToChangeSelection =
      AllowsTransactionsToChangeSelection();

  RefPtr<Selection> previousSelection;
  for (size_t i = 0; i < savedRanges.Length(); ++i) {
    // And adjust the selection if needed.
    SavedRange& range = savedRanges[i];

    // If we have not seen the selection yet, clear all of its ranges.
    if (range.mSelection != previousSelection) {
      ErrorResult error;
      MOZ_KnownLive(range.mSelection)->RemoveAllRanges(error);
      if (NS_WARN_IF(Destroyed())) {
        error.SuppressException();
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (error.Failed()) {
        NS_WARNING("Selection::RemoveAllRanges() failed");
        return error.StealNSResult();
      }
      previousSelection = range.mSelection;
    }

    if (allowedTransactionsToChangeSelection &&
        range.mSelection->Type() == SelectionType::eNormal) {
      // If the editor should adjust the selection, don't bother restoring
      // the ranges for the normal selection here.
      continue;
    }

    // Check to see if we joined nodes where selection starts.
    if (range.mStartContainer == &aContentToJoin) {
      range.mStartContainer = &aContentToKeep;
    } else if (range.mStartContainer == &aContentToKeep) {
      range.mStartOffset += firstNodeLength;
    }

    // Check to see if we joined nodes where selection ends.
    if (range.mEndContainer == &aContentToJoin) {
      range.mEndContainer = &aContentToKeep;
    } else if (range.mEndContainer == &aContentToKeep) {
      range.mEndOffset += firstNodeLength;
    }

    RefPtr<nsRange> newRange =
        nsRange::Create(range.mStartContainer, range.mStartOffset,
                        range.mEndContainer, range.mEndOffset, IgnoreErrors());
    if (!newRange) {
      NS_WARNING("nsRange::Create() failed");
      return NS_ERROR_FAILURE;
    }

    ErrorResult error;
    // The `MOZ_KnownLive` annotation is only necessary because of a bug
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1622253) in the
    // static analyzer.
    MOZ_KnownLive(range.mSelection)
        ->AddRangeAndSelectFramesAndNotifyListeners(*newRange, error);
    if (NS_WARN_IF(Destroyed())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  if (allowedTransactionsToChangeSelection) {
    // Editor wants us to set selection at join point.
    DebugOnly<nsresult> rvIgnored =
        SelectionRef().CollapseInLimiter(&aContentToKeep, firstNodeLength);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Selection::CollapseInLimiter() failed, but ignored");
  }

  return NS_OK;
}

nsresult HTMLEditor::MoveNodeWithTransaction(
    nsIContent& aContent, const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  EditorDOMPoint oldPoint(&aContent);
  if (NS_WARN_IF(!oldPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Don't do anything if it's already in right place.
  if (aPointToInsert == oldPoint) {
    return NS_OK;
  }

  // Notify our internal selection state listener
  AutoMoveNodeSelNotify selNotify(RangeUpdaterRef(), oldPoint, aPointToInsert);

  // Hold a reference so aNode doesn't go away when we remove it (bug 772282)
  // HTMLEditor::DeleteNodeWithTransaction() does not move non-editable
  // node, but we need to move non-editable nodes too.  Therefore, call
  // EditorBase's method directly.
  // XXX Perhaps, this method and DeleteNodeWithTransaction() should take
  //     new argument for making callers specify whether non-editable nodes
  //     should be moved or not.
  nsresult rv = EditorBase::DeleteNodeWithTransaction(aContent);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return rv;
  }

  // Mutation event listener could break insertion point. Let's check it.
  EditorDOMPoint pointToInsert(selNotify.ComputeInsertionPoint());
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  // If some children have removed from the container, let's append to the
  // container.
  // XXX Perhaps, if mutation event listener inserts or removes some children
  //     but the child node referring with aPointToInsert is still available,
  //     we should insert aContent before it.  However, we should keep
  //     traditional behavior for now.
  if (NS_WARN_IF(!pointToInsert.IsSetAndValid())) {
    pointToInsert.SetToEndOf(pointToInsert.GetContainer());
  }
  rv = InsertNodeWithTransaction(aContent, pointToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertNodeWithTransaction() failed");
  return rv;
}

already_AddRefed<Element> HTMLEditor::DeleteSelectionAndCreateElement(
    nsAtom& aTag) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsresult rv = DeleteSelectionAndPrepareToCreateNode();
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteSelectionAndPrepareToCreateNode() failed");
    return nullptr;
  }

  EditorDOMPoint pointToInsert(SelectionRef().AnchorRef());
  if (!pointToInsert.IsSet()) {
    return nullptr;
  }
  Result<RefPtr<Element>, nsresult> maybeNewElement =
      CreateNodeWithTransaction(aTag, pointToInsert);
  if (maybeNewElement.isErr()) {
    NS_WARNING("EditorBase::CreateNodeWithTransaction() failed");
    return nullptr;
  }
  MOZ_ASSERT(maybeNewElement.inspect());

  // We want the selection to be just after the new node
  EditorRawDOMPoint afterNewElement(
      EditorRawDOMPoint::After(maybeNewElement.inspect()));
  MOZ_ASSERT(afterNewElement.IsSetAndValid());
  IgnoredErrorResult ignoredError;
  SelectionRef().CollapseInLimiter(afterNewElement, ignoredError);
  if (ignoredError.Failed()) {
    NS_WARNING("Selection::CollapseInLimiter() failed");
    // XXX Even if it succeeded to create new element, this returns error
    //     when Selection.Collapse() fails something.  This could occur with
    //     mutation observer or mutation event listener.
    return nullptr;
  }
  return maybeNewElement.unwrap().forget();
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
    EditorRawDOMPoint atAnchorContainer(atAnchor.GetContainer());
    if (NS_WARN_IF(!atAnchorContainer.IsSetAndValid())) {
      return NS_ERROR_FAILURE;
    }
    ErrorResult error;
    SelectionRef().CollapseInLimiter(atAnchorContainer, error);
    NS_WARNING_ASSERTION(!error.Failed(),
                         "Selection::CollapseInLimiter() failed");
    return error.StealNSResult();
  }

  if (atAnchor.IsEndOfContainer()) {
    EditorRawDOMPoint afterAnchorContainer(atAnchor.GetContainer());
    if (NS_WARN_IF(!afterAnchorContainer.AdvanceOffset())) {
      return NS_ERROR_FAILURE;
    }
    ErrorResult error;
    SelectionRef().CollapseInLimiter(afterAnchorContainer, error);
    NS_WARNING_ASSERTION(!error.Failed(),
                         "Selection::CollapseInLimiter() failed");
    return error.StealNSResult();
  }

  ErrorResult error;
  nsCOMPtr<nsIContent> newLeftNode = SplitNodeWithTransaction(atAnchor, error);
  if (error.Failed()) {
    NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
    return error.StealNSResult();
  }

  EditorRawDOMPoint atRightNode(atAnchor.GetContainer());
  if (NS_WARN_IF(!atRightNode.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atRightNode.IsSetAndValid());
  SelectionRef().CollapseInLimiter(atRightNode, error);
  NS_WARNING_ASSERTION(!error.Failed(),
                       "Selection::CollapseInLimiter() failed");
  return error.StealNSResult();
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
    existingValue.Append(' ');
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

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
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
          if (RefPtr<nsStyledElement> blockStyledElement =
                  nsStyledElement::FromNodeOrNull(
                      HTMLEditUtils::GetAncestorBlockElement(
                          *startOfRange.ContainerAsText()))) {
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
          if (RefPtr<nsStyledElement> blockStyledElement =
                  nsStyledElement::FromNodeOrNull(
                      HTMLEditUtils::GetInclusiveAncestorBlockElement(
                          *startOfRange.GetChild()))) {
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
        RefPtr<Element> blockElement = HTMLEditUtils::GetAncestorBlockElement(
            *startOfRange.ContainerAsText());
        if (blockElement && handledBlockParent != blockElement) {
          handledBlockParent = blockElement;
          if (nsStyledElement* blockStyledElement =
                  nsStyledElement::FromNode(blockElement)) {
            // MOZ_KnownLive(*blockStyledElement): It's blockElement whose
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

      // Then, set background color of each block or block parent of all nodes
      // in the range entirely.
      for (OwningNonNull<nsIContent>& content : arrayOfContents) {
        RefPtr<Element> blockElement =
            HTMLEditUtils::GetInclusiveAncestorBlockElement(content);
        if (blockElement && handledBlockParent != blockElement) {
          handledBlockParent = blockElement;
          if (nsStyledElement* blockStyledElement =
                  nsStyledElement::FromNode(blockElement)) {
            // MOZ_KnownLive(*blockStyledElement): It's blockElement whose
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
        RefPtr<Element> blockElement = HTMLEditUtils::GetAncestorBlockElement(
            *endOfRange.ContainerAsText());
        if (blockElement && handledBlockParent != blockElement) {
          if (nsStyledElement* blockStyledElement =
                  nsStyledElement::FromNode(blockElement)) {
            // MOZ_KnownLive(*blockStyledElement): It's blockElement whose
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

nsresult HTMLEditor::CopyLastEditableChildStylesWithTransaction(
    Element& aPreviousBlock, Element& aNewBlock,
    RefPtr<Element>* aNewBRElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aNewBRElement)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aNewBRElement = nullptr;

  RefPtr<Element> previousBlock(&aPreviousBlock);
  RefPtr<Element> newBlock(&aNewBlock);

  // First, clear out aNewBlock.  Contract is that we want only the styles
  // from aPreviousBlock.
  for (nsCOMPtr<nsIContent> child = newBlock->GetFirstChild(); child;
       child = newBlock->GetFirstChild()) {
    nsresult rv = DeleteNodeWithTransaction(*child);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }

  RefPtr<Element> editingHost = GetActiveEditingHost();
  if (!editingHost) {
    return NS_OK;
  }

  // XXX aNewBlock may be moved or removed.  Even in such case, we should
  //     keep cloning the styles?

  // Look for the deepest last editable leaf node in aPreviousBlock.
  // Then, if found one is a <br> element, look for non-<br> element.
  nsIContent* deepestEditableContent = nullptr;
  for (nsCOMPtr<nsIContent> child = previousBlock.get(); child;
       child = HTMLEditUtils::GetLastChild(
           *child, {WalkTreeOption::IgnoreNonEditableNode})) {
    deepestEditableContent = child;
  }
  while (deepestEditableContent &&
         deepestEditableContent->IsHTMLElement(nsGkAtoms::br)) {
    deepestEditableContent = HTMLEditUtils::GetPreviousContent(
        *deepestEditableContent, {WalkTreeOption::IgnoreNonEditableNode},
        editingHost);
  }
  if (!deepestEditableContent) {
    return NS_OK;
  }

  Element* deepestVisibleEditableElement =
      deepestEditableContent->GetAsElementOrParentElement();
  if (!deepestVisibleEditableElement) {
    return NS_OK;
  }

  // Clone inline elements to keep current style in the new block.
  // XXX Looks like that this is really slow if lastEditableDescendant is
  //     far from aPreviousBlock.  Probably, we should clone inline containers
  //     from ancestor to descendants without transactions, then, insert it
  //     after that with transaction.
  RefPtr<Element> lastClonedElement, firstClonedElement;
  for (RefPtr<Element> elementInPreviousBlock = deepestVisibleEditableElement;
       elementInPreviousBlock && elementInPreviousBlock != previousBlock;
       elementInPreviousBlock = elementInPreviousBlock->GetParentElement()) {
    if (!HTMLEditUtils::IsInlineStyle(elementInPreviousBlock) &&
        !elementInPreviousBlock->IsHTMLElement(nsGkAtoms::span)) {
      continue;
    }
    nsAtom* tagName = elementInPreviousBlock->NodeInfo()->NameAtom();
    // At first time, just create the most descendant inline container
    // element.
    if (!firstClonedElement) {
      Result<RefPtr<Element>, nsresult> maybeNewElement =
          CreateNodeWithTransaction(MOZ_KnownLive(*tagName),
                                    EditorDOMPoint(newBlock, 0));
      if (maybeNewElement.isErr()) {
        NS_WARNING("EditorBase::CreateNodeWithTransaction() failed");
        return maybeNewElement.unwrapErr();
      }
      firstClonedElement = lastClonedElement = maybeNewElement.unwrap();
      // Clone all attributes.
      // XXX Looks like that this clones id attribute too.
      CloneAttributesWithTransaction(*lastClonedElement,
                                     *elementInPreviousBlock);
      continue;
    }
    // Otherwise, inserts new parent inline container to the previous inserted
    // inline container.
    lastClonedElement = InsertContainerWithTransaction(*lastClonedElement,
                                                       MOZ_KnownLive(*tagName));
    if (!lastClonedElement) {
      NS_WARNING("HTMLEditor::InsertContainerWithTransaction() failed");
      return NS_ERROR_FAILURE;
    }
    CloneAttributesWithTransaction(*lastClonedElement, *elementInPreviousBlock);
  }

  if (!firstClonedElement) {
    // XXX Even if no inline elements are cloned, shouldn't we create new
    //     <br> element for aNewBlock?
    return NS_OK;
  }

  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      InsertBRElementWithTransaction(EditorDOMPoint(firstClonedElement, 0));
  if (resultOfInsertingBRElement.isErr()) {
    NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
    return resultOfInsertingBRElement.unwrapErr();
  }
  MOZ_ASSERT(resultOfInsertingBRElement.inspect());
  *aNewBRElement = resultOfInsertingBRElement.unwrap().forget();
  return NS_OK;
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
    uint32_t rangeCount = SelectionRef().RangeCount();
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
      for (uint32_t i = 0; i < rangeCount; i++) {
        const nsRange* range = SelectionRef().GetRangeAt(i);
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

bool HTMLEditor::GetReturnInParagraphCreatesNewParagraph() {
  return mCRInParagraphCreatesParagraph;
}

nsresult HTMLEditor::GetReturnInParagraphCreatesNewParagraph(
    bool* aCreatesNewParagraph) {
  *aCreatesNewParagraph = mCRInParagraphCreatesParagraph;
  return NS_OK;
}

nsIContent* HTMLEditor::GetFocusedContent() const {
  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (NS_WARN_IF(!focusManager)) {
    return nullptr;
  }

  nsIContent* focusedContent = focusManager->GetFocusedElement();

  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  bool inDesignMode = document->HasFlag(NODE_IS_EDITABLE);
  if (!focusedContent) {
    // in designMode, nobody gets focus in most cases.
    if (inDesignMode && OurWindowHasFocus()) {
      return document->GetRootElement();
    }
    return nullptr;
  }

  if (inDesignMode) {
    return OurWindowHasFocus() &&
                   focusedContent->IsInclusiveDescendantOf(document)
               ? focusedContent
               : nullptr;
  }

  // We're HTML editor for contenteditable

  // If the focused content isn't editable, or it has independent selection,
  // we don't have focus.
  if (!focusedContent->HasFlag(NODE_IS_EDITABLE) ||
      focusedContent->HasIndependentSelection()) {
    return nullptr;
  }
  // If our window is focused, we're focused.
  return OurWindowHasFocus() ? focusedContent : nullptr;
}

nsIContent* HTMLEditor::GetFocusedContentForIME() const {
  nsIContent* focusedContent = GetFocusedContent();
  if (!focusedContent) {
    return nullptr;
  }

  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  return document->HasFlag(NODE_IS_EDITABLE) ? nullptr : focusedContent;
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
  bool inDesignMode = document->HasFlag(NODE_IS_EDITABLE);

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

Element* HTMLEditor::GetActiveEditingHost(
    LimitInBodyElement aLimitInBodyElement /* = LimitInBodyElement::Yes */)
    const {
  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  if (document->HasFlag(NODE_IS_EDITABLE)) {
    return document->GetBodyElement();
  }

  // We're HTML editor for contenteditable
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return nullptr;
  }

  nsINode* focusNode = SelectionRef().GetFocusNode();
  if (NS_WARN_IF(!focusNode) || NS_WARN_IF(!focusNode->IsContent())) {
    return nullptr;
  }
  nsIContent* content = focusNode->AsContent();

  // If the active content isn't editable, or it has independent selection,
  // we're not active.
  if (!content->HasFlag(NODE_IS_EDITABLE) ||
      content->HasIndependentSelection()) {
    return nullptr;
  }
  Element* candidateEditingHost = content->GetEditingHost();
  if (!candidateEditingHost) {
    return nullptr;
  }
  // Currently, we don't support editing outside of `<body>` element.
  return aLimitInBodyElement != LimitInBodyElement::Yes ||
                 (document->GetBodyElement() &&
                  nsContentUtils::ContentIsFlattenedTreeDescendantOf(
                      candidateEditingHost, document->GetBodyElement()))
             ? candidateEditingHost
             : document->GetBodyElement();
}

void HTMLEditor::NotifyEditingHostMaybeChanged() {
  Document* document = GetDocument();
  if (NS_WARN_IF(!document) ||
      NS_WARN_IF(document->HasFlag(NODE_IS_EDITABLE))) {
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
  nsIContent* editingHost = GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return;
  }

  // Update selection ancestor limit if current editing host includes the
  // previous editing host.
  if (ancestorLimiter->IsInclusiveDescendantOf(editingHost)) {
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
  nsIContent* focusedContent = GetFocusedContent();
  if (!focusedContent) {
    return nullptr;
  }

  // focusedContent might be non-null even focusManager->GetFocusedContent()
  // is null.  That's the designMode case, and in that case our
  // FocusedContent() returns the root element, but we want to return
  // the document.

  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  NS_ASSERTION(focusManager, "Focus manager is null");
  Element* focusedElement = focusManager->GetFocusedElement();
  if (focusedElement) {
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

  RefPtr<EventTarget> eventTarget = aGUIEvent->GetOriginalDOMEventTarget();
  if (NS_WARN_IF(!eventTarget)) {
    return false;
  }
  nsCOMPtr<nsINode> eventTargetNode = do_QueryInterface(eventTarget);
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

  if (document->HasFlag(NODE_IS_EDITABLE)) {
    // If this editor is in designMode and the event target is the document,
    // the event is for this editor.
    if (eventTargetNode->IsDocument()) {
      return eventTargetNode == document;
    }
    // Otherwise, check whether the event target is in this document or not.
    if (NS_WARN_IF(!eventTargetNode->IsContent())) {
      return false;
    }
    return document == eventTargetNode->GetUncomposedDoc();
  }

  // This HTML editor is for contenteditable.  We need to check the validity
  // of the target.
  if (NS_WARN_IF(!eventTargetNode->IsContent())) {
    return false;
  }

  // If the event is a mouse event, we need to check if the target content is
  // the focused editing host or its descendant.
  if (aGUIEvent->AsMouseEventBase()) {
    nsIContent* editingHost = GetActiveEditingHost();
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
  RefPtr<Element> target = GetActiveEditingHost(LimitInBodyElement::No);
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

Element* HTMLEditor::GetEditorRoot() const { return GetActiveEditingHost(); }

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
