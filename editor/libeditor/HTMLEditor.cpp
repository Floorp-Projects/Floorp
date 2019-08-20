/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include "mozilla/ComposerCommandsUpdater.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/EventStates.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/mozInlineSpellChecker.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_editor.h"
#include "mozilla/TextEvents.h"

#include "nsCRT.h"

#include "nsUnicharUtils.h"

#include "HTMLEditorEventListener.h"
#include "HTMLEditRules.h"
#include "HTMLEditUtils.h"
#include "HTMLURIRefObject.h"
#include "TextEditUtils.h"
#include "TypeInState.h"

#include "nsHTMLDocument.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsISelectionController.h"
#include "nsIInlineSpellChecker.h"
#include "nsIPrincipal.h"

#include "mozilla/css/Loader.h"

#include "nsIContent.h"
#include "nsIMutableArray.h"
#include "nsContentUtils.h"
#include "nsIDocumentEncoder.h"
#include "nsGenericHTMLElement.h"
#include "nsPresContext.h"
#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"

// netwerk
#include "nsIURI.h"
#include "nsNetUtil.h"

// Misc
#include "mozilla/EditorUtils.h"
#include "WSRunObject.h"
#include "nsGkAtoms.h"
#include "nsIWidget.h"

#include "nsIFrame.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "nsElementTable.h"
#include "nsTextFragment.h"
#include "nsContentList.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

namespace mozilla {

using namespace dom;
using namespace widget;

const char16_t kNBSP = 160;

static already_AddRefed<nsAtom> GetLowerCaseNameAtom(
    const nsAString& aTagName) {
  if (aTagName.IsEmpty()) {
    return nullptr;
  }
  nsAutoString lowerTagName;
  nsContentUtils::ASCIIToLower(aTagName, lowerTagName);
  return NS_Atomize(lowerTagName);
}

// Some utilities to handle overloading of "A" tag for link and named anchor.
static bool IsLinkTag(const nsAtom& aTagName) {
  return &aTagName == nsGkAtoms::href;
}

static bool IsNamedAnchorTag(const nsAtom& aTagName) {
  return &aTagName == nsGkAtoms::anchor;
}

HTMLEditor::HTMLEditor()
    : mCRInParagraphCreatesParagraph(false),
      mCSSAware(false),
      mSelectedCellIndex(0),
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
  if (mRules && mRules->AsHTMLEditRules()) {
    mRules->AsHTMLEditRules()->EndListeningToEditSubActions();
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

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLEditor, TextEditor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTypeInState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mComposerCommandsUpdater)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChangedRangeForTopLevelEditSubAction)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStyleSheets)

  tmp->HideAnonymousEditingUIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLEditor, TextEditor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTypeInState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mComposerCommandsUpdater)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChangedRangeForTopLevelEditSubAction)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyleSheets)

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
  NS_INTERFACE_MAP_ENTRY(nsIEditorStyleSheets)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsIEditorMailSupport)
NS_INTERFACE_MAP_END_INHERITING(TextEditor)

nsresult HTMLEditor::Init(Document& aDoc, Element* aRoot,
                          nsISelectionController* aSelCon, uint32_t aFlags,
                          const nsAString& aInitialValue) {
  MOZ_ASSERT(aInitialValue.IsEmpty(), "Non-empty initial values not supported");

  nsresult rulesRv = NS_OK;

  {
    // block to scope AutoEditInitRulesTrigger
    AutoEditInitRulesTrigger rulesTrigger(this, rulesRv);

    // Init the plaintext editor
    nsresult rv = TextEditor::Init(aDoc, aRoot, nullptr, aFlags, aInitialValue);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Init mutation observer
    aDoc.AddMutationObserverUnlessExists(this);

    if (!mRootElement) {
      UpdateRootElement();
    }

    // disable Composer-only features
    if (IsMailEditor()) {
      SetAbsolutePositioningEnabled(false);
      SetSnapToGridEnabled(false);
    }

    // Init the HTML-CSS utils
    mCSSEditUtils = MakeUnique<CSSEditUtils>(this);

    // disable links
    Document* doc = GetDocument();
    if (NS_WARN_IF(!doc)) {
      return NS_ERROR_FAILURE;
    }
    if (!IsPlaintextEditor() && !IsInteractionAllowed()) {
      mDisabledLinkHandling = true;
      mOldLinkHandlingEnabled = doc->LinkHandlingEnabled();
      doc->SetLinkHandlingEnabled(false);
    }

    // init the type-in state
    mTypeInState = new TypeInState();

    if (!IsInteractionAllowed()) {
      // ignore any errors from this in case the file is missing
      AddOverrideStyleSheetInternal(
          NS_LITERAL_STRING("resource://gre/res/EditorOverride.css"));
    }
  }
  NS_ENSURE_SUCCESS(rulesRv, rulesRv);

  return NS_OK;
}

void HTMLEditor::PreDestroy(bool aDestroyingFrames) {
  if (mDidPreDestroy) {
    return;
  }

  // FYI: Cannot create AutoEditActionDataSetter here.  However, it does not
  //      necessary for the methods called by the following code.

  RefPtr<Document> document = GetDocument();
  if (document) {
    document->RemoveMutationObserver(this);
  }

  while (!mStyleSheetURLs.IsEmpty()) {
    DebugOnly<nsresult> rv =
        RemoveOverrideStyleSheetInternal(mStyleSheetURLs[0]);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to remove an override style sheet");
  }

  // Clean up after our anonymous content -- we don't want these nodes to
  // stay around (which they would, since the frames have an owning reference).
  HideAnonymousEditingUIs();

  EditorBase::PreDestroy(aDestroyingFrames);
}

NS_IMETHODIMP
HTMLEditor::NotifySelectionChanged(Document* aDocument, Selection* aSelection,
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
    typeInState->OnSelectionChange(*aSelection);

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
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "RefreshEditingUI() failed");
    }
  }

  if (mComposerCommandsUpdater) {
    RefPtr<ComposerCommandsUpdater> updater = mComposerCommandsUpdater;
    updater->OnSelectionChange();
  }

  return EditorBase::NotifySelectionChanged(aDocument, aSelection, aReason);
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

  Document* doc = aNode->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  if (aNode->IsInUncomposedDoc() &&
      (doc->HasFlag(NODE_IS_EDITABLE) || !aNode->IsContent())) {
    return doc->GetRootElement();
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
        content->AsElement()->State().HasState(NS_EVENT_STATE_MOZ_READWRITE)) {
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
  return listener->Connect(this);
}

void HTMLEditor::RemoveEventListeners() {
  if (!IsInitialized()) {
    return;
  }

  TextEditor::RemoveEventListeners();
}

NS_IMETHODIMP
HTMLEditor::SetFlags(uint32_t aFlags) {
  nsresult rv = TextEditor::SetFlags(aFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // Sets mCSSAware to correspond to aFlags. This toggles whether CSS is
  // used to style elements in the editor. Note that the editor is only CSS
  // aware by default in Composer and in the mail editor.
  mCSSAware = !NoCSS() && !IsMailEditor();

  return NS_OK;
}

nsresult HTMLEditor::InitRules() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mRules) {
    // instantiate the rules for the html editor
    mRules = new HTMLEditRules();
  }
  RefPtr<TextEditRules> rules(mRules);
  return rules->Init(this);
}

NS_IMETHODIMP
HTMLEditor::BeginningOfDocument() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return MaybeCollapseSelectionAtFirstEditableNode(false);
}

void HTMLEditor::InitializeSelectionAncestorLimit(nsIContent& aAncestorLimit) {
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
  if (SelectionRefPtr()->RangeCount() == 1 &&
      SelectionRefPtr()->IsCollapsed()) {
    Element* editingHost = GetActiveEditingHost();
    nsRange* range = SelectionRefPtr()->GetRangeAt(0);
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
    MaybeCollapseSelectionAtFirstEditableNode(true);
  }
}

nsresult HTMLEditor::MaybeCollapseSelectionAtFirstEditableNode(
    bool aIgnoreIfSelectionInEditingHost) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Use editing host.  If you use root element here, selection may be
  // moved to <head> element, e.g., if there is a text node in <script>
  // element.  So, we should use active editing host.
  RefPtr<Element> editingHost = GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return NS_OK;
  }

  // If selection range is already in the editing host and the range is not
  // start of the editing host, we shouldn't reset selection.  E.g., window
  // is activated when the editor had focus before inactivated.
  if (aIgnoreIfSelectionInEditingHost && SelectionRefPtr()->RangeCount() == 1) {
    nsRange* range = SelectionRefPtr()->GetRangeAt(0);
    if (!range->Collapsed() ||
        range->GetStartContainer() != editingHost.get() ||
        range->StartOffset()) {
      return NS_OK;
    }
  }

  // Find first editable and visible node.
  EditorRawDOMPoint pointToPutCaret(editingHost, 0);
  for (;;) {
    WSRunObject wsObj(this, pointToPutCaret.GetContainer(),
                      pointToPutCaret.Offset());
    int32_t visOffset = 0;
    WSType visType;
    nsCOMPtr<nsINode> visNode;
    wsObj.NextVisibleNode(pointToPutCaret, address_of(visNode), &visOffset,
                          &visType);

    // If we meet a non-editable node first, we should move caret to start of
    // the editing host (perhaps, user may want to insert something before
    // the first non-editable node? Chromium behaves so).
    if (visNode && !visNode->IsEditable()) {
      pointToPutCaret.Set(editingHost, 0);
      break;
    }

    // WSRunObject::NextVisibleNode() returns WSType::special and the "special"
    // node when it meets empty inline element.  In this case, we should go to
    // next sibling.  For example, if current editor is:
    // <div contenteditable><span></span><b><br></b></div>
    // then, we should put caret at the <br> element.  So, let's check if
    // found node is an empty inline container element.
    if (visType == WSType::special && visNode &&
        TagCanContainTag(*visNode->NodeInfo()->NameAtom(),
                         *nsGkAtoms::textTagName)) {
      pointToPutCaret.Set(visNode);
      DebugOnly<bool> advanced = pointToPutCaret.AdvanceOffset();
      NS_WARNING_ASSERTION(
          advanced,
          "Failed to advance offset from found empty inline container element");
      continue;
    }

    // If there is editable and visible text node, move caret at start of it.
    if (visType == WSType::normalWS || visType == WSType::text) {
      pointToPutCaret.Set(visNode, visOffset);
      break;
    }

    // If there is editable <br> or something inline special element like
    // <img>, <input>, etc, move caret before it.
    if (visType == WSType::br || visType == WSType::special) {
      pointToPutCaret.Set(visNode);
      break;
    }

    // If there is no visible/editable node except another block element in
    // current editing host, we should move caret to very first of the editing
    // host.
    // XXX This may not make sense, but Chromium behaves so.  Therefore, the
    //     reason why we do this is just compatibility with Chromium.
    if (visType != WSType::otherBlock) {
      pointToPutCaret.Set(editingHost, 0);
      break;
    }

    // By definition of WSRunObject, a block element terminates a whitespace
    // run. That is, although we are calling a method that is named
    // "NextVisibleNode", the node returned might not be visible/editable!

    // However, we were given a block that is not a container.  Since the
    // block can not contain anything that's visible, such a block only
    // makes sense if it is visible by itself, like a <hr>.  We want to
    // place the caret in front of that block.
    if (!IsContainer(visNode)) {
      pointToPutCaret.Set(visNode);
      break;
    }

    // If the given block does not contain any visible/editable items, we want
    // to skip it and continue our search.
    bool isEmptyBlock;
    if (NS_SUCCEEDED(IsEmptyNode(visNode, &isEmptyBlock)) && isEmptyBlock) {
      // Skip the empty block
      pointToPutCaret.Set(visNode);
      DebugOnly<bool> advanced = pointToPutCaret.AdvanceOffset();
      NS_WARNING_ASSERTION(
          advanced, "Failed to advance offset from the found empty block node");
    } else {
      pointToPutCaret.Set(visNode, 0);
    }
  }
  nsresult rv = SelectionRefPtr()->Collapse(pointToPutCaret);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent) {
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/tests/test_htmleditor_keyevent_handling.html

  if (IsReadonly() || IsDisabled()) {
    // When we're not editable, the events are handled on EditorBase, so, we can
    // bypass TextEditor.
    return EditorBase::HandleKeyPressEvent(aKeyboardEvent);
  }

  if (NS_WARN_IF(!aKeyboardEvent)) {
    return NS_ERROR_UNEXPECTED;
  }
  MOZ_ASSERT(aKeyboardEvent->mMessage == eKeyPress,
             "HandleKeyPressEvent gets non-keypress event");

  switch (aKeyboardEvent->mKeyCode) {
    case NS_VK_META:
    case NS_VK_WIN:
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
      // These keys are handled on EditorBase, so, we can bypass
      // TextEditor.
      return EditorBase::HandleKeyPressEvent(aKeyboardEvent);
    case NS_VK_BACK:
    case NS_VK_DELETE:
      // These keys are handled on TextEditor.
      return TextEditor::HandleKeyPressEvent(aKeyboardEvent);
    case NS_VK_TAB: {
      if (IsPlaintextEditor()) {
        // If this works as plain text editor, e.g., mail editor for plain
        // text, should be handled on TextEditor.
        return TextEditor::HandleKeyPressEvent(aKeyboardEvent);
      }

      if (IsTabbable()) {
        return NS_OK;  // let it be used for focus switching
      }

      if (aKeyboardEvent->IsControl() || aKeyboardEvent->IsAlt() ||
          aKeyboardEvent->IsMeta() || aKeyboardEvent->IsOS()) {
        return NS_OK;
      }

      RefPtr<Selection> selection = GetSelection();
      if (NS_WARN_IF(!selection) || NS_WARN_IF(!selection->RangeCount())) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsINode> node = selection->GetRangeAt(0)->GetStartContainer();
      MOZ_ASSERT(node);

      nsCOMPtr<Element> blockParent = GetBlock(*node);

      if (!blockParent) {
        break;
      }

      bool handled = false;
      nsresult rv = NS_OK;
      if (HTMLEditUtils::IsTableElement(blockParent)) {
        rv = TabInTable(aKeyboardEvent->IsShift(), &handled);
        // TabInTable might cause reframe
        if (Destroyed()) {
          return NS_OK;
        }
        if (handled) {
          ScrollSelectionIntoView(false);
        }
      } else if (HTMLEditUtils::IsListItem(blockParent)) {
        rv = !aKeyboardEvent->IsShift() ? IndentAsAction() : OutdentAsAction();
        handled = true;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (handled) {
        aKeyboardEvent->PreventDefault();  // consumed
        return NS_OK;
      }
      if (aKeyboardEvent->IsShift()) {
        return NS_OK;  // don't type text for shift tabs
      }
      aKeyboardEvent->PreventDefault();
      return OnInputText(NS_LITERAL_STRING("\t"));
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
                             "InsertLineBreakAsAction() failed");
        return EditorBase::ToGenericNSResult(rv);
      }
      // uses rules to figure out what to insert
      nsresult rv = InsertParagraphSeparatorAsAction();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "InsertParagraphSeparatorAsAction() failed");
      return EditorBase::ToGenericNSResult(rv);
  }

  if (!aKeyboardEvent->IsInputtingText()) {
    // we don't PreventDefault() here or keybindings like control-x won't work
    return NS_OK;
  }
  aKeyboardEvent->PreventDefault();
  nsAutoString str(aKeyboardEvent->mCharCode);
  return OnInputText(str);
}

/**
 * Returns true if the id represents an element of block type.
 * Can be used to determine if a new paragraph should be started.
 */
bool HTMLEditor::NodeIsBlockStatic(const nsINode* aElement) {
  MOZ_ASSERT(aElement);

  // We want to treat these as block nodes even though nsHTMLElement says
  // they're not.
  if (aElement->IsAnyOfHTMLElements(
          nsGkAtoms::body, nsGkAtoms::head, nsGkAtoms::tbody, nsGkAtoms::thead,
          nsGkAtoms::tfoot, nsGkAtoms::tr, nsGkAtoms::th, nsGkAtoms::td,
          nsGkAtoms::dt, nsGkAtoms::dd)) {
    return true;
  }

  return nsHTMLElement::IsBlock(
      nsHTMLTags::AtomTagToId(aElement->NodeInfo()->NameAtom()));
}

NS_IMETHODIMP
HTMLEditor::NodeIsBlock(nsINode* aNode, bool* aIsBlock) {
  *aIsBlock = IsBlockNode(aNode);
  return NS_OK;
}

bool HTMLEditor::IsBlockNode(nsINode* aNode) const {
  return aNode && NodeIsBlockStatic(aNode);
}

/**
 * GetBlockNodeParent returns enclosing block level ancestor, if any.
 */
Element* HTMLEditor::GetBlockNodeParent(nsINode* aNode,
                                        nsINode* aAncestorLimiter) {
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(!aAncestorLimiter || aNode == aAncestorLimiter ||
                 EditorUtils::IsDescendantOf(*aNode, *aAncestorLimiter),
             "aNode isn't in aAncestorLimiter");

  // The caller has already reached the limiter.
  if (aNode == aAncestorLimiter) {
    return nullptr;
  }

  nsCOMPtr<nsINode> p = aNode->GetParentNode();

  while (p) {
    if (NodeIsBlockStatic(p)) {
      return p->AsElement();
    }
    // Now, we have reached the limiter, there is no block in its ancestors.
    if (p == aAncestorLimiter) {
      return nullptr;
    }
    p = p->GetParentNode();
  }

  return nullptr;
}

/**
 * Returns the node if it's a block, otherwise GetBlockNodeParent
 */
Element* HTMLEditor::GetBlock(nsINode& aNode, nsINode* aAncestorLimiter) {
  MOZ_ASSERT(!aAncestorLimiter || &aNode == aAncestorLimiter ||
                 EditorUtils::IsDescendantOf(aNode, *aAncestorLimiter),
             "aNode isn't in aAncestorLimiter");

  if (NodeIsBlockStatic(&aNode)) {
    return aNode.AsElement();
  }
  return GetBlockNodeParent(&aNode, aAncestorLimiter);
}

/**
 * IsNextCharInNodeWhitespace() checks the adjacent content in the same node to
 * see if following selection is whitespace or nbsp.
 */
void HTMLEditor::IsNextCharInNodeWhitespace(nsIContent* aContent,
                                            int32_t aOffset, bool* outIsSpace,
                                            bool* outIsNBSP,
                                            nsIContent** outNode,
                                            int32_t* outOffset) {
  MOZ_ASSERT(aContent && outIsSpace && outIsNBSP);
  MOZ_ASSERT((outNode && outOffset) || (!outNode && !outOffset));
  *outIsSpace = false;
  *outIsNBSP = false;
  if (outNode && outOffset) {
    *outNode = nullptr;
    *outOffset = -1;
  }

  if (aContent->IsText() && (uint32_t)aOffset < aContent->Length()) {
    char16_t ch = aContent->GetText()->CharAt(aOffset);
    *outIsSpace = nsCRT::IsAsciiSpace(ch);
    *outIsNBSP = (ch == kNBSP);
    if (outNode && outOffset) {
      NS_IF_ADDREF(*outNode = aContent);
      // yes, this is _past_ the character
      *outOffset = aOffset + 1;
    }
  }
}

/**
 * IsPrevCharInNodeWhitespace() checks the adjacent content in the same node to
 * see if following selection is whitespace.
 */
void HTMLEditor::IsPrevCharInNodeWhitespace(nsIContent* aContent,
                                            int32_t aOffset, bool* outIsSpace,
                                            bool* outIsNBSP,
                                            nsIContent** outNode,
                                            int32_t* outOffset) {
  MOZ_ASSERT(aContent && outIsSpace && outIsNBSP);
  MOZ_ASSERT((outNode && outOffset) || (!outNode && !outOffset));
  *outIsSpace = false;
  *outIsNBSP = false;
  if (outNode && outOffset) {
    *outNode = nullptr;
    *outOffset = -1;
  }

  if (aContent->IsText() && aOffset > 0) {
    char16_t ch = aContent->GetText()->CharAt(aOffset - 1);
    *outIsSpace = nsCRT::IsAsciiSpace(ch);
    *outIsNBSP = (ch == kNBSP);
    if (outNode && outOffset) {
      NS_IF_ADDREF(*outNode = aContent);
      *outOffset = aOffset - 1;
    }
  }
}

bool HTMLEditor::IsVisibleBRElement(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  if (!TextEditUtils::IsBreak(aNode)) {
    return false;
  }
  // Check if there is another element or text node in block after current
  // <br> element.
  // Note that even if following node is non-editable, it may make the
  // <br> element visible if it just exists.
  // E.g., foo<br><button contenteditable="false">button</button>
  // However, we need to ignore invisible data nodes like comment node.
  nsCOMPtr<nsINode> nextNode = GetNextHTMLElementOrTextInBlock(*aNode);
  if (nextNode && TextEditUtils::IsBreak(nextNode)) {
    return true;
  }

  // A single line break before a block boundary is not displayed, so e.g.
  // foo<p>bar<br></p> and foo<br><p>bar</p> display the same as foo<p>bar</p>.
  // But if there are multiple <br>s in a row, all but the last are visible.
  if (!nextNode) {
    // This break is trailer in block, it's not visible
    return false;
  }
  if (IsBlockNode(nextNode)) {
    // Break is right before a block, it's not visible
    return false;
  }

  // If there's an inline node after this one that's not a break, and also a
  // prior break, this break must be visible.
  // Note that even if previous node is non-editable, it may make the
  // <br> element visible if it just exists.
  // E.g., <button contenteditable="false"><br>foo
  // However, we need to ignore invisible data nodes like comment node.
  nsCOMPtr<nsINode> priorNode = GetPreviousHTMLElementOrTextInBlock(*aNode);
  if (priorNode && TextEditUtils::IsBreak(priorNode)) {
    return true;
  }

  // Sigh.  We have to use expensive whitespace calculation code to
  // determine what is going on
  int32_t selOffset;
  nsCOMPtr<nsINode> selNode = GetNodeLocation(aNode, &selOffset);
  // Let's look after the break
  selOffset++;
  WSRunObject wsObj(this, selNode, selOffset);
  WSType visType;
  wsObj.NextVisibleNode(EditorRawDOMPoint(selNode, selOffset), &visType);
  if (visType & WSType::block) {
    return false;
  }

  return true;
}

NS_IMETHODIMP
HTMLEditor::UpdateBaseURL() {
  RefPtr<Document> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // Look for an HTML <base> tag
  RefPtr<nsContentList> nodeList =
      doc->GetElementsByTagName(NS_LITERAL_STRING("base"));

  // If no base tag, then set baseURL to the document's URL.  This is very
  // important, else relative URLs for links and images are wrong
  if (!nodeList || !nodeList->Item(0)) {
    doc->SetBaseURI(doc->GetDocumentURI());
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::InsertLineBreak() {
  MOZ_ASSERT(!IsSingleLineEditor());

  // XPCOM method's InsertLineBreak() should insert paragraph separator in
  // HTMLEditor.
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eInsertParagraphSeparator);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // XXX This method is called by chrome of comm-central.  So, using
  //     TypingTxnName here is odd in such case.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName);
  nsresult rv = InsertParagraphSeparatorAsSubAction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult HTMLEditor::InsertLineBreakAsAction(nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertLineBreak,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // XXX This method may be called by "insertLineBreak" command.  So, using
  //     TypingTxnName here is odd in such case.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName);
  nsresult rv = InsertBrElementAtSelectionWithTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::InsertParagraphSeparatorAsAction(
    nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eInsertParagraphSeparator, aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // XXX This may be called by execCommand() with "insertParagraph".
  //     In such case, naming the transaction "TypingTxnName" is odd.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName);
  nsresult rv = InsertParagraphSeparatorAsSubAction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult HTMLEditor::InsertParagraphSeparatorAsSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertParagraphSeparator, nsIEditor::eNext);

  EditSubActionInfo subActionInfo(EditSubAction::eInsertParagraphSeparator);
  subActionInfo.maxLength = mMaxTextLength;
  bool cancel, handled;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (cancel) {
    return rv;  // We don't need to call DidDoAction() if canceled.
  }
  // XXX DidDoAction() does nothing for eInsertParagraphSeparator.  However,
  //     we should call it until we keep using this style.  Perhaps, each
  //     editor method should call necessary method of
  //     TextEditRules/HTMLEditRules directly.
  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::TabInTable(bool inIsShift, bool* outHandled) {
  NS_ENSURE_TRUE(outHandled, NS_ERROR_NULL_POINTER);
  *outHandled = false;

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    // Do nothing if we didn't find a table cell.
    return NS_OK;
  }

  // Find enclosing table cell from selection (cell may be selected element)
  Element* cellElement = GetElementOrParentByTagNameAtSelection(*nsGkAtoms::td);
  if (NS_WARN_IF(!cellElement)) {
    // Do nothing if we didn't find a table cell.
    return NS_OK;
  }

  // find enclosing table
  RefPtr<Element> table = GetEnclosingTable(cellElement);
  if (NS_WARN_IF(!table)) {
    return NS_OK;
  }

  // advance to next cell
  // first create an iterator over the table
  PostContentIterator postOrderIter;
  nsresult rv = postOrderIter.Init(table);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  // position postOrderIter at block
  rv = postOrderIter.PositionAt(cellElement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsINode> node;
  do {
    if (inIsShift) {
      postOrderIter.Prev();
    } else {
      postOrderIter.Next();
    }

    node = postOrderIter.GetCurrentNode();

    if (node && HTMLEditUtils::IsTableCell(node) &&
        GetEnclosingTable(node) == table) {
      CollapseSelectionToDeepestNonTableFirstChild(node);
      *outHandled = true;
      return NS_OK;
    }
  } while (!postOrderIter.IsDone());

  if (!(*outHandled) && !inIsShift) {
    // If we haven't handled it yet, then we must have run off the end of the
    // table.  Insert a new row.
    // XXX We should investigate whether this behavior is supported by other
    //     browsers later.
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eInsertTableRowElement);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_FAILURE;
    }
    rv = InsertTableRowsWithTransaction(1, InsertPosition::eAfterSelectedCell);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    *outHandled = true;
    // Put selection in right place.  Use table code to get selection and index
    // to new row...
    RefPtr<Element> tblElement, cell;
    int32_t row;
    rv = GetCellContext(getter_AddRefs(tblElement), getter_AddRefs(cell),
                        nullptr, nullptr, &row, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (NS_WARN_IF(!tblElement)) {
      return NS_ERROR_FAILURE;
    }
    // ...so that we can ask for first cell in that row...
    cell = GetTableCellElementAt(*tblElement, row, 0);
    // ...and then set selection there.  (Note that normally you should use
    // CollapseSelectionToDeepestNonTableFirstChild(), but we know cell is an
    // empty new cell, so this works fine)
    if (cell) {
      SelectionRefPtr()->Collapse(cell, 0);
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::InsertBrElementAtSelectionWithTransaction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // calling it text insertion to trigger moz br treatment by rules
  // XXX Why do we use EditSubAction::eInsertText here?  Looks like
  //     EditSubAction::eInsertLineBreak or EditSubAction::eInsertNode
  //     is better.
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext);

  if (!SelectionRefPtr()->IsCollapsed()) {
    nsresult rv = DeleteSelectionAsSubAction(eNone, eStrip);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  EditorDOMPoint atStartOfSelection(
      EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // InsertBRElementWithTransaction() will set selection after the new <br>
  // element.
  RefPtr<Element> newBrElement =
      InsertBRElementWithTransaction(atStartOfSelection, eNext);
  if (NS_WARN_IF(!newBrElement)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void HTMLEditor::CollapseSelectionToDeepestNonTableFirstChild(nsINode* aNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(aNode);

  nsCOMPtr<nsINode> node = aNode;

  for (nsCOMPtr<nsIContent> child = node->GetFirstChild(); child;
       child = child->GetFirstChild()) {
    // Stop if we find a table, don't want to go into nested tables
    if (HTMLEditUtils::IsTable(child) || !IsContainer(child)) {
      break;
    }
    node = child;
  }

  SelectionRefPtr()->Collapse(node, 0);
}

nsresult HTMLEditor::ReplaceHeadContentsWithSourceWithTransaction(
    const nsAString& aSourceToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // don't do any post processing, rules get confused
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eReplaceHeadWithHTMLSource, nsIEditor::eNone);

  CommitComposition();

  // Do not use AutoTopLevelEditSubActionNotifier -- rules code won't let us
  // insert in <head>.  Use the head node as a parent and delete/insert
  // directly.
  // XXX We're using AutoTopLevelEditSubActionNotifier above...
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsContentList> nodeList =
      document->GetElementsByTagName(NS_LITERAL_STRING("head"));
  if (NS_WARN_IF(!nodeList)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> headNode = nodeList->Item(0);
  if (NS_WARN_IF(!headNode)) {
    return NS_ERROR_FAILURE;
  }

  // First, make sure there are no return chars in the source.  Bad things
  // happen if you insert returns (instead of dom newlines, \n) into an editor
  // document.
  nsAutoString inputString(aSourceToInsert);

  // Windows linebreaks: Map CRLF to LF:
  inputString.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                               NS_LITERAL_STRING("\n"));

  // Mac linebreaks: Map any remaining CR to LF:
  inputString.ReplaceSubstring(NS_LITERAL_STRING("\r"),
                               NS_LITERAL_STRING("\n"));

  AutoPlaceholderBatch treatAsOneTransaction(*this);

  // Get the first range in the selection, for context:
  RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult err;
  RefPtr<DocumentFragment> documentFragment =
      range->CreateContextualFragment(inputString, err);

  // XXXX BUG 50965: This is not returning the text between <title>...</title>
  // Special code is needed in JS to handle title anyway, so it doesn't matter!

  if (err.Failed()) {
    return err.StealNSResult();
  }
  if (NS_WARN_IF(!documentFragment)) {
    return NS_ERROR_FAILURE;
  }

  // First delete all children in head
  while (nsCOMPtr<nsIContent> child = headNode->GetFirstChild()) {
    nsresult rv = DeleteNodeWithTransaction(*child);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Now insert the new nodes
  int32_t offsetOfNewNode = 0;

  // Loop over the contents of the fragment and move into the document
  while (nsCOMPtr<nsIContent> child = documentFragment->GetFirstChild()) {
    nsresult rv = InsertNodeWithTransaction(
        *child, EditorDOMPoint(headNode, offsetOfNewNode++));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::RebuildDocumentFromSource(const nsAString& aSourceString) {
  CommitComposition();

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetHTML);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
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
  bool foundbody = CaseInsensitiveFindInReadable(NS_LITERAL_STRING("<body"),
                                                 beginbody, endbody);

  nsReadingIterator<char16_t> beginhead;
  nsReadingIterator<char16_t> endhead;
  aSourceString.BeginReading(beginhead);
  aSourceString.EndReading(endhead);
  bool foundhead = CaseInsensitiveFindInReadable(NS_LITERAL_STRING("<head"),
                                                 beginhead, endhead);
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
      NS_LITERAL_STRING("</head>"), beginclosehead, endclosehead);
  // a valid close head appears after a found head
  if (foundhead && beginhead.get() > beginclosehead.get()) {
    foundclosehead = false;
  }
  // a valid close head appears before a found body
  if (foundbody && beginclosehead.get() > beginbody.get()) {
    foundclosehead = false;
  }

  // Time to change the document
  AutoPlaceholderBatch treatAsOneTransaction(*this);

  nsReadingIterator<char16_t> endtotal;
  aSourceString.EndReading(endtotal);

  if (foundhead) {
    if (foundclosehead) {
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(
          Substring(beginhead, beginclosehead));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (foundbody) {
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(
          Substring(beginhead, beginbody));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // XXX Without recourse to some parser/content sink/docshell hackery we
      // don't really know where the head ends and the body begins so we assume
      // that there is no body
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(
          Substring(beginhead, endtotal));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  } else {
    nsReadingIterator<char16_t> begintotal;
    aSourceString.BeginReading(begintotal);
    NS_NAMED_LITERAL_STRING(head, "<head>");
    if (foundclosehead) {
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(
          head + Substring(begintotal, beginclosehead));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (foundbody) {
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(
          head + Substring(begintotal, beginbody));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // XXX Without recourse to some parser/content sink/docshell hackery we
      // don't really know where the head ends and the body begins so we assume
      // that there is no head
      nsresult rv = ReplaceHeadContentsWithSourceWithTransaction(head);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  nsresult rv = SelectAll();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!foundbody) {
    NS_NAMED_LITERAL_STRING(body, "<body>");
    // XXX Without recourse to some parser/content sink/docshell hackery we
    // don't really know where the head ends and the body begins
    if (foundclosehead) {
      // assume body starts after the head ends
      nsresult rv = LoadHTML(body + Substring(endclosehead, endtotal));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (foundhead) {
      // assume there is no body
      nsresult rv = LoadHTML(body);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // assume there is no head, the entire source is body
      nsresult rv = LoadHTML(body + aSourceString);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    RefPtr<Element> divElement = CreateElementWithDefaults(*nsGkAtoms::div);
    if (NS_WARN_IF(!divElement)) {
      return NS_ERROR_FAILURE;
    }
    CloneAttributesWithTransaction(*rootElement, *divElement);

    return MaybeCollapseSelectionAtFirstEditableNode(false);
  }

  rv = LoadHTML(Substring(beginbody, endtotal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Now we must copy attributes user might have edited on the <body> tag
  // because InsertHTML (actually, CreateContextualFragment()) will never
  // return a body node in the DOM fragment

  // We already know where "<body" begins
  nsReadingIterator<char16_t> beginclosebody = beginbody;
  nsReadingIterator<char16_t> endclosebody;
  aSourceString.EndReading(endclosebody);
  if (!FindInReadable(NS_LITERAL_STRING(">"), beginclosebody, endclosebody)) {
    return NS_ERROR_FAILURE;
  }

  // Truncate at the end of the body tag.  Kludge of the year: fool the parser
  // by replacing "body" with "div" so we get a node
  nsAutoString bodyTag;
  bodyTag.AssignLiteral("<div ");
  bodyTag.Append(Substring(endbody, endclosebody));

  RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult erv;
  RefPtr<DocumentFragment> docfrag =
      range->CreateContextualFragment(bodyTag, erv);
  NS_ENSURE_TRUE(!erv.Failed(), erv.StealNSResult());
  NS_ENSURE_TRUE(docfrag, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> child = docfrag->GetFirstChild();
  NS_ENSURE_TRUE(child && child->IsElement(), NS_ERROR_NULL_POINTER);

  // Copy all attributes from the div child to current body element
  CloneAttributesWithTransaction(*rootElement,
                                 MOZ_KnownLive(*child->AsElement()));

  // place selection at first editable content
  return MaybeCollapseSelectionAtFirstEditableNode(false);
}

EditorRawDOMPoint HTMLEditor::GetBetterInsertionPointFor(
    nsINode& aNodeToInsert, const EditorRawDOMPoint& aPointToInsert) {
  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return aPointToInsert;
  }

  EditorRawDOMPoint pointToInsert(aPointToInsert.GetNonAnonymousSubtreePoint());
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    // Cannot insert aNodeToInsert into this DOM tree.
    return EditorRawDOMPoint();
  }

  // If the node to insert is not a block level element, we can insert it
  // at any point.
  if (!IsBlockNode(&aNodeToInsert)) {
    return pointToInsert;
  }

  WSRunObject wsObj(this, pointToInsert.GetContainer(), pointToInsert.Offset());

  // If the insertion position is after the last visible item in a line,
  // i.e., the insertion position is just before a visible line break <br>,
  // we want to skip to the position just after the line break (see bug 68767).
  nsCOMPtr<nsINode> nextVisibleNode;
  WSType nextVisibleType;
  wsObj.NextVisibleNode(pointToInsert, address_of(nextVisibleNode), nullptr,
                        &nextVisibleType);
  // So, if the next visible node isn't a <br> element, we can insert the block
  // level element to the point.
  if (!nextVisibleNode || !(nextVisibleType & WSType::br)) {
    return pointToInsert;
  }

  // However, we must not skip next <br> element when the caret appears to be
  // positioned at the beginning of a block, in that case skipping the <br>
  // would not insert the <br> at the caret position, but after the current
  // empty line.
  nsCOMPtr<nsINode> previousVisibleNode;
  WSType previousVisibleType;
  wsObj.PriorVisibleNode(pointToInsert, address_of(previousVisibleNode),
                         nullptr, &previousVisibleType);
  // So, if there is no previous visible node,
  // or, if both nodes of the insertion point is <br> elements,
  // or, if the previous visible node is different block,
  // we need to skip the following <br>.  So, otherwise, we can insert the
  // block at the insertion point.
  if (!previousVisibleNode || (previousVisibleType & WSType::br) ||
      (previousVisibleType & WSType::thisBlock)) {
    return pointToInsert;
  }

  EditorRawDOMPoint afterBRNode(nextVisibleNode);
  DebugOnly<bool> advanced = afterBRNode.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced,
                       "Failed to advance offset to after the <br> node");
  return afterBRNode;
}

NS_IMETHODIMP
HTMLEditor::InsertElementAtSelection(Element* aElement, bool aDeleteSelection) {
  nsresult rv = InsertElementAtSelectionAsAction(aElement, aDeleteSelection);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Failed to insert element at selection");
  return rv;
}

nsresult HTMLEditor::InsertElementAtSelectionAsAction(
    Element* aElement, bool aDeleteSelection, nsIPrincipal* aPrincipal) {
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(
      *this, HTMLEditUtils::GetEditActionForInsert(*aElement), aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  CommitComposition();
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertElement, nsIEditor::eNext);

  // hand off to the rules system, see if it has anything to say about this
  bool cancel, handled;
  EditSubActionInfo subActionInfo(EditSubAction::eInsertElement);
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled) {
    if (aDeleteSelection) {
      if (!IsBlockNode(aElement)) {
        // E.g., inserting an image.  In this case we don't need to delete any
        // inline wrappers before we do the insertion.  Otherwise we let
        // DeleteSelectionAndPrepareToCreateNode do the deletion for us, which
        // calls DeleteSelection with aStripWrappers = eStrip.
        rv = DeleteSelectionAsSubAction(eNone, eNoStrip);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      nsresult rv = DeleteSelectionAndPrepareToCreateNode();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // If deleting, selection will be collapsed.
    // so if not, we collapse it
    if (!aDeleteSelection) {
      // Named Anchor is a special case,
      // We collapse to insert element BEFORE the selection
      // For all other tags, we insert AFTER the selection
      if (HTMLEditUtils::IsNamedAnchor(aElement)) {
        SelectionRefPtr()->CollapseToStart(IgnoreErrors());
      } else {
        SelectionRefPtr()->CollapseToEnd(IgnoreErrors());
      }
    }

    if (SelectionRefPtr()->GetAnchorNode()) {
      EditorRawDOMPoint atAnchor(SelectionRefPtr()->AnchorRef());
      // Adjust position based on the node we are going to insert.
      EditorDOMPoint pointToInsert =
          GetBetterInsertionPointFor(*aElement, atAnchor);
      if (NS_WARN_IF(!pointToInsert.IsSet())) {
        return NS_ERROR_FAILURE;
      }

      EditorDOMPoint insertedPoint =
          InsertNodeIntoProperAncestorWithTransaction(
              *aElement, pointToInsert,
              SplitAtEdges::eAllowToCreateEmptyContainer);
      if (NS_WARN_IF(!insertedPoint.IsSet())) {
        return NS_ERROR_FAILURE;
      }
      // Set caret after element, but check for special case
      //  of inserting table-related elements: set in first cell instead
      if (!SetCaretInTableCell(aElement)) {
        rv = CollapseSelectionAfter(*aElement);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      // check for inserting a whole table at the end of a block. If so insert
      // a br after it.
      if (HTMLEditUtils::IsTable(aElement) && IsLastEditableChild(aElement)) {
        DebugOnly<bool> advanced = insertedPoint.AdvanceOffset();
        NS_WARNING_ASSERTION(advanced,
                             "Failed to advance offset from inserted point");
        // Collapse selection to the new <br> element node after creating it.
        RefPtr<Element> newBrElement =
            InsertBRElementWithTransaction(insertedPoint, ePrevious);
        if (NS_WARN_IF(!newBrElement)) {
          return NS_ERROR_FAILURE;
        }
      }
    }
  }
  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
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
  while (!CanContain(*pointToInsert.GetContainer(), aNode)) {
    // If the current parent is a root (body or table element)
    // then go no further - we can't insert.
    if (pointToInsert.IsContainerHTMLElement(nsGkAtoms::body) ||
        HTMLEditUtils::IsTableElement(pointToInsert.GetContainer())) {
      return EditorDOMPoint();
    }

    // Get the next point.
    pointToInsert.Set(pointToInsert.GetContainer());
    if (NS_WARN_IF(!pointToInsert.IsSet())) {
      return EditorDOMPoint();
    }

    if (!IsEditable(pointToInsert.GetContainer())) {
      // There's no suitable place to put the node in this editing host.  Maybe
      // someone is trying to put block content in a span.  So just put it
      // where we were originally asked.
      pointToInsert = aPointToInsert;
      break;
    }
  }

  if (pointToInsert != aPointToInsert) {
    // We need to split some levels above the original selection parent.
    MOZ_ASSERT(pointToInsert.GetChild());
    SplitNodeResult splitNodeResult =
        SplitNodeDeepWithTransaction(MOZ_KnownLive(*pointToInsert.GetChild()),
                                     aPointToInsert, aSplitAtEdges);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
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
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorDOMPoint();
    }
  }
  return pointToInsert;
}

NS_IMETHODIMP
HTMLEditor::SelectElement(Element* aElement) {
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = SelectContentInternal(MOZ_KnownLive(*aElement));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::SelectContentInternal(nsIContent& aContentToSelect) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Must be sure that element is contained in the document body
  if (!IsDescendantOfEditorRoot(&aContentToSelect)) {
    return NS_ERROR_FAILURE;
  }

  EditorRawDOMPoint newSelectionStart(&aContentToSelect);
  if (NS_WARN_IF(!newSelectionStart.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  EditorRawDOMPoint newSelectionEnd(&aContentToSelect);
  MOZ_ASSERT(newSelectionEnd.IsSet());
  DebugOnly<bool> advanced = newSelectionEnd.AdvanceOffset();
  ErrorResult error;
  MOZ_KnownLive(SelectionRefPtr())
      ->SetStartAndEndInLimiter(newSelectionStart, newSelectionEnd, error);
  NS_WARNING_ASSERTION(!error.Failed(), "Failed to select the given content");
  return error.StealNSResult();
}

NS_IMETHODIMP
HTMLEditor::SetCaretAfterElement(Element* aElement) {
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = CollapseSelectionAfter(*aElement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::CollapseSelectionAfter(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Be sure the element is contained in the document body
  if (NS_WARN_IF(!IsDescendantOfEditorRoot(&aElement))) {
    return NS_ERROR_INVALID_ARG;
  }
  nsINode* parent = aElement.GetParentNode();
  if (NS_WARN_IF(!parent)) {
    return NS_ERROR_FAILURE;
  }
  // Collapse selection to just after desired element,
  EditorRawDOMPoint afterElement(&aElement);
  if (NS_WARN_IF(!afterElement.AdvanceOffset())) {
    return NS_ERROR_FAILURE;
  }
  ErrorResult error;
  SelectionRefPtr()->Collapse(afterElement, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetParagraphFormat(const nsAString& aParagraphFormat) {
  nsresult rv = SetParagraphFormatAsAction(aParagraphFormat);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to set paragraph format");
  return rv;
}

nsresult HTMLEditor::SetParagraphFormatAsAction(
    const nsAString& aParagraphFormat, nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eInsertBlockElement, aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsAutoString lowerCaseTagName(aParagraphFormat);
  ToLowerCase(lowerCaseTagName);
  RefPtr<nsAtom> tagName = NS_Atomize(lowerCaseTagName);
  MOZ_ASSERT(tagName);
  if (tagName == nsGkAtoms::dd || tagName == nsGkAtoms::dt) {
    nsresult rv = MakeDefinitionListItemWithTransaction(*tagName);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "MakeDefinitionListItemWithTransaction() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  nsresult rv = InsertBasicBlockWithTransaction(*tagName);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "InsertBasicBlockWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP
HTMLEditor::GetParagraphState(bool* aMixed, nsAString& outFormat) {
  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<HTMLEditRules> htmlRules(mRules->AsHTMLEditRules());
  return htmlRules->GetParagraphState(aMixed, outFormat);
}

NS_IMETHODIMP
HTMLEditor::GetBackgroundColorState(bool* aMixed, nsAString& aOutColor) {
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
    return GetCSSBackgroundColorState(aMixed, aOutColor, true);
  }
  // in HTML mode, we look only at page's background
  return GetHTMLBackgroundColorState(aMixed, aOutColor);
}

NS_IMETHODIMP
HTMLEditor::GetHighlightColorState(bool* aMixed, nsAString& aOutColor) {
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
  return GetCSSBackgroundColorState(aMixed, aOutColor, false);
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

  RefPtr<nsRange> firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> parent = firstRange->GetStartContainer();
  if (NS_WARN_IF(!parent)) {
    return NS_ERROR_FAILURE;
  }

  // is the selection collapsed?
  nsCOMPtr<nsINode> nodeToExamine;
  if (SelectionRefPtr()->IsCollapsed() || IsTextNode(parent)) {
    // we want to look at the parent and ancestors
    nodeToExamine = parent;
  } else {
    // otherwise we want to look at the first editable node after
    // {parent,offset} and its ancestors for divs with alignment on them
    nodeToExamine = firstRange->GetChildAtStartOffset();
    // GetNextNode(parent, offset, true, address_of(nodeToExamine));
  }

  if (NS_WARN_IF(!nodeToExamine)) {
    return NS_ERROR_FAILURE;
  }

  if (aBlockLevel) {
    // we are querying the block background (and not the text background), let's
    // climb to the block container
    nsCOMPtr<Element> blockParent = GetBlock(*nodeToExamine);
    if (NS_WARN_IF(!blockParent)) {
      return NS_OK;
    }

    // Make sure to not walk off onto the Document node
    do {
      // retrieve the computed style of background-color for blockParent
      CSSEditUtils::GetComputedProperty(*blockParent,
                                        *nsGkAtoms::backgroundColor, aOutColor);
      blockParent = blockParent->GetParentElement();
      // look at parent if the queried color is transparent and if the node to
      // examine is not the root of the document
    } while (aOutColor.EqualsLiteral("transparent") && blockParent);
    if (aOutColor.EqualsLiteral("transparent")) {
      // we have hit the root of the document and the color is still transparent
      // ! Grumble... Let's look at the default background color because that's
      // the color we are looking for
      CSSEditUtils::GetDefaultBackgroundColor(aOutColor);
    }
  } else {
    // no, we are querying the text background for the Text Highlight button
    if (IsTextNode(nodeToExamine)) {
      // if the node of interest is a text node, let's climb a level
      nodeToExamine = nodeToExamine->GetParentNode();
    }
    // Return default value due to no parent node
    if (!nodeToExamine) {
      return NS_OK;
    }
    do {
      // is the node to examine a block ?
      if (NodeIsBlockStatic(nodeToExamine)) {
        // yes it is a block; in that case, the text background color is
        // transparent
        aOutColor.AssignLiteral("transparent");
        break;
      } else {
        // no, it's not; let's retrieve the computed style of background-color
        // for the node to examine
        CSSEditUtils::GetComputedProperty(
            *nodeToExamine, *nsGkAtoms::backgroundColor, aOutColor);
        if (!aOutColor.EqualsLiteral("transparent")) {
          break;
        }
      }
      nodeToExamine = nodeToExamine->GetParentNode();
    } while (aOutColor.EqualsLiteral("transparent") && nodeToExamine);
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
  if (NS_WARN_IF(error.Failed())) {
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
  Element* bodyElement = GetRoot();
  if (NS_WARN_IF(!bodyElement)) {
    return NS_ERROR_FAILURE;
  }

  bodyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::bgcolor, aOutColor);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetListState(bool* aMixed, bool* aOL, bool* aUL, bool* aDL) {
  if (NS_WARN_IF(!aMixed) || NS_WARN_IF(!aOL) || NS_WARN_IF(!aUL) ||
      NS_WARN_IF(!aDL)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<HTMLEditRules> htmlRules(mRules->AsHTMLEditRules());
  return htmlRules->GetListState(aMixed, aOL, aUL, aDL);
}

NS_IMETHODIMP
HTMLEditor::GetListItemState(bool* aMixed, bool* aLI, bool* aDT, bool* aDD) {
  if (NS_WARN_IF(!aMixed) || NS_WARN_IF(!aLI) || NS_WARN_IF(!aDT) ||
      NS_WARN_IF(!aDD)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<HTMLEditRules> htmlRules(mRules->AsHTMLEditRules());
  return htmlRules->GetListItemState(aMixed, aLI, aDT, aDD);
}

NS_IMETHODIMP
HTMLEditor::GetAlignment(bool* aMixed, nsIHTMLEditor::EAlignment* aAlign) {
  if (NS_WARN_IF(!aMixed) || NS_WARN_IF(!aAlign)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<HTMLEditRules> htmlRules(mRules->AsHTMLEditRules());
  return htmlRules->GetAlignment(aMixed, aAlign);
}

NS_IMETHODIMP
HTMLEditor::MakeOrChangeList(const nsAString& aListType, bool entireList,
                             const nsAString& aBulletType) {
  nsresult rv = MakeOrChangeListAsAction(aListType, entireList, aBulletType);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to make or change list");
  return rv;
}

nsresult HTMLEditor::MakeOrChangeListAsAction(const nsAString& aListType,
                                              bool entireList,
                                              const nsAString& aBulletType,
                                              nsIPrincipal* aPrincipal) {
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsAtom> listAtom = NS_Atomize(aListType);
  if (NS_WARN_IF(!listAtom)) {
    return NS_ERROR_INVALID_ARG;
  }
  AutoEditActionDataSetter editActionData(
      *this, HTMLEditUtils::GetEditActionForInsert(*listAtom), aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  bool cancel, handled;

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eCreateOrChangeList, nsIEditor::eNext);

  EditSubActionInfo subActionInfo(EditSubAction::eCreateOrChangeList);
  subActionInfo.blockType = &aListType;
  subActionInfo.entireList = entireList;
  subActionInfo.bulletType = &aBulletType;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return EditorBase::ToGenericNSResult(rv);
  }

  if (!handled && SelectionRefPtr()->IsCollapsed()) {
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet()) ||
        NS_WARN_IF(!atStartOfSelection.GetContainerAsContent())) {
      return NS_ERROR_FAILURE;
    }

    // Have to find a place to put the list.
    EditorDOMPoint pointToInsertList(atStartOfSelection);

    while (!CanContainTag(*pointToInsertList.GetContainer(), *listAtom)) {
      pointToInsertList.Set(pointToInsertList.GetContainer());
      if (NS_WARN_IF(!pointToInsertList.IsSet()) ||
          NS_WARN_IF(!pointToInsertList.GetContainerAsContent())) {
        return NS_ERROR_FAILURE;
      }
    }

    if (pointToInsertList.GetContainer() != atStartOfSelection.GetContainer()) {
      // We need to split up to the child of parent.
      SplitNodeResult splitNodeResult = SplitNodeDeepWithTransaction(
          MOZ_KnownLive(*pointToInsertList.GetChild()), atStartOfSelection,
          SplitAtEdges::eAllowToCreateEmptyContainer);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return EditorBase::ToGenericNSResult(splitNodeResult.Rv());
      }
      pointToInsertList = splitNodeResult.SplitPoint();
      if (NS_WARN_IF(!pointToInsertList.IsSet())) {
        return NS_ERROR_FAILURE;
      }
    }

    // Create a list and insert it before the right node if we split some
    // parents of start of selection above, or just start of selection
    // otherwise.
    RefPtr<Element> newList =
        CreateNodeWithTransaction(*listAtom, pointToInsertList);
    if (NS_WARN_IF(!newList)) {
      return NS_ERROR_FAILURE;
    }
    // make a list item
    RefPtr<Element> newItem =
        CreateNodeWithTransaction(*nsGkAtoms::li, EditorDOMPoint(newList, 0));
    if (NS_WARN_IF(!newItem)) {
      return NS_ERROR_FAILURE;
    }
    ErrorResult error;
    SelectionRefPtr()->Collapse(RawRangeBoundary(newItem, 0), error);
    if (NS_WARN_IF(error.Failed())) {
      return EditorBase::ToGenericNSResult(error.StealNSResult());
    }
  }

  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::RemoveList(const nsAString& aListType) {
  nsresult rv = RemoveListAsAction(aListType);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to remove list");
  return rv;
}

nsresult HTMLEditor::RemoveListAsAction(const nsAString& aListType,
                                        nsIPrincipal* aPrincipal) {
  if (!mRules) {
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
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eRemoveList, nsIEditor::eNext);

  EditSubActionInfo subActionInfo(EditSubAction::eRemoveList);
  bool cancel, handled;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return EditorBase::ToGenericNSResult(rv);
  }

  // no default behavior for this yet.  what would it mean?

  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult HTMLEditor::MakeDefinitionListItemWithTransaction(nsAtom& aTagName) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(&aTagName == nsGkAtoms::dt || &aTagName == nsGkAtoms::dd);

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  bool cancel, handled;

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eCreateOrChangeDefinitionList, nsIEditor::eNext);

  nsDependentAtomString tagName(&aTagName);
  EditSubActionInfo subActionInfo(EditSubAction::eCreateOrChangeDefinitionList);
  subActionInfo.blockType = &tagName;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled) {
    // todo: no default for now.  we count on rules to handle it.
  }

  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::InsertBasicBlockWithTransaction(nsAtom& aTagName) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(&aTagName != nsGkAtoms::dd && &aTagName != nsGkAtoms::dt);

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  bool cancel, handled;

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eCreateOrRemoveBlock, nsIEditor::eNext);

  nsDependentAtomString tagName(&aTagName);
  EditSubActionInfo subActionInfo(EditSubAction::eCreateOrRemoveBlock);
  subActionInfo.blockType = &tagName;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled && SelectionRefPtr()->IsCollapsed()) {
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet()) ||
        NS_WARN_IF(!atStartOfSelection.GetContainerAsContent())) {
      return NS_ERROR_FAILURE;
    }

    // Have to find a place to put the block.
    EditorDOMPoint pointToInsertBlock(atStartOfSelection);

    while (!CanContainTag(*pointToInsertBlock.GetContainer(), aTagName)) {
      pointToInsertBlock.Set(pointToInsertBlock.GetContainer());
      if (NS_WARN_IF(!pointToInsertBlock.IsSet()) ||
          NS_WARN_IF(!pointToInsertBlock.GetContainerAsContent())) {
        return NS_ERROR_FAILURE;
      }
    }

    if (pointToInsertBlock.GetContainer() !=
        atStartOfSelection.GetContainer()) {
      // We need to split up to the child of the point to insert a block.
      SplitNodeResult splitBlockResult = SplitNodeDeepWithTransaction(
          MOZ_KnownLive(*pointToInsertBlock.GetChild()), atStartOfSelection,
          SplitAtEdges::eAllowToCreateEmptyContainer);
      if (NS_WARN_IF(splitBlockResult.Failed())) {
        return splitBlockResult.Rv();
      }
      pointToInsertBlock = splitBlockResult.SplitPoint();
      if (NS_WARN_IF(!pointToInsertBlock.IsSet())) {
        return NS_ERROR_FAILURE;
      }
    }

    // Create a block and insert it before the right node if we split some
    // parents of start of selection above, or just start of selection
    // otherwise.
    RefPtr<Element> newBlock =
        CreateNodeWithTransaction(aTagName, pointToInsertBlock);
    if (NS_WARN_IF(!newBlock)) {
      return NS_ERROR_FAILURE;
    }

    // reposition selection to inside the block
    ErrorResult error;
    SelectionRefPtr()->Collapse(RawRangeBoundary(newBlock, 0), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::Indent(const nsAString& aIndent) {
  if (aIndent.LowerCaseEqualsLiteral("indent")) {
    nsresult rv = IndentAsAction();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to indent");
    return rv;
  }
  if (aIndent.LowerCaseEqualsLiteral("outdent")) {
    nsresult rv = OutdentAsAction();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to outdent");
    return rv;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult HTMLEditor::IndentAsAction(nsIPrincipal* aPrincipal) {
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eIndent,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  nsresult rv = IndentOrOutdentAsSubAction(EditSubAction::eIndent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult HTMLEditor::OutdentAsAction(nsIPrincipal* aPrincipal) {
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eOutdent,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  nsresult rv = IndentOrOutdentAsSubAction(EditSubAction::eOutdent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult HTMLEditor::IndentOrOutdentAsSubAction(
    EditSubAction aIndentOrOutdent) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mRules);
  MOZ_ASSERT(mPlaceholderBatch);

  MOZ_ASSERT(aIndentOrOutdent == EditSubAction::eIndent ||
             aIndentOrOutdent == EditSubAction::eOutdent);

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  bool cancel, handled;
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, aIndentOrOutdent, nsIEditor::eNext);

  EditSubActionInfo subActionInfo(aIndentOrOutdent);
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled && SelectionRefPtr()->IsCollapsed() &&
      aIndentOrOutdent == EditSubAction::eIndent) {
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet()) ||
        NS_WARN_IF(!atStartOfSelection.GetContainerAsContent())) {
      return NS_ERROR_FAILURE;
    }

    // Have to find a place to put the blockquote.
    EditorDOMPoint pointToInsertBlockquote(atStartOfSelection);

    while (!CanContainTag(*pointToInsertBlockquote.GetContainer(),
                          *nsGkAtoms::blockquote)) {
      pointToInsertBlockquote.Set(pointToInsertBlockquote.GetContainer());
      if (NS_WARN_IF(!pointToInsertBlockquote.IsSet()) ||
          NS_WARN_IF(!pointToInsertBlockquote.GetContainerAsContent())) {
        return NS_ERROR_FAILURE;
      }
    }

    if (pointToInsertBlockquote.GetContainer() !=
        atStartOfSelection.GetContainer()) {
      // We need to split up to the child of parent.
      SplitNodeResult splitBlockquoteResult = SplitNodeDeepWithTransaction(
          MOZ_KnownLive(*pointToInsertBlockquote.GetChild()),
          atStartOfSelection, SplitAtEdges::eAllowToCreateEmptyContainer);
      if (NS_WARN_IF(splitBlockquoteResult.Failed())) {
        return splitBlockquoteResult.Rv();
      }
      pointToInsertBlockquote = splitBlockquoteResult.SplitPoint();
      if (NS_WARN_IF(!pointToInsertBlockquote.IsSet())) {
        return NS_ERROR_FAILURE;
      }
    }

    // Create a list and insert it before the right node if we split some
    // parents of start of selection above, or just start of selection
    // otherwise.
    RefPtr<Element> newBQ = CreateNodeWithTransaction(*nsGkAtoms::blockquote,
                                                      pointToInsertBlockquote);
    if (NS_WARN_IF(!newBQ)) {
      return NS_ERROR_FAILURE;
    }
    // put a space in it so layout will draw the list item
    ErrorResult error;
    SelectionRefPtr()->Collapse(RawRangeBoundary(newBQ, 0), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    rv = InsertTextAsSubAction(NS_LITERAL_STRING(" "));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Reposition selection to before the space character.
    firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }
    SelectionRefPtr()->Collapse(
        RawRangeBoundary(firstRange->GetStartContainer(), 0), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }
  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

// TODO: IMPLEMENT ALIGNMENT!

NS_IMETHODIMP
HTMLEditor::Align(const nsAString& aAlignType) {
  nsresult rv = AlignAsAction(aAlignType);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to align content");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::AlignAsAction(const nsAString& aAlignType,
                                   nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this, HTMLEditUtils::GetEditActionForAlignment(aAlignType), aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eSetOrClearAlignment, nsIEditor::eNext);

  bool cancel, handled;

  // Find out if the selection is collapsed:
  EditSubActionInfo subActionInfo(EditSubAction::eSetOrClearAlignment);
  subActionInfo.alignType = &aAlignType;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

Element* HTMLEditor::GetElementOrParentByTagName(const nsAtom& aTagName,
                                                 nsINode* aNode) const {
  MOZ_ASSERT(&aTagName != nsGkAtoms::_empty);

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return nullptr;
  }

  if (aNode) {
    return GetElementOrParentByTagNameInternal(aTagName, *aNode);
  }
  return GetElementOrParentByTagNameAtSelection(aTagName);
}

Element* HTMLEditor::GetElementOrParentByTagNameAtSelection(
    const nsAtom& aTagName) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(&aTagName != nsGkAtoms::_empty);

  // If no node supplied, get it from anchor node of current selection
  const EditorRawDOMPoint atAnchor(SelectionRefPtr()->AnchorRef());
  if (NS_WARN_IF(!atAnchor.IsSet())) {
    return nullptr;
  }

  // Try to get the actual selected node
  nsCOMPtr<nsINode> node;
  if (atAnchor.GetContainer()->HasChildNodes() &&
      atAnchor.GetContainerAsContent()) {
    node = atAnchor.GetChild();
  }
  // Anchor node is probably a text node - just use that
  if (!node) {
    node = atAnchor.GetContainer();
    if (NS_WARN_IF(!node)) {
      return nullptr;
    }
  }
  return GetElementOrParentByTagNameInternal(aTagName, *node);
}

Element* HTMLEditor::GetElementOrParentByTagNameInternal(const nsAtom& aTagName,
                                                         nsINode& aNode) const {
  MOZ_ASSERT(&aTagName != nsGkAtoms::_empty);

  Element* currentElement =
      aNode.IsElement() ? aNode.AsElement() : aNode.GetParentElement();
  if (NS_WARN_IF(!currentElement)) {
    // Neither aNode nor its parent is an element, so no ancestor is
    MOZ_ASSERT(!aNode.GetParentNode() ||
               !aNode.GetParentNode()->GetParentNode());
    return nullptr;
  }

  bool getLink = IsLinkTag(aTagName);
  bool getNamedAnchor = IsNamedAnchorTag(aTagName);
  for (; currentElement; currentElement = currentElement->GetParentElement()) {
    if (getLink) {
      // Test if we have a link (an anchor with href set)
      if (HTMLEditUtils::IsLink(currentElement)) {
        return currentElement;
      }
    } else if (getNamedAnchor) {
      // Test if we have a named anchor (an anchor with name set)
      if (HTMLEditUtils::IsNamedAnchor(currentElement)) {
        return currentElement;
      }
    } else if (&aTagName == nsGkAtoms::list_) {
      // Match "ol", "ul", or "dl" for lists
      if (HTMLEditUtils::IsList(currentElement)) {
        return currentElement;
      }
    } else if (&aTagName == nsGkAtoms::td) {
      // Table cells are another special case: match either "td" or "th"
      if (HTMLEditUtils::IsTableCell(currentElement)) {
        return currentElement;
      }
    } else if (&aTagName == currentElement->NodeInfo()->NameAtom()) {
      return currentElement;
    }

    // Stop searching if parent is a body tag.  Note: Originally used IsRoot to
    // stop at table cells, but that's too messy when you are trying to find
    // the parent table
    if (currentElement->GetParentElement() &&
        currentElement->GetParentElement()->IsHTMLElement(nsGkAtoms::body)) {
      break;
    }
  }

  return nullptr;
}

NS_IMETHODIMP
HTMLEditor::GetElementOrParentByTagName(const nsAString& aTagName,
                                        nsINode* aNode, Element** aReturn) {
  if (NS_WARN_IF(aTagName.IsEmpty()) || NS_WARN_IF(!aReturn)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<nsAtom> tagName = GetLowerCaseNameAtom(aTagName);
  if (NS_WARN_IF(!tagName) || NS_WARN_IF(tagName == nsGkAtoms::_empty)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Element> parent = GetElementOrParentByTagName(*tagName, aNode);
  if (!parent) {
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }
  parent.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetSelectedElement(const nsAString& aTagName,
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
  RefPtr<nsAtom> tagName = GetLowerCaseNameAtom(aTagName);
  RefPtr<nsINode> selectedNode = GetSelectedElement(tagName, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  selectedNode.forget(aReturn);
  return NS_OK;
}

already_AddRefed<Element> HTMLEditor::GetSelectedElement(const nsAtom* aTagName,
                                                         ErrorResult& aRv) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(!aRv.Failed());

  // If there is no Selection or two or more selection ranges, that means that
  // not only one element is selected so that return nullptr.
  if (SelectionRefPtr()->RangeCount() != 1) {
    return nullptr;
  }

  bool isLinkTag = aTagName && IsLinkTag(*aTagName);
  bool isNamedAnchorTag = aTagName && IsNamedAnchorTag(*aTagName);

  RefPtr<nsRange> firstRange = SelectionRefPtr()->GetRangeAt(0);
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

  if (isLinkTag) {
    // Link node must be the same for both ends of selection.
    Element* parentLinkOfStart = GetElementOrParentByTagNameInternal(
        *nsGkAtoms::href, *startRef.Container());
    if (parentLinkOfStart) {
      if (SelectionRefPtr()->IsCollapsed()) {
        // We have just a caret in the link.
        return do_AddRef(parentLinkOfStart);
      }
      // Link node must be the same for both ends of selection.
      Element* parentLinkOfEnd = GetElementOrParentByTagNameInternal(
          *nsGkAtoms::href, *endRef.Container());
      if (parentLinkOfStart == parentLinkOfEnd) {
        return do_AddRef(parentLinkOfStart);
      }
    }
  }

  if (SelectionRefPtr()->IsCollapsed()) {
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
    if (lastElementInRange->GetNextSibling() &&
        lastElementInRange->GetNextSibling()->IsHTMLElement(nsGkAtoms::br)) {
      return nullptr;
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
  IgnoredErrorResult rv;
  newElement->SetAttribute(NS_LITERAL_STRING("_moz_dirty"), EmptyString(), rv);

  // Set default values for new elements
  if (realTagName == nsGkAtoms::table) {
    newElement->SetAttribute(NS_LITERAL_STRING("cellpadding"),
                             NS_LITERAL_STRING("2"), rv);
    if (NS_WARN_IF(rv.Failed())) {
      return nullptr;
    }
    newElement->SetAttribute(NS_LITERAL_STRING("cellspacing"),
                             NS_LITERAL_STRING("2"), rv);
    if (NS_WARN_IF(rv.Failed())) {
      return nullptr;
    }
    newElement->SetAttribute(NS_LITERAL_STRING("border"),
                             NS_LITERAL_STRING("1"), rv);
    if (NS_WARN_IF(rv.Failed())) {
      return nullptr;
    }
  } else if (realTagName == nsGkAtoms::td) {
    nsresult rv = SetAttributeOrEquivalent(newElement, nsGkAtoms::valign,
                                           NS_LITERAL_STRING("top"), true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }
  // ADD OTHER TAGS HERE

  return newElement.forget();
}

NS_IMETHODIMP
HTMLEditor::CreateElementWithDefaults(const nsAString& aTagName,
                                      Element** aReturn) {
  if (NS_WARN_IF(aTagName.IsEmpty()) || NS_WARN_IF(!aReturn)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aReturn = nullptr;

  RefPtr<nsAtom> tagName = GetLowerCaseNameAtom(aTagName);
  if (NS_WARN_IF(!tagName)) {
    return NS_ERROR_INVALID_ARG;
  }
  RefPtr<Element> newElement = CreateElementWithDefaults(*tagName);
  if (NS_WARN_IF(!newElement)) {
    return NS_ERROR_FAILURE;
  }
  newElement.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::InsertLinkAroundSelection(Element* aAnchorElement) {
  nsresult rv = InsertLinkAroundSelectionAsAction(aAnchorElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Failed to insert link around selection");
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

  if (NS_WARN_IF(SelectionRefPtr()->IsCollapsed())) {
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

  nsAutoString href;
  anchor->GetHref(href);
  if (href.IsEmpty()) {
    return NS_OK;
  }

  nsresult rv;
  AutoPlaceholderBatch treatAsOneTransaction(*this);

  // Set all attributes found on the supplied anchor element
  RefPtr<nsDOMAttributeMap> attrMap = anchor->Attributes();
  NS_ENSURE_TRUE(attrMap, NS_ERROR_FAILURE);

  uint32_t count = attrMap->Length();
  nsAutoString value;

  for (uint32_t i = 0; i < count; ++i) {
    RefPtr<Attr> attribute = attrMap->Item(i);

    if (attribute) {
      // We must clear the string buffers
      //   because GetValue appends to previous string!
      value.Truncate();

      nsAtom* name = attribute->NodeInfo()->NameAtom();

      attribute->GetValue(value);

      rv = SetInlinePropertyInternal(*nsGkAtoms::a, MOZ_KnownLive(name), value);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
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
  if (NS_WARN_IF(error.Failed())) {
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
      IgnoredErrorResult ignoredError;
      RefPtr<Element> cellElement =
          GetFirstSelectedTableCellElement(ignoredError);
      if (cellElement) {
        if (setColor) {
          while (cellElement) {
            nsresult rv = SetAttributeWithTransaction(
                *cellElement, *nsGkAtoms::bgcolor, aColor);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
            cellElement = GetNextSelectedTableCellElement(ignoredError);
          }
          return NS_OK;
        }
        while (cellElement) {
          nsresult rv =
              RemoveAttributeWithTransaction(*cellElement, *nsGkAtoms::bgcolor);
          if (NS_FAILED(rv)) {
            return rv;
          }
          cellElement = GetNextSelectedTableCellElement(ignoredError);
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
  return setColor ? SetAttributeWithTransaction(*rootElementOfBackgroundColor,
                                                *nsGkAtoms::bgcolor, aColor)
                  : RemoveAttributeWithTransaction(
                        *rootElementOfBackgroundColor, *nsGkAtoms::bgcolor);
}

NS_IMETHODIMP
HTMLEditor::GetLinkedObjects(nsIArray** aNodeList) {
  NS_ENSURE_TRUE(aNodeList, NS_ERROR_NULL_POINTER);

  nsresult rv;
  nsCOMPtr<nsIMutableArray> nodes = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<Document> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  PostContentIterator postOrderIter;
  postOrderIter.Init(doc->GetRootElement());

  // loop through the content iterator for each content node
  for (; !postOrderIter.IsDone(); postOrderIter.Next()) {
    nsCOMPtr<nsINode> node = postOrderIter.GetCurrentNode();
    if (node) {
      // Let nsURIRefObject make the hard decisions:
      nsCOMPtr<nsIURIRefObject> refObject;
      rv = NS_NewHTMLURIRefObject(getter_AddRefs(refObject), node);
      if (NS_SUCCEEDED(rv)) {
        nodes->AppendElement(refObject);
      }
    }
  }

  nodes.forget(aNodeList);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::AddOverrideStyleSheet(const nsAString& aURL) {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eAddOverrideStyleSheet);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = AddOverrideStyleSheetInternal(aURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::AddOverrideStyleSheetInternal(const nsAString& aURL) {
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL)) {
    return NS_OK;
  }

  // Make sure the pres shell doesn't disappear during the load.
  RefPtr<PresShell> presShell = GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIURI> uaURI;
  nsresult rv = NS_NewURI(getter_AddRefs(uaURI), aURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We MUST ONLY load synchronous local files (no @import)
  // XXXbz Except this will actually try to load remote files
  // synchronously, of course..
  // Editor override style sheets may want to style Gecko anonymous boxes
  auto result = presShell->GetDocument()->CSSLoader()->LoadSheetSync(
      uaURI, css::eAgentSheetFeatures, css::Loader::UseSystemPrincipal::Yes);
  // Synchronous loads should ALWAYS return completed
  if (NS_WARN_IF(result.isErr())) {
    return result.unwrapErr();
  }

  RefPtr<StyleSheet> sheet = result.unwrap();

  // Add the override style sheet
  // (This checks if already exists)
  presShell->AddOverrideStyleSheet(sheet);
  presShell->GetDocument()->ApplicableStylesChanged();

  // Save as the last-loaded sheet
  mLastOverrideStyleSheetURL = aURL;

  // Add URL and style sheet to our lists
  rv = AddNewStyleSheetToList(aURL, sheet);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::ReplaceOverrideStyleSheet(const nsAString& aURL) {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eReplaceOverrideStyleSheet);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL)) {
    // Disable last sheet if not the same as new one
    if (!mLastOverrideStyleSheetURL.IsEmpty() &&
        !mLastOverrideStyleSheetURL.Equals(aURL)) {
      EnableStyleSheetInternal(mLastOverrideStyleSheetURL, false);
    }
    return NS_OK;
  }
  // Remove the previous sheet
  if (!mLastOverrideStyleSheetURL.IsEmpty()) {
    DebugOnly<nsresult> rv =
        RemoveOverrideStyleSheetInternal(mLastOverrideStyleSheetURL);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to remove the last override style sheet");
  }
  nsresult rv = AddOverrideStyleSheetInternal(aURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

// Do NOT use transaction system for override style sheets
NS_IMETHODIMP
HTMLEditor::RemoveOverrideStyleSheet(const nsAString& aURL) {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eRemoveOverrideStyleSheet);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = RemoveOverrideStyleSheetInternal(aURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::RemoveOverrideStyleSheetInternal(const nsAString& aURL) {
  if (NS_WARN_IF(!IsInitialized())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Make sure we remove the stylesheet from our internal list in all
  // cases.
  RefPtr<StyleSheet> sheet = RemoveStyleSheetFromList(aURL);
  if (!sheet) {
    return NS_OK;  // It's okay even if not found.
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  presShell->RemoveOverrideStyleSheet(sheet);
  presShell->GetDocument()->ApplicableStylesChanged();

  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::EnableStyleSheet(const nsAString& aURL, bool aEnable) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eEnableStyleSheet);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  EnableStyleSheetInternal(aURL, aEnable);
  return NS_OK;
}

void HTMLEditor::EnableStyleSheetInternal(const nsAString& aURL, bool aEnable) {
  RefPtr<StyleSheet> sheet = GetStyleSheetForURL(aURL);
  if (!sheet) {
    return;
  }

  // Ensure the style sheet is owned by our document.
  RefPtr<Document> document = GetDocument();
  sheet->SetAssociatedDocumentOrShadowRoot(
      document, StyleSheet::NotOwnedByDocumentOrShadowRoot);

  sheet->SetDisabled(!aEnable);
}

bool HTMLEditor::EnableExistingStyleSheet(const nsAString& aURL) {
  RefPtr<StyleSheet> sheet = GetStyleSheetForURL(aURL);

  // Enable sheet if already loaded.
  if (!sheet) {
    return false;
  }

  // Ensure the style sheet is owned by our document.
  RefPtr<Document> document = GetDocument();
  sheet->SetAssociatedDocumentOrShadowRoot(
      document, StyleSheet::NotOwnedByDocumentOrShadowRoot);

  // FIXME: This used to do sheet->SetDisabled(false), figure out if we can
  // just remove all this code in bug 1449522, since it seems unused.
  return true;
}

nsresult HTMLEditor::AddNewStyleSheetToList(const nsAString& aURL,
                                            StyleSheet* aStyleSheet) {
  uint32_t countSS = mStyleSheets.Length();
  uint32_t countU = mStyleSheetURLs.Length();

  if (countSS != countU) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mStyleSheetURLs.AppendElement(aURL)) {
    return NS_ERROR_UNEXPECTED;
  }

  return mStyleSheets.AppendElement(aStyleSheet) ? NS_OK : NS_ERROR_UNEXPECTED;
}

already_AddRefed<StyleSheet> HTMLEditor::RemoveStyleSheetFromList(
    const nsAString& aURL) {
  // is it already in the list?
  size_t foundIndex = mStyleSheetURLs.IndexOf(aURL);
  if (foundIndex == mStyleSheetURLs.NoIndex) {
    return nullptr;
  }

  RefPtr<StyleSheet> removingStyleSheet = mStyleSheets[foundIndex];
  MOZ_ASSERT(removingStyleSheet);

  // Attempt both removals; if one fails there's not much we can do.
  mStyleSheets.RemoveElementAt(foundIndex);
  mStyleSheetURLs.RemoveElementAt(foundIndex);

  return removingStyleSheet.forget();
}

StyleSheet* HTMLEditor::GetStyleSheetForURL(const nsAString& aURL) {
  // is it already in the list?
  size_t foundIndex;
  foundIndex = mStyleSheetURLs.IndexOf(aURL);
  if (foundIndex == mStyleSheetURLs.NoIndex) {
    return nullptr;
  }

  MOZ_ASSERT(mStyleSheets[foundIndex]);
  return mStyleSheets[foundIndex];
}

nsresult HTMLEditor::DeleteSelectionWithTransaction(
    EDirection aAction, EStripWrappers aStripWrappers) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  nsresult rv =
      TextEditor::DeleteSelectionWithTransaction(aAction, aStripWrappers);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If we weren't asked to strip any wrappers, we're done.
  if (aStripWrappers == eNoStrip) {
    return NS_OK;
  }

  // Just checking that the selection itself is collapsed doesn't seem to work
  // right in the multi-range case
  if (NS_WARN_IF(!SelectionRefPtr()->GetAnchorFocusRange()) ||
      NS_WARN_IF(!SelectionRefPtr()->GetAnchorFocusRange()->Collapsed()) ||
      NS_WARN_IF(!SelectionRefPtr()->GetAnchorNode()->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> content =
      SelectionRefPtr()->GetAnchorNode()->AsContent();

  // Don't strip wrappers if this is the only wrapper in the block.  Then we'll
  // add a <br> later, so it won't be an empty wrapper in the end.
  nsCOMPtr<nsIContent> blockParent = content;
  while (blockParent && !IsBlockNode(blockParent)) {
    blockParent = blockParent->GetParent();
  }
  if (!blockParent) {
    return NS_OK;
  }
  bool emptyBlockParent;
  rv = IsEmptyNode(blockParent, &emptyBlockParent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (emptyBlockParent) {
    return NS_OK;
  }

  if (content && !IsBlockNode(content) && !content->Length() &&
      content->IsEditable() && content != content->GetEditingHost()) {
    while (content->GetParent() && !IsBlockNode(content->GetParent()) &&
           content->GetParent()->Length() == 1 &&
           content->GetParent()->IsEditable() &&
           content->GetParent() != content->GetEditingHost()) {
      content = content->GetParent();
    }
    rv = DeleteNodeWithTransaction(*content);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::DeleteNodeWithTransaction(nsINode& aNode) {
  if (NS_WARN_IF(!aNode.IsContent())) {
    return NS_ERROR_INVALID_ARG;
  }
  // Do nothing if the node is read-only.
  // XXX This is not a override method of EditorBase's method.  This might
  //     cause not called accidentally.  We need to investigate this issue.
  if (NS_WARN_IF(!IsModifiableNode(*aNode.AsContent()) &&
                 !EditorBase::IsPaddingBRElementForEmptyEditor(aNode))) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = EditorBase::DeleteNodeWithTransaction(aNode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::DeleteAllChildrenWithTransaction(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Prevent rules testing until we're done
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eDeleteNode, nsIEditor::eNext);

  while (nsCOMPtr<nsINode> child = aElement.GetLastChild()) {
    nsresult rv = DeleteNodeWithTransaction(*child);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::DeleteParentBlocksWithTransactionIfEmpty(
    const EditorDOMPoint& aPoint) {
  MOZ_ASSERT(aPoint.IsSet());
  MOZ_ASSERT(mPlaceholderBatch);

  // First, check there is visible contents before the point in current block.
  WSRunObject wsObj(this, aPoint);
  if (!(wsObj.mStartReason & WSType::thisBlock)) {
    // If there is visible node before the point, we shouldn't remove the
    // parent block.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }
  if (NS_WARN_IF(!wsObj.mStartReasonNode) ||
      NS_WARN_IF(!wsObj.mStartReasonNode->GetParentNode())) {
    return NS_ERROR_FAILURE;
  }
  if (wsObj.GetEditingHost() == wsObj.mStartReasonNode) {
    // If we reach editing host, there is no parent blocks which can be removed.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }
  if (HTMLEditUtils::IsTableCellOrCaption(*wsObj.mStartReasonNode)) {
    // If we reach a <td>, <th> or <caption>, we shouldn't remove it even
    // becomes empty because removing such element changes the structure of
    // the <table>.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  // Next, check there is visible contents after the point in current block.
  WSType wsType = WSType::none;
  wsObj.NextVisibleNode(aPoint, &wsType);
  if (wsType == WSType::br) {
    // If the <br> element is visible, we shouldn't remove the parent block.
    if (IsVisibleBRElement(wsObj.mEndReasonNode)) {
      return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
    }
    if (wsObj.mEndReasonNode->GetNextSibling()) {
      EditorRawDOMPoint afterBRElement;
      afterBRElement.SetAfter(wsObj.mEndReasonNode);
      WSRunObject wsRunObjAfterBR(this, afterBRElement);
      WSType wsTypeAfterBR = WSType::none;
      wsRunObjAfterBR.NextVisibleNode(afterBRElement, &wsTypeAfterBR);
      if (wsTypeAfterBR != WSType::thisBlock) {
        // If we couldn't reach the block's end after the invisible <br>,
        // that means that there is visible content.
        return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
      }
    }
  } else if (wsType != WSType::thisBlock) {
    // If we couldn't reach the block's end, the block has visible content.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  // Delete the parent block.
  EditorDOMPoint nextPoint(wsObj.mStartReasonNode->GetParentNode(), 0);
  nsresult rv =
      DeleteNodeWithTransaction(MOZ_KnownLive(*wsObj.mStartReasonNode));
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  // If we reach editing host, return NS_OK.
  if (nextPoint.GetContainer() == wsObj.GetEditingHost()) {
    return NS_OK;
  }

  // Otherwise, we need to check whether we're still in empty block or not.

  // If we have mutation event listeners, the next point is now outside of
  // editing host or editing hos has been changed.
  if (HasMutationEventListeners(NS_EVENT_BITS_MUTATION_NODEREMOVED |
                                NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
                                NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED)) {
    Element* editingHost = GetActiveEditingHost();
    if (NS_WARN_IF(!editingHost) ||
        NS_WARN_IF(editingHost != wsObj.GetEditingHost())) {
      return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
    }
    if (NS_WARN_IF(!EditorUtils::IsDescendantOf(*nextPoint.GetContainer(),
                                                *editingHost))) {
      return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
    }
  }

  rv = DeleteParentBlocksWithTransactionIfEmpty(nextPoint);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::DeleteNode(nsINode* aNode) {
  if (NS_WARN_IF(!aNode)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRemoveNode);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = DeleteNodeWithTransaction(*aNode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::DeleteTextWithTransaction(Text& aTextNode,
                                               uint32_t aOffset,
                                               uint32_t aLength) {
  // Do nothing if the node is read-only
  if (!IsModifiableNode(aTextNode)) {
    return NS_ERROR_FAILURE;
  }

  return EditorBase::DeleteTextWithTransaction(aTextNode, aOffset, aLength);
}

nsresult HTMLEditor::InsertTextWithTransaction(
    Document& aDocument, const nsAString& aStringToInsert,
    const EditorRawDOMPoint& aPointToInsert,
    EditorRawDOMPoint* aPointAfterInsertedString) {
  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  // Do nothing if the node is read-only
  if (!IsModifiableNode(*aPointToInsert.GetContainer())) {
    return NS_ERROR_FAILURE;
  }

  return EditorBase::InsertTextWithTransaction(
      aDocument, aStringToInsert, aPointToInsert, aPointAfterInsertedString);
}

already_AddRefed<Element> HTMLEditor::InsertBRElementWithTransaction(
    const EditorDOMPoint& aPointToInsert, EDirection aSelect /* = eNone */) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  EditorDOMPoint pointToInsert = PrepareToInsertBRElement(aPointToInsert);
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    return nullptr;
  }

  RefPtr<Element> newBRElement =
      CreateNodeWithTransaction(*nsGkAtoms::br, pointToInsert);
  if (NS_WARN_IF(!newBRElement)) {
    return nullptr;
  }

  switch (aSelect) {
    case eNone:
      break;
    case eNext: {
      SelectionRefPtr()->SetInterlinePosition(true, IgnoreErrors());
      // Collapse selection after the <br> node.
      EditorRawDOMPoint afterBRElement(newBRElement);
      if (afterBRElement.IsSet()) {
        DebugOnly<bool> advanced = afterBRElement.AdvanceOffset();
        NS_WARNING_ASSERTION(advanced,
                             "Failed to advance offset after the <br> element");
        ErrorResult error;
        SelectionRefPtr()->Collapse(afterBRElement, error);
        NS_WARNING_ASSERTION(
            !error.Failed(),
            "Failed to collapse selection after the <br> element");
      } else {
        NS_WARNING("The <br> node is not in the DOM tree?");
      }
      break;
    }
    case ePrevious: {
      SelectionRefPtr()->SetInterlinePosition(true, IgnoreErrors());
      // Collapse selection at the <br> node.
      EditorRawDOMPoint atBRElement(newBRElement);
      if (atBRElement.IsSet()) {
        ErrorResult error;
        SelectionRefPtr()->Collapse(atBRElement, error);
        NS_WARNING_ASSERTION(
            !error.Failed(),
            "Failed to collapse selection at the <br> element");
      } else {
        NS_WARNING("The <br> node is not in the DOM tree?");
      }
      break;
    }
    default:
      NS_WARNING(
          "aSelect has invalid value, the caller need to set selection "
          "by itself");
      break;
  }

  return newBRElement.forget();
}

void HTMLEditor::ContentAppended(nsIContent* aFirstNewContent) {
  DoContentInserted(aFirstNewContent, eAppended);
}

void HTMLEditor::ContentInserted(nsIContent* aChild) {
  DoContentInserted(aChild, eInserted);
}

bool HTMLEditor::IsInObservedSubtree(nsIContent* aChild) {
  if (!aChild) {
    return false;
  }

  Element* root = GetRoot();
  // To be super safe here, check both ChromeOnlyAccess and GetBindingParent.
  // That catches (also unbound) native anonymous content, XBL and ShadowDOM.
  if (root && (root->ChromeOnlyAccess() != aChild->ChromeOnlyAccess() ||
               root->GetBindingParent() != aChild->GetBindingParent())) {
    return false;
  }

  return !aChild->ChromeOnlyAccess() && !aChild->GetBindingParent();
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
    nsContentUtils::AddScriptRunner(NewRunnableMethod(
        "HTMLEditor::NotifyRootChanged", this, &HTMLEditor::NotifyRootChanged));
  }
  // We don't need to handle our own modifications
  else if (!GetTopLevelEditSubAction() && container->IsEditable()) {
    if (EditorBase::IsPaddingBRElementForEmptyEditor(*aChild)) {
      // Ignore insertion of the padding <br> element.
      return;
    }
    RefPtr<HTMLEditRules> htmlRules = mRules->AsHTMLEditRules();
    if (htmlRules) {
      htmlRules->DocumentModified();
    }

    // Update spellcheck for only the newly-inserted node (bug 743819)
    if (mInlineSpellChecker) {
      RefPtr<nsRange> range = new nsRange(aChild);
      nsIContent* endContent = aChild;
      if (aInsertedOrAppended == eAppended) {
        // Maybe more than 1 child was appended.
        endContent = container->GetLastChild();
      }
      range->SelectNodesInContainer(container, aChild, endContent);
      mInlineSpellChecker->SpellCheckRange(range);
    }
  }
}

void HTMLEditor::ContentRemoved(nsIContent* aChild,
                                nsIContent* aPreviousSibling) {
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
    nsContentUtils::AddScriptRunner(NewRunnableMethod(
        "HTMLEditor::NotifyRootChanged", this, &HTMLEditor::NotifyRootChanged));
    // We don't need to handle our own modifications
  } else if (!GetTopLevelEditSubAction() &&
             aChild->GetParentNode()->IsEditable()) {
    if (aChild && EditorBase::IsPaddingBRElementForEmptyEditor(*aChild)) {
      // Ignore removal of the padding <br> element for empty editor.
      return;
    }

    RefPtr<HTMLEditRules> htmlRules = mRules->AsHTMLEditRules();
    if (htmlRules) {
      htmlRules->DocumentModified();
    }
  }
}

void HTMLEditor::OnStartToHandleTopLevelEditSubAction(
    EditSubAction aEditSubAction, nsIEditor::EDirection aDirection) {
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  EditorBase::OnStartToHandleTopLevelEditSubAction(aEditSubAction, aDirection);
  if (!rules) {
    return;
  }

  MOZ_ASSERT(GetTopLevelEditSubAction() == aEditSubAction);
  MOZ_ASSERT(GetDirectionOfTopLevelEditSubAction() == aDirection);
  DebugOnly<nsresult> rv = rules->BeforeEdit();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditRules::BeforeEdit() failed to handle something");
}

void HTMLEditor::OnEndHandlingTopLevelEditSubAction() {
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  // post processing
  DebugOnly<nsresult> rv = rules ? rules->AfterEdit() : NS_OK;
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditRules::AfterEdit() failed to handle something");
  EditorBase::OnEndHandlingTopLevelEditSubAction();
  MOZ_ASSERT(!GetTopLevelEditSubAction());
  MOZ_ASSERT(GetDirectionOfTopLevelEditSubAction() == eNone);
}

bool HTMLEditor::TagCanContainTag(nsAtom& aParentTag, nsAtom& aChildTag) const {
  int32_t childTagEnum;
  // XXX Should this handle #cdata-section too?
  if (&aChildTag == nsGkAtoms::textTagName) {
    childTagEnum = eHTMLTag_text;
  } else {
    childTagEnum = nsHTMLTags::AtomTagToId(&aChildTag);
  }

  int32_t parentTagEnum = nsHTMLTags::AtomTagToId(&aParentTag);
  return HTMLEditUtils::CanContain(parentTagEnum, childTagEnum);
}

bool HTMLEditor::IsContainer(nsINode* aNode) const {
  MOZ_ASSERT(aNode);

  int32_t tagEnum;
  // XXX Should this handle #cdata-section too?
  if (aNode->IsText()) {
    tagEnum = eHTMLTag_text;
  } else {
    tagEnum = nsHTMLTags::StringTagToId(aNode->NodeName());
  }

  return HTMLEditUtils::IsContainer(tagEnum);
}

nsresult HTMLEditor::SelectEntireDocument() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mRules) {
    return NS_ERROR_NULL_POINTER;
  }

  RefPtr<Element> rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  // If we're empty, don't select all children because that would select the
  // padding <br> element for empty editor.
  if (rules->DocumentIsEmpty()) {
    nsresult rv = SelectionRefPtr()->Collapse(rootElement, 0);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to move caret to start of the editor root element");
    return rv;
  }

  // Otherwise, select all children.
  ErrorResult error;
  SelectionRefPtr()->SelectAllChildren(*rootElement, error);
  NS_WARNING_ASSERTION(
      !error.Failed(),
      "Failed to select all children of the editor root element");
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

  nsINode* anchorNode = SelectionRefPtr()->GetAnchorNode();
  if (NS_WARN_IF(!anchorNode) || NS_WARN_IF(!anchorNode->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  nsIContent* anchorContent = anchorNode->AsContent();
  nsIContent* rootContent;
  if (anchorContent->HasIndependentSelection()) {
    SelectionRefPtr()->SetAncestorLimiter(nullptr);
    rootContent = mRootElement;
  } else {
    RefPtr<PresShell> presShell = GetPresShell();
    rootContent = anchorContent->GetSelectionRootContent(presShell);
  }

  if (NS_WARN_IF(!rootContent)) {
    return NS_ERROR_UNEXPECTED;
  }

  Maybe<Selection::AutoUserInitiated> userSelection;
  if (!rootContent->IsEditable()) {
    userSelection.emplace(SelectionRefPtr());
  }
  ErrorResult errorResult;
  SelectionRefPtr()->SelectAllChildren(*rootContent, errorResult);
  NS_WARNING_ASSERTION(!errorResult.Failed(), "SelectAllChildren() failed");
  return errorResult.StealNSResult();
}

// this will NOT find aAttribute unless aAttribute has a non-null value
// so singleton attributes like <Table border> will not be matched!
bool HTMLEditor::IsTextPropertySetByContent(nsINode* aNode, nsAtom* aProperty,
                                            nsAtom* aAttribute,
                                            const nsAString* aValue,
                                            nsAString* outValue) {
  MOZ_ASSERT(aNode && aProperty);

  while (aNode) {
    if (aNode->IsElement()) {
      Element* element = aNode->AsElement();
      if (aProperty == element->NodeInfo()->NameAtom()) {
        if (!aAttribute) {
          return true;
        }
        nsAutoString value;
        element->GetAttr(kNameSpaceID_None, aAttribute, value);
        if (outValue) {
          *outValue = value;
        }
        if (!value.IsEmpty()) {
          if (!aValue) {
            return true;
          }
          if (aValue->Equals(value, nsCaseInsensitiveStringComparator())) {
            return true;
          }
          // We found the prop with the attribute, but the value doesn't
          // match.
          break;
        }
      }
    }
    aNode = aNode->GetParentNode();
  }
  return false;
}

bool HTMLEditor::SetCaretInTableCell(Element* aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!aElement || !aElement->IsHTMLElement() ||
      !HTMLEditUtils::IsTableElement(aElement) ||
      !IsDescendantOfEditorRoot(aElement)) {
    return false;
  }

  nsIContent* node = aElement;
  while (node->HasChildren()) {
    node = node->GetFirstChild();
  }

  // Set selection at beginning of the found node
  nsresult rv = SelectionRefPtr()->Collapse(node, 0);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Failed to collapse Selection in aElement");
  return NS_SUCCEEDED(rv);
}

/**
 * GetEnclosingTable() finds ancestor who is a table, if any.
 */
Element* HTMLEditor::GetEnclosingTable(nsINode* aNode) {
  MOZ_ASSERT(aNode);

  for (nsCOMPtr<Element> block = GetBlockNodeParent(aNode); block;
       block = GetBlockNodeParent(block)) {
    if (HTMLEditUtils::IsTable(block)) {
      return block;
    }
  }
  return nullptr;
}

/**
 * This method scans the selection for adjacent text nodes
 * and collapses them into a single text node.
 * "adjacent" means literally adjacent siblings of the same parent.
 * Uses EditorBase::JoinNodesWithTransaction() so action is undoable.
 * Should be called within the context of a batch transaction.
 */
nsresult HTMLEditor::CollapseAdjacentTextNodes(nsRange* aInRange) {
  NS_ENSURE_TRUE(aInRange, NS_ERROR_NULL_POINTER);
  AutoTransactionsConserveSelection dontChangeMySelection(*this);
  nsTArray<nsCOMPtr<nsINode>> textNodes;
  // we can't actually do anything during iteration, so store the text nodes in
  // an array don't bother ref counting them because we know we can hold them
  // for the lifetime of this method

  // build a list of editable text nodes
  ContentSubtreeIterator subtreeIter;
  subtreeIter.Init(aInRange);
  for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
    nsINode* node = subtreeIter.GetCurrentNode();
    if (node->NodeType() == nsINode::TEXT_NODE &&
        IsEditable(node->AsContent())) {
      textNodes.AppendElement(node);
    }
  }

  // now that I have a list of text nodes, collapse adjacent text nodes
  // NOTE: assumption that JoinNodes keeps the righthand node
  while (textNodes.Length() > 1) {
    // we assume a textNodes entry can't be nullptr
    nsINode* leftTextNode = textNodes[0];
    nsINode* rightTextNode = textNodes[1];
    NS_ASSERTION(leftTextNode && rightTextNode,
                 "left or rightTextNode null in CollapseAdjacentTextNodes");

    // get the prev sibling of the right node, and see if its leftTextNode
    nsCOMPtr<nsINode> prevSibOfRightNode = rightTextNode->GetPreviousSibling();
    if (prevSibOfRightNode && prevSibOfRightNode == leftTextNode) {
      nsresult rv = JoinNodesWithTransaction(MOZ_KnownLive(*leftTextNode),
                                             MOZ_KnownLive(*rightTextNode));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    textNodes.RemoveElementAt(
        0);  // remove the leftmost text node from the list
  }

  return NS_OK;
}

nsresult HTMLEditor::SetSelectionAtDocumentStart() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  dom::Element* rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER);

  nsresult rv = SelectionRefPtr()->Collapse(rootElement, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
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

  nsCOMPtr<nsIContent> child = GetFirstEditableChild(aElement);

  if (child) {
    // The case of aNode not being empty.  We need a br at start unless:
    // 1) previous sibling of aNode is a block, OR
    // 2) previous sibling of aNode is a br, OR
    // 3) first child of aNode is a block OR
    // 4) either is null

    nsCOMPtr<nsIContent> sibling = GetPriorHTMLSibling(&aElement);
    if (sibling && !IsBlockNode(sibling) &&
        !sibling->IsHTMLElement(nsGkAtoms::br) && !IsBlockNode(child)) {
      // Insert br node
      RefPtr<Element> brElement =
          InsertBRElementWithTransaction(EditorDOMPoint(&aElement, 0));
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }
    }

    // We need a br at end unless:
    // 1) following sibling of aNode is a block, OR
    // 2) last child of aNode is a block, OR
    // 3) last child of aNode is a br OR
    // 4) either is null

    sibling = GetNextHTMLSibling(&aElement);
    if (sibling && !IsBlockNode(sibling)) {
      child = GetLastEditableChild(aElement);
      MOZ_ASSERT(child, "aNode has first editable child but not last?");
      if (!IsBlockNode(child) && !child->IsHTMLElement(nsGkAtoms::br)) {
        // Insert br node
        EditorDOMPoint endOfNode;
        endOfNode.SetToEndOf(&aElement);
        RefPtr<Element> brElement = InsertBRElementWithTransaction(endOfNode);
        if (NS_WARN_IF(!brElement)) {
          return NS_ERROR_FAILURE;
        }
      }
    }
  } else {
    // The case of aNode being empty.  We need a br at start unless:
    // 1) previous sibling of aNode is a block, OR
    // 2) previous sibling of aNode is a br, OR
    // 3) following sibling of aNode is a block, OR
    // 4) following sibling of aNode is a br OR
    // 5) either is null
    nsCOMPtr<nsIContent> sibling = GetPriorHTMLSibling(&aElement);
    if (sibling && !IsBlockNode(sibling) &&
        !sibling->IsHTMLElement(nsGkAtoms::br)) {
      sibling = GetNextHTMLSibling(&aElement);
      if (sibling && !IsBlockNode(sibling) &&
          !sibling->IsHTMLElement(nsGkAtoms::br)) {
        // Insert br node
        RefPtr<Element> brElement =
            InsertBRElementWithTransaction(EditorDOMPoint(&aElement, 0));
        if (NS_WARN_IF(!brElement)) {
          return NS_ERROR_FAILURE;
        }
      }
    }
  }

  // Now remove container
  nsresult rv = RemoveContainerWithTransaction(aElement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

/**
 * GetPriorHTMLSibling() returns the previous editable sibling, if there is
 * one within the parent.
 */
nsIContent* HTMLEditor::GetPriorHTMLSibling(nsINode* aNode) {
  MOZ_ASSERT(aNode);

  nsIContent* node = aNode->GetPreviousSibling();
  while (node && !IsEditable(node)) {
    node = node->GetPreviousSibling();
  }

  return node;
}

/**
 * GetNextHTMLSibling() returns the next editable sibling, if there is
 * one within the parent.
 */
nsIContent* HTMLEditor::GetNextHTMLSibling(nsINode* aNode) {
  MOZ_ASSERT(aNode);

  nsIContent* node = aNode->GetNextSibling();
  while (node && !IsEditable(node)) {
    node = node->GetNextSibling();
  }

  return node;
}

nsIContent* HTMLEditor::GetPreviousHTMLElementOrTextInternal(
    nsINode& aNode, bool aNoBlockCrossing) {
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetPreviousElementOrTextInBlock(aNode)
                          : GetPreviousElementOrText(aNode);
}

template <typename PT, typename CT>
nsIContent* HTMLEditor::GetPreviousHTMLElementOrTextInternal(
    const EditorDOMPointBase<PT, CT>& aPoint, bool aNoBlockCrossing) {
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetPreviousElementOrTextInBlock(aPoint)
                          : GetPreviousElementOrText(aPoint);
}

nsIContent* HTMLEditor::GetPreviousEditableHTMLNodeInternal(
    nsINode& aNode, bool aNoBlockCrossing) {
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetPreviousEditableNodeInBlock(aNode)
                          : GetPreviousEditableNode(aNode);
}

template <typename PT, typename CT>
nsIContent* HTMLEditor::GetPreviousEditableHTMLNodeInternal(
    const EditorDOMPointBase<PT, CT>& aPoint, bool aNoBlockCrossing) {
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetPreviousEditableNodeInBlock(aPoint)
                          : GetPreviousEditableNode(aPoint);
}

nsIContent* HTMLEditor::GetNextHTMLElementOrTextInternal(
    nsINode& aNode, bool aNoBlockCrossing) {
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetNextElementOrTextInBlock(aNode)
                          : GetNextElementOrText(aNode);
}

template <typename PT, typename CT>
nsIContent* HTMLEditor::GetNextHTMLElementOrTextInternal(
    const EditorDOMPointBase<PT, CT>& aPoint, bool aNoBlockCrossing) {
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetNextElementOrTextInBlock(aPoint)
                          : GetNextElementOrText(aPoint);
}

nsIContent* HTMLEditor::GetNextEditableHTMLNodeInternal(
    nsINode& aNode, bool aNoBlockCrossing) const {
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetNextEditableNodeInBlock(aNode)
                          : GetNextEditableNode(aNode);
}

template <typename PT, typename CT>
nsIContent* HTMLEditor::GetNextEditableHTMLNodeInternal(
    const EditorDOMPointBase<PT, CT>& aPoint, bool aNoBlockCrossing) const {
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetNextEditableNodeInBlock(aPoint)
                          : GetNextEditableNode(aPoint);
}

bool HTMLEditor::IsFirstEditableChild(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  // find first editable child and compare it to aNode
  nsCOMPtr<nsINode> parent = aNode->GetParentNode();
  if (NS_WARN_IF(!parent)) {
    return false;
  }
  return (GetFirstEditableChild(*parent) == aNode);
}

bool HTMLEditor::IsLastEditableChild(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  // find last editable child and compare it to aNode
  nsCOMPtr<nsINode> parent = aNode->GetParentNode();
  if (NS_WARN_IF(!parent)) {
    return false;
  }
  return (GetLastEditableChild(*parent) == aNode);
}

nsIContent* HTMLEditor::GetFirstEditableChild(nsINode& aNode) {
  nsCOMPtr<nsIContent> child = aNode.GetFirstChild();

  while (child && !IsEditable(child)) {
    child = child->GetNextSibling();
  }

  return child;
}

nsIContent* HTMLEditor::GetLastEditableChild(nsINode& aNode) {
  nsCOMPtr<nsIContent> child = aNode.GetLastChild();

  while (child && !IsEditable(child)) {
    child = child->GetPreviousSibling();
  }

  return child;
}

nsIContent* HTMLEditor::GetFirstEditableLeaf(nsINode& aNode) {
  nsCOMPtr<nsIContent> child = GetLeftmostChild(&aNode);
  while (child && (!IsEditable(child) || child->HasChildren())) {
    child = GetNextEditableHTMLNode(*child);

    // Only accept nodes that are descendants of aNode
    if (!aNode.Contains(child)) {
      return nullptr;
    }
  }

  return child;
}

nsIContent* HTMLEditor::GetLastEditableLeaf(nsINode& aNode) {
  nsCOMPtr<nsIContent> child = GetRightmostChild(&aNode, false);
  while (child && (!IsEditable(child) || child->HasChildren())) {
    child = GetPreviousEditableHTMLNode(*child);

    // Only accept nodes that are descendants of aNode
    if (!aNode.Contains(child)) {
      return nullptr;
    }
  }

  return child;
}

bool HTMLEditor::IsInVisibleTextFrames(Text& aText) const {
  nsISelectionController* selectionController = GetSelectionController();
  if (NS_WARN_IF(!selectionController)) {
    return false;
  }

  if (!aText.TextDataLength()) {
    return false;
  }

  // Ask the selection controller for information about whether any of the
  // data in the node is really rendered.  This is really something that
  // frames know about, but we aren't supposed to talk to frames.  So we put
  // a call in the selection controller interface, since it's already in bed
  // with frames anyway.  (This is a fix for bug 22227, and a partial fix for
  // bug 46209.)
  bool isVisible = false;
  nsresult rv = selectionController->CheckVisibilityContent(
      &aText, 0, aText.TextDataLength(), &isVisible);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return isVisible;
}

bool HTMLEditor::IsVisibleTextNode(Text& aText) const {
  if (!aText.TextDataLength()) {
    return false;
  }

  if (!aText.TextIsOnlyWhitespace()) {
    return true;
  }

  WSRunScanner wsRunScanner(this, &aText, 0);
  nsCOMPtr<nsINode> nextVisibleNode;
  WSType visibleNodeType;
  wsRunScanner.NextVisibleNode(EditorRawDOMPoint(&aText, 0),
                               address_of(nextVisibleNode), nullptr,
                               &visibleNodeType);
  return (visibleNodeType == WSType::normalWS ||
          visibleNodeType == WSType::text) &&
         &aText == nextVisibleNode;
}

/**
 * IsEmptyNode() figures out if aNode is an empty node.  A block can have
 * children and still be considered empty, if the children are empty or
 * non-editable.
 */
nsresult HTMLEditor::IsEmptyNode(nsINode* aNode, bool* outIsEmptyNode,
                                 bool aSingleBRDoesntCount,
                                 bool aListOrCellNotEmpty,
                                 bool aSafeToAskFrames) const {
  NS_ENSURE_TRUE(aNode && outIsEmptyNode, NS_ERROR_NULL_POINTER);
  *outIsEmptyNode = true;
  bool seenBR = false;
  return IsEmptyNodeImpl(aNode, outIsEmptyNode, aSingleBRDoesntCount,
                         aListOrCellNotEmpty, aSafeToAskFrames, &seenBR);
}

/**
 * IsEmptyNodeImpl() is workhorse for IsEmptyNode().
 */
nsresult HTMLEditor::IsEmptyNodeImpl(nsINode* aNode, bool* outIsEmptyNode,
                                     bool aSingleBRDoesntCount,
                                     bool aListOrCellNotEmpty,
                                     bool aSafeToAskFrames,
                                     bool* aSeenBR) const {
  NS_ENSURE_TRUE(aNode && outIsEmptyNode && aSeenBR, NS_ERROR_NULL_POINTER);

  if (Text* text = aNode->GetAsText()) {
    *outIsEmptyNode = aSafeToAskFrames ? !IsInVisibleTextFrames(*text)
                                       : !IsVisibleTextNode(*text);
    return NS_OK;
  }

  // if it's not a text node (handled above) and it's not a container,
  // then we don't call it empty (it's an <hr>, or <br>, etc.).
  // Also, if it's an anchor then don't treat it as empty - even though
  // anchors are containers, named anchors are "empty" but we don't
  // want to treat them as such.  Also, don't call ListItems or table
  // cells empty if caller desires.  Form Widgets not empty.
  if (!IsContainer(aNode) ||
      (HTMLEditUtils::IsNamedAnchor(aNode) ||
       HTMLEditUtils::IsFormWidget(aNode) ||
       (aListOrCellNotEmpty && (HTMLEditUtils::IsListItem(aNode) ||
                                HTMLEditUtils::IsTableCell(aNode))))) {
    *outIsEmptyNode = false;
    return NS_OK;
  }

  // need this for later
  bool isListItemOrCell =
      HTMLEditUtils::IsListItem(aNode) || HTMLEditUtils::IsTableCell(aNode);

  // loop over children of node. if no children, or all children are either
  // empty text nodes or non-editable, then node qualifies as empty
  for (nsCOMPtr<nsIContent> child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    // Is the child editable and non-empty?  if so, return false
    if (EditorBase::IsEditable(child)) {
      if (Text* text = child->GetAsText()) {
        // break out if we find we aren't empty
        *outIsEmptyNode = aSafeToAskFrames ? !IsInVisibleTextFrames(*text)
                                           : !IsVisibleTextNode(*text);
        if (!*outIsEmptyNode) {
          return NS_OK;
        }
      } else {
        // An editable, non-text node. We need to check its content.
        // Is it the node we are iterating over?
        if (child == aNode) {
          break;
        }

        if (aSingleBRDoesntCount && !*aSeenBR &&
            child->IsHTMLElement(nsGkAtoms::br)) {
          // the first br in a block doesn't count if the caller so indicated
          *aSeenBR = true;
        } else {
          // is it an empty node of some sort?
          // note: list items or table cells are not considered empty
          // if they contain other lists or tables
          if (child->IsElement()) {
            if (isListItemOrCell) {
              if (HTMLEditUtils::IsList(child) ||
                  child->IsHTMLElement(nsGkAtoms::table)) {
                // break out if we find we aren't empty
                *outIsEmptyNode = false;
                return NS_OK;
              }
            } else if (HTMLEditUtils::IsFormWidget(child)) {
              // is it a form widget?
              // break out if we find we aren't empty
              *outIsEmptyNode = false;
              return NS_OK;
            }
          }

          bool isEmptyNode = true;
          nsresult rv =
              IsEmptyNodeImpl(child, &isEmptyNode, aSingleBRDoesntCount,
                              aListOrCellNotEmpty, aSafeToAskFrames, aSeenBR);
          NS_ENSURE_SUCCESS(rv, rv);
          if (!isEmptyNode) {
            // otherwise it ain't empty
            *outIsEmptyNode = false;
            return NS_OK;
          }
        }
      }
    }
  }

  return NS_OK;
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

  if (!IsCSSEnabled() || !mCSSEditUtils) {
    // we are not in an HTML+CSS editor; let's set the attribute the HTML way
    if (mCSSEditUtils) {
      mCSSEditUtils->RemoveCSSEquivalentToHTMLStyle(
          aElement, nullptr, aAttribute, nullptr, aSuppressTransaction);
    }
    return aSuppressTransaction
               ? aElement->SetAttr(kNameSpaceID_None, aAttribute, aValue, true)
               : SetAttributeWithTransaction(*aElement, *aAttribute, aValue);
  }

  int32_t count = mCSSEditUtils->SetCSSEquivalentToHTMLStyle(
      aElement, nullptr, aAttribute, &aValue, aSuppressTransaction);
  if (count) {
    // we found an equivalence ; let's remove the HTML attribute itself if it
    // is set
    nsAutoString existingValue;
    if (!aElement->GetAttr(kNameSpaceID_None, aAttribute, existingValue)) {
      return NS_OK;
    }

    return aSuppressTransaction
               ? aElement->UnsetAttr(kNameSpaceID_None, aAttribute, true)
               : RemoveAttributeWithTransaction(*aElement, *aAttribute);
  }

  // count is an integer that represents the number of CSS declarations applied
  // to the element. If it is zero, we found no equivalence in this
  // implementation for the attribute
  if (aAttribute == nsGkAtoms::style) {
    // if it is the style attribute, just add the new value to the existing
    // style attribute's value
    nsAutoString existingValue;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::style, existingValue);
    existingValue.Append(' ');
    existingValue.Append(aValue);
    return aSuppressTransaction
               ? aElement->SetAttr(kNameSpaceID_None, aAttribute, existingValue,
                                   true)
               : SetAttributeWithTransaction(*aElement, *aAttribute,
                                             existingValue);
  }

  // we have no CSS equivalence for this attribute and it is not the style
  // attribute; let's set it the good'n'old HTML way
  return aSuppressTransaction
             ? aElement->SetAttr(kNameSpaceID_None, aAttribute, aValue, true)
             : SetAttributeWithTransaction(*aElement, *aAttribute, aValue);
}

nsresult HTMLEditor::RemoveAttributeOrEquivalent(Element* aElement,
                                                 nsAtom* aAttribute,
                                                 bool aSuppressTransaction) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aAttribute);

  if (IsCSSEnabled() && mCSSEditUtils) {
    nsresult rv = mCSSEditUtils->RemoveCSSEquivalentToHTMLStyle(
        aElement, nullptr, aAttribute, nullptr, aSuppressTransaction);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!aElement->HasAttr(kNameSpaceID_None, aAttribute)) {
    return NS_OK;
  }

  return aSuppressTransaction
             ? aElement->UnsetAttr(kNameSpaceID_None, aAttribute,
                                   /* aNotify = */ true)
             : RemoveAttributeWithTransaction(*aElement, *aAttribute);
}

NS_IMETHODIMP
HTMLEditor::SetIsCSSEnabled(bool aIsCSSPrefChecked) {
  if (!mCSSEditUtils) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eEnableOrDisableCSS);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mCSSEditUtils->SetCSSEnabled(aIsCSSPrefChecked);

  // Disable the eEditorNoCSSMask flag if we're enabling StyleWithCSS.
  uint32_t flags = mFlags;
  if (aIsCSSPrefChecked) {
    // Turn off NoCSS as we're enabling CSS
    flags &= ~eEditorNoCSSMask;
  } else {
    // Turn on NoCSS, as we're disabling CSS.
    flags |= eEditorNoCSSMask;
  }

  return SetFlags(flags);
}

// Set the block background color
nsresult HTMLEditor::SetCSSBackgroundColorWithTransaction(
    const nsAString& aColor) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mRules)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CommitComposition();

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  bool isCollapsed = SelectionRefPtr()->IsCollapsed();

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertElement, nsIEditor::eNext);
  AutoSelectionRestorer restoreSelectionLater(*this);
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  // XXX Although, this method may set background color of ancestor block
  //     element, using EditSubAction::eSetTextProperty.
  bool cancel, handled;
  EditSubActionInfo subActionInfo(EditSubAction::eSetTextProperty);
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!cancel && !handled) {
    // Loop through the ranges in the selection
    for (uint32_t i = 0; i < SelectionRefPtr()->RangeCount(); i++) {
      RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(i);
      if (NS_WARN_IF(!range)) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<Element> cachedBlockParent;

      // Check for easy case: both range endpoints in same text node
      nsCOMPtr<nsINode> startNode = range->GetStartContainer();
      int32_t startOffset = range->StartOffset();
      nsCOMPtr<nsINode> endNode = range->GetEndContainer();
      int32_t endOffset = range->EndOffset();
      if (startNode == endNode && IsTextNode(startNode)) {
        // Let's find the block container of the text node
        nsCOMPtr<Element> blockParent = GetBlockNodeParent(startNode);
        // And apply the background color to that block container
        if (blockParent && cachedBlockParent != blockParent) {
          cachedBlockParent = blockParent;
          mCSSEditUtils->SetCSSEquivalentToHTMLStyle(
              blockParent, nullptr, nsGkAtoms::bgcolor, &aColor, false);
        }
      } else if (startNode == endNode &&
                 startNode->IsHTMLElement(nsGkAtoms::body) && isCollapsed) {
        // No block in the document, let's apply the background to the body
        mCSSEditUtils->SetCSSEquivalentToHTMLStyle(
            MOZ_KnownLive(startNode->AsElement()), nullptr, nsGkAtoms::bgcolor,
            &aColor, false);
      } else if (startNode == endNode && (endOffset - startOffset == 1 ||
                                          (!startOffset && !endOffset))) {
        // A unique node is selected, let's also apply the background color to
        // the containing block, possibly the node itself
        nsCOMPtr<nsIContent> selectedNode = range->GetChildAtStartOffset();
        nsCOMPtr<Element> blockParent = GetBlock(*selectedNode);
        if (blockParent && cachedBlockParent != blockParent) {
          cachedBlockParent = blockParent;
          mCSSEditUtils->SetCSSEquivalentToHTMLStyle(
              blockParent, nullptr, nsGkAtoms::bgcolor, &aColor, false);
        }
      } else {
        // Not the easy case.  Range not contained in single text node.  There
        // are up to three phases here.  There are all the nodes reported by
        // the subtree iterator to be processed.  And there are potentially a
        // starting textnode and an ending textnode which are only partially
        // contained by the range.

        // Let's handle the nodes reported by the iterator.  These nodes are
        // entirely contained in the selection range.  We build up a list of
        // them (since doing operations on the document during iteration would
        // perturb the iterator).

        nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
        nsCOMPtr<nsINode> node;

        // Iterate range and build up array
        ContentSubtreeIterator subtreeIter;
        rv = subtreeIter.Init(range);
        // Init returns an error if no nodes in range.  This can easily happen
        // with the subtree iterator if the selection doesn't contain any
        // *whole* nodes.
        if (NS_SUCCEEDED(rv)) {
          for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
            node = subtreeIter.GetCurrentNode();
            NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

            if (IsEditable(node)) {
              arrayOfNodes.AppendElement(*node);
            }
          }
        }
        // First check the start parent of the range to see if it needs to be
        // separately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(startNode) && IsEditable(startNode)) {
          nsCOMPtr<Element> blockParent = GetBlockNodeParent(startNode);
          if (blockParent && cachedBlockParent != blockParent) {
            cachedBlockParent = blockParent;
            mCSSEditUtils->SetCSSEquivalentToHTMLStyle(
                blockParent, nullptr, nsGkAtoms::bgcolor, &aColor, false);
          }
        }

        // Then loop through the list, set the property on each node
        for (auto& node : arrayOfNodes) {
          nsCOMPtr<Element> blockParent = GetBlock(node);
          if (blockParent && cachedBlockParent != blockParent) {
            cachedBlockParent = blockParent;
            mCSSEditUtils->SetCSSEquivalentToHTMLStyle(
                blockParent, nullptr, nsGkAtoms::bgcolor, &aColor, false);
          }
        }
        arrayOfNodes.Clear();

        // Last, check the end parent of the range to see if it needs to be
        // separately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(endNode) && IsEditable(endNode)) {
          nsCOMPtr<Element> blockParent = GetBlockNodeParent(endNode);
          if (blockParent && cachedBlockParent != blockParent) {
            cachedBlockParent = blockParent;
            mCSSEditUtils->SetCSSEquivalentToHTMLStyle(
                blockParent, nullptr, nsGkAtoms::bgcolor, &aColor, false);
          }
        }
      }
    }
  }
  if (!cancel) {
    // Post-process
    rv = rules->DidDoAction(subActionInfo, rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetBackgroundColor(const nsAString& aColor) {
  nsresult rv = SetBackgroundColorAsAction(aColor);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to set background color");
  return rv;
}

nsresult HTMLEditor::SetBackgroundColorAsAction(const nsAString& aColor,
                                                nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eSetBackgroundColor, aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (IsCSSEnabled()) {
    // if we are in CSS mode, we have to apply the background color to the
    // containing block (or the body if we have no block-level element in
    // the document)
    return EditorBase::ToGenericNSResult(
        SetCSSBackgroundColorWithTransaction(aColor));
  }

  // but in HTML mode, we can only set the document's background color
  return EditorBase::ToGenericNSResult(
      SetHTMLBackgroundColorWithTransaction(aColor));
}

nsresult HTMLEditor::CopyLastEditableChildStylesWithTransaction(
    Element& aPreviousBlock, Element& aNewBlock,
    RefPtr<Element>* aNewBrElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aNewBrElement)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aNewBrElement = nullptr;

  RefPtr<Element> previousBlock(&aPreviousBlock);
  RefPtr<Element> newBlock(&aNewBlock);

  // First, clear out aNewBlock.  Contract is that we want only the styles
  // from aPreviousBlock.
  for (nsCOMPtr<nsINode> child = newBlock->GetFirstChild(); child;
       child = newBlock->GetFirstChild()) {
    nsresult rv = DeleteNodeWithTransaction(*child);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // XXX aNewBlock may be moved or removed.  Even in such case, we should
  //     keep cloning the styles?

  // Look for the deepest last editable leaf node in aPreviousBlock.
  // Then, if found one is a <br> element, look for non-<br> element.
  nsIContent* deepestEditableContent = nullptr;
  for (nsCOMPtr<nsIContent> child = previousBlock.get(); child;
       child = GetLastEditableChild(*child)) {
    deepestEditableContent = child;
  }
  while (deepestEditableContent &&
         TextEditUtils::IsBreak(deepestEditableContent)) {
    deepestEditableContent =
        GetPreviousEditableHTMLNode(*deepestEditableContent);
  }
  Element* deepestVisibleEditableElement = nullptr;
  if (deepestEditableContent) {
    deepestVisibleEditableElement =
        deepestEditableContent->IsElement()
            ? deepestEditableContent->AsElement()
            : deepestEditableContent->GetParentElement();
  }

  // Clone inline elements to keep current style in the new block.
  // XXX Looks like that this is really slow if lastEditableDescendant is
  //     far from aPreviousBlock.  Probably, we should clone inline containers
  //     from ancestor to descendants without transactions, then, insert it
  //     after that with transaction.
  RefPtr<Element> lastClonedElement, firstClonsedElement;
  for (RefPtr<Element> elementInPreviousBlock = deepestVisibleEditableElement;
       elementInPreviousBlock && elementInPreviousBlock != previousBlock;
       elementInPreviousBlock = elementInPreviousBlock->GetParentElement()) {
    if (!HTMLEditUtils::IsInlineStyle(elementInPreviousBlock) &&
        !elementInPreviousBlock->IsHTMLElement(nsGkAtoms::span)) {
      continue;
    }
    nsAtom* tagName = elementInPreviousBlock->NodeInfo()->NameAtom();
    // At first time, just create the most descendant inline container element.
    if (!firstClonsedElement) {
      firstClonsedElement = lastClonedElement = CreateNodeWithTransaction(
          MOZ_KnownLive(*tagName), EditorDOMPoint(newBlock, 0));
      if (NS_WARN_IF(!firstClonsedElement)) {
        return NS_ERROR_FAILURE;
      }
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
    if (NS_WARN_IF(!lastClonedElement)) {
      return NS_ERROR_FAILURE;
    }
    CloneAttributesWithTransaction(*lastClonedElement, *elementInPreviousBlock);
  }

  if (!firstClonsedElement) {
    // XXX Even if no inline elements are cloned, shouldn't we create new
    //     <br> element for aNewBlock?
    return NS_OK;
  }

  RefPtr<Element> brElement =
      InsertBRElementWithTransaction(EditorDOMPoint(firstClonsedElement, 0));
  if (NS_WARN_IF(!brElement)) {
    return NS_ERROR_FAILURE;
  }
  *aNewBrElement = brElement.forget();
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
  NS_ENSURE_TRUE(frame, NS_OK);

  nsIFrame* container = presShell->GetAbsoluteContainingBlock(frame);
  if (NS_WARN_IF(!container)) {
    return NS_OK;
  }
  nsPoint off = frame->GetOffsetTo(container);
  aX = nsPresContext::AppUnitsToIntCSSPixels(off.x);
  aY = nsPresContext::AppUnitsToIntCSSPixels(off.y);

  return NS_OK;
}

Element* HTMLEditor::GetSelectionContainerElement() const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsINode* focusNode = nullptr;
  if (SelectionRefPtr()->IsCollapsed()) {
    focusNode = SelectionRefPtr()->GetFocusNode();
    if (NS_WARN_IF(!focusNode)) {
      return nullptr;
    }
  } else {
    uint32_t rangeCount = SelectionRefPtr()->RangeCount();
    MOZ_ASSERT(rangeCount, "If 0, Selection::IsCollapsed() should return true");

    if (rangeCount == 1) {
      nsRange* range = SelectionRefPtr()->GetRangeAt(0);

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
        focusNode = range->GetCommonAncestor();
        if (NS_WARN_IF(!focusNode)) {
          return nullptr;
        }
      }
    } else {
      for (uint32_t i = 0; i < rangeCount; i++) {
        nsRange* range = SelectionRefPtr()->GetRangeAt(i);
        nsINode* startContainer = range->GetStartContainer();
        if (!focusNode) {
          focusNode = startContainer;
        } else if (focusNode != startContainer) {
          // XXX Looks odd to use parent of startContainer because previous
          //     range may not be in the parent node of current startContainer.
          focusNode = startContainer->GetParentNode();
          // XXX Looks odd to break the for-loop here because we refer only
          //     first range and another range which starts from different
          //     container, and the latter range is preferred. Why?
          break;
        }
      }
      if (NS_WARN_IF(!focusNode)) {
        return nullptr;
      }
    }
  }

  if (focusNode->GetAsText()) {
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

NS_IMETHODIMP
HTMLEditor::IsAnonymousElement(Element* aElement, bool* aReturn) {
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);
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

nsIContent* HTMLEditor::GetFocusedContent() {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nullptr);

  nsCOMPtr<nsIContent> focusedContent = fm->GetFocusedElement();

  RefPtr<Document> document = GetDocument();
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
               ? focusedContent.get()
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
  return OurWindowHasFocus() ? focusedContent.get() : nullptr;
}

already_AddRefed<nsIContent> HTMLEditor::GetFocusedContentForIME() {
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (!focusedContent) {
    return nullptr;
  }

  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  return document->HasFlag(NODE_IS_EDITABLE) ? nullptr
                                             : focusedContent.forget();
}

bool HTMLEditor::IsActiveInDOMWindow() {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);

  RefPtr<Document> document = GetDocument();
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

Element* HTMLEditor::GetActiveEditingHost() const {
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

  nsINode* focusNode = SelectionRefPtr()->GetFocusNode();
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
  return content->GetEditingHost();
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
  nsIContent* ancestorLimiter = SelectionRefPtr()->GetAncestorLimiter();
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
    // Note that don't call HTMLEditor::InitializeSelectionAncestorLimit() here
    // because it may collapse selection to the first editable node.
    EditorBase::InitializeSelectionAncestorLimit(*editingHost);
  }
}

EventTarget* HTMLEditor::GetDOMEventTarget() {
  // Don't use getDocument here, because we have no way of knowing
  // whether Init() was ever called.  So we need to get the document
  // ourselves, if it exists.
  MOZ_ASSERT(IsInitialized(), "The HTMLEditor has not been initialized yet");
  nsCOMPtr<EventTarget> target = GetDocument();
  return target;
}

bool HTMLEditor::ShouldReplaceRootElement() {
  if (!mRootElement) {
    // If we don't know what is our root element, we should find our root.
    return true;
  }

  // If we temporary set document root element to mRootElement, but there is
  // body element now, we should replace the root element by the body element.
  return mRootElement != GetBodyElement();
}

void HTMLEditor::NotifyRootChanged() {
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }

  RemoveEventListeners();
  nsresult rv = InstallEventListeners();
  if (NS_FAILED(rv)) {
    return;
  }

  UpdateRootElement();
  if (!mRootElement) {
    return;
  }

  rv = MaybeCollapseSelectionAtFirstEditableNode(false);
  if (NS_FAILED(rv)) {
    return;
  }

  // When this editor has focus, we need to reset the selection limiter to
  // new root.  Otherwise, that is going to be done when this gets focus.
  nsCOMPtr<nsINode> node = GetFocusedNode();
  if (node) {
    InitializeSelection(node);
  }

  SyncRealTimeSpell();
}

Element* HTMLEditor::GetBodyElement() {
  MOZ_ASSERT(IsInitialized(), "The HTMLEditor hasn't been initialized yet");
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  return document->GetBody();
}

already_AddRefed<nsINode> HTMLEditor::GetFocusedNode() {
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (!focusedContent) {
    return nullptr;
  }

  // focusedContent might be non-null even fm->GetFocusedContent() is
  // null.  That's the designMode case, and in that case our
  // FocusedContent() returns the root element, but we want to return
  // the document.

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ASSERTION(fm, "Focus manager is null");
  RefPtr<Element> focusedElement = fm->GetFocusedElement();
  if (focusedElement) {
    return focusedElement.forget();
  }

  RefPtr<Document> document = GetDocument();
  return document.forget();
}

bool HTMLEditor::OurWindowHasFocus() {
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);
  nsCOMPtr<mozIDOMWindowProxy> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow) {
    return false;
  }
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return false;
  }
  nsPIDOMWindowOuter* ourWindow = document->GetWindow();
  return ourWindow == focusedWindow;
}

bool HTMLEditor::IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent) {
  if (!EditorBase::IsAcceptableInputEvent(aGUIEvent)) {
    return false;
  }

  // While there is composition, all composition events in its top level window
  // are always fired on the composing editor.  Therefore, if this editor has
  // composition, the composition events should be handled in this editor.
  if (mComposition && aGUIEvent->AsCompositionEvent()) {
    return true;
  }

  RefPtr<EventTarget> target = aGUIEvent->GetOriginalDOMEventTarget();
  nsCOMPtr<nsIContent> content = do_QueryInterface(target);
  if (content) {
    target = content->FindFirstNonChromeOnlyAccessContent();
  }
  NS_ENSURE_TRUE(target, false);

  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return false;
  }

  if (document->HasFlag(NODE_IS_EDITABLE)) {
    // If this editor is in designMode and the event target is the document,
    // the event is for this editor.
    nsCOMPtr<Document> targetDocument = do_QueryInterface(target);
    if (targetDocument) {
      return targetDocument == document;
    }
    // Otherwise, check whether the event target is in this document or not.
    nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);
    NS_ENSURE_TRUE(targetContent, false);
    return document == targetContent->GetUncomposedDoc();
  }

  // This HTML editor is for contenteditable.  We need to check the validity of
  // the target.
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);
  NS_ENSURE_TRUE(targetContent, false);

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
    // active editing host, we should assume that the click event is targetted.
    if (targetContent == document->GetRootElement() &&
        !targetContent->HasFlag(NODE_IS_EDITABLE) &&
        editingHost == document->GetBodyElement()) {
      targetContent = editingHost;
    }
    // If the target element is neither the active editing host nor a descendant
    // of it, we may not be able to handle the event.
    if (!targetContent->IsInclusiveDescendantOf(editingHost)) {
      return false;
    }
    // If the clicked element has an independent selection, we shouldn't
    // handle this click event.
    if (targetContent->HasIndependentSelection()) {
      return false;
    }
    // If the target content is editable, we should handle this event.
    return targetContent->HasFlag(NODE_IS_EDITABLE);
  }

  // If the target of the other events which target focused element isn't
  // editable or has an independent selection, this editor shouldn't handle the
  // event.
  if (!targetContent->HasFlag(NODE_IS_EDITABLE) ||
      targetContent->HasIndependentSelection()) {
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
  if (IsReadonly() || IsDisabled()) {
    aState->mEnabled = IMEState::DISABLED;
  } else {
    aState->mEnabled = IMEState::ENABLED;
  }
  return NS_OK;
}

already_AddRefed<Element> HTMLEditor::GetInputEventTargetElement() {
  RefPtr<Element> target = GetActiveEditingHost();
  return target.forget();
}

Element* HTMLEditor::GetEditorRoot() const { return GetActiveEditingHost(); }

nsHTMLDocument* HTMLEditor::GetHTMLDocument() const {
  Document* doc = GetDocument();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }
  if (!doc->IsHTMLOrXHTML()) {
    return nullptr;
  }
  return doc->AsHTMLDocument();
}

nsresult HTMLEditor::OnModifyDocument() {
  if (IsEditActionDataAvailable()) {
    return OnModifyDocumentInternal();
  }

  AutoEditActionDataSetter editActionData(
      *this, EditAction::eCreatePaddingBRElementForEmptyEditor);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return OnModifyDocumentInternal();
}

nsresult HTMLEditor::OnModifyDocumentInternal() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // EnsureNoPaddingBRElementForEmptyEditor() below may cause a flush, which
  // could destroy the editor
  nsAutoScriptBlockerSuppressNodeRemoved scriptBlocker;

  // Delete our padding <br> element for empty editor, if we have one, since
  // the document might not be empty any more.
  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Failed to remove the padding <br> element");
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return rv;
  }

  // Try to recreate the padding <br> element for empty editor if needed.
  rv = MaybeCreatePaddingBRElementForEmptyEditor();
  NS_WARNING_ASSERTION(
      rv != NS_ERROR_EDITOR_DESTROYED,
      "The editor has been destroyed during creating a padding <br> element");
  return rv;
}

}  // namespace mozilla
