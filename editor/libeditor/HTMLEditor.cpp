/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include "mozilla/ComposerCommandsUpdater.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EventStates.h"
#include "mozilla/mozInlineSpellChecker.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TextEvents.h"

#include "nsCRT.h"

#include "nsUnicharUtils.h"

#include "HTMLEditorEventListener.h"
#include "HTMLEditRules.h"
#include "HTMLEditUtils.h"
#include "HTMLURIRefObject.h"
#include "StyleSheetTransactions.h"
#include "TextEditUtils.h"
#include "TypeInState.h"

#include "nsHTMLDocument.h"
#include "nsIDocumentInlines.h"
#include "nsISelectionController.h"
#include "nsILinkHandler.h"
#include "nsIInlineSpellChecker.h"

#include "mozilla/css/Loader.h"

#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIMutableArray.h"
#include "nsContentUtils.h"
#include "nsIDocumentEncoder.h"
#include "nsGenericHTMLElement.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"

// netwerk
#include "nsIURI.h"
#include "nsNetUtil.h"

// Misc
#include "mozilla/EditorUtils.h"
#include "HTMLEditorObjectResizerUtils.h"
#include "TextEditorTest.h"
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

// Some utilities to handle overloading of "A" tag for link and named anchor.
static bool
IsLinkTag(const nsString& s)
{
  return s.EqualsIgnoreCase("href");
}

static bool
IsNamedAnchorTag(const nsString& s)
{
  return s.EqualsIgnoreCase("anchor") || s.EqualsIgnoreCase("namedanchor");
}

template EditorDOMPoint
HTMLEditor::InsertNodeIntoProperAncestorWithTransaction(
              nsIContent& aNode,
              const EditorDOMPoint& aPointToInsert,
              SplitAtEdges aSplitAtEdges);
template EditorDOMPoint
HTMLEditor::InsertNodeIntoProperAncestorWithTransaction(
              nsIContent& aNode,
              const EditorRawDOMPoint& aPointToInsert,
              SplitAtEdges aSplitAtEdges);

HTMLEditor::HTMLEditor()
  : mCRInParagraphCreatesParagraph(false)
  , mCSSAware(false)
  , mSelectedCellIndex(0)
  , mHasShownResizers(false)
  , mIsObjectResizingEnabled(true)
  , mIsResizing(false)
  , mPreserveRatio(false)
  , mResizedObjectIsAnImage(false)
  , mIsAbsolutelyPositioningEnabled(true)
  , mResizedObjectIsAbsolutelyPositioned(false)
  , mHasShownGrabber(false)
  , mGrabberClicked(false)
  , mIsMoving(false)
  , mSnapToGridEnabled(false)
  , mHasShownInlineTableEditor(false)
  , mIsInlineTableEditingEnabled(true)
  , mOriginalX(0)
  , mOriginalY(0)
  , mResizedObjectX(0)
  , mResizedObjectY(0)
  , mResizedObjectWidth(0)
  , mResizedObjectHeight(0)
  , mResizedObjectMarginLeft(0)
  , mResizedObjectMarginTop(0)
  , mResizedObjectBorderLeft(0)
  , mResizedObjectBorderTop(0)
  , mXIncrementFactor(0)
  , mYIncrementFactor(0)
  , mWidthIncrementFactor(0)
  , mHeightIncrementFactor(0)
  , mResizerUsedCount(0)
  , mGrabberUsedCount(0)
  , mInlineTableEditorUsedCount(0)
  , mInfoXIncrement(20)
  , mInfoYIncrement(20)
  , mPositionedObjectX(0)
  , mPositionedObjectY(0)
  , mPositionedObjectWidth(0)
  , mPositionedObjectHeight(0)
  , mPositionedObjectMarginLeft(0)
  , mPositionedObjectMarginTop(0)
  , mPositionedObjectBorderLeft(0)
  , mPositionedObjectBorderTop(0)
  , mGridSize(0)
  , mDefaultParagraphSeparator(
      Preferences::GetBool("editor.use_div_for_default_newlines", true)
      ? ParagraphSeparator::div : ParagraphSeparator::br)
{
  mIsHTMLEditorClass = true;
}

HTMLEditor::~HTMLEditor()
{
  if (mRules && mRules->AsHTMLEditRules()) {
    mRules->AsHTMLEditRules()->EndListeningToEditActions();
  }

  mTypeInState = nullptr;

  if (mLinkHandler && IsInitialized()) {
    nsCOMPtr<nsIPresShell> ps = GetPresShell();

    if (ps && ps->GetPresContext()) {
      ps->GetPresContext()->SetLinkHandler(mLinkHandler);
    }
  }

  RemoveEventListeners();

  HideAnonymousEditingUIs();

  Telemetry::Accumulate(
    Telemetry::HTMLEDITORS_WITH_RESIZERS,
    mHasShownResizers ? 1 : 0);
  if (mHasShownResizers) {
    Telemetry::Accumulate(
      Telemetry::HTMLEDITORS_WHOSE_RESIZERS_USED_BY_USER,
      mResizerUsedCount);
  }
  Telemetry::Accumulate(
    Telemetry::HTMLEDITORS_WITH_ABSOLUTE_POSITIONER,
    mHasShownGrabber ? 1 : 0);
  if (mHasShownGrabber) {
    Telemetry::Accumulate(
      Telemetry::HTMLEDITORS_WHOSE_ABSOLUTE_POSITIONER_USED_BY_USER,
      mGrabberUsedCount);
  }
  Telemetry::Accumulate(
    Telemetry::HTMLEDITORS_WITH_INLINE_TABLE_EDITOR,
    mHasShownInlineTableEditor ? 1 : 0);
  if (mHasShownInlineTableEditor) {
    Telemetry::Accumulate(
      Telemetry::HTMLEDITORS_WHOSE_INLINE_TABLE_EDITOR_USED_BY_USER,
      mInlineTableEditorUsedCount);
  }
}

void
HTMLEditor::HideAnonymousEditingUIs()
{
  if (mAbsolutelyPositionedObject) {
    HideGrabber();
  }
  if (mInlineEditedCell) {
    HideInlineTableEditingUI();
  }
  if (mResizedObject) {
    HideResizers();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLEditor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLEditor, TextEditor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTypeInState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mComposerCommandsUpdater)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStyleSheets)

  tmp->HideAnonymousEditingUIs();

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLinkHandler)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLEditor, TextEditor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTypeInState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mComposerCommandsUpdater)
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMouseMotionListenerP)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResizeEventListenerP)

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

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLinkHandler)
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
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END_INHERITING(TextEditor)

nsresult
HTMLEditor::Init(nsIDocument& aDoc,
                 Element* aRoot,
                 nsISelectionController* aSelCon,
                 uint32_t aFlags,
                 const nsAString& aInitialValue)
{
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
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
    nsPresContext *context = presShell->GetPresContext();
    NS_ENSURE_TRUE(context, NS_ERROR_NULL_POINTER);
    if (!IsPlaintextEditor() && !IsInteractionAllowed()) {
      mLinkHandler = context->GetLinkHandler();
      context->SetLinkHandler(nullptr);
    }

    // init the type-in state
    mTypeInState = new TypeInState();

    if (!IsInteractionAllowed()) {
      // ignore any errors from this in case the file is missing
      AddOverrideStyleSheet(NS_LITERAL_STRING("resource://gre/res/EditorOverride.css"));
    }
  }
  NS_ENSURE_SUCCESS(rulesRv, rulesRv);

  return NS_OK;
}

void
HTMLEditor::PreDestroy(bool aDestroyingFrames)
{
  if (mDidPreDestroy) {
    return;
  }

  nsCOMPtr<nsIDocument> document = GetDocument();
  if (document) {
    document->RemoveMutationObserver(this);
  }

  while (!mStyleSheetURLs.IsEmpty()) {
    RemoveOverrideStyleSheet(mStyleSheetURLs[0]);
  }

  // Clean up after our anonymous content -- we don't want these nodes to
  // stay around (which they would, since the frames have an owning reference).
  HideAnonymousEditingUIs();

  EditorBase::PreDestroy(aDestroyingFrames);
}

NS_IMETHODIMP
HTMLEditor::NotifySelectionChanged(nsIDocument* aDocument,
                                   Selection* aSelection,
                                   int16_t aReason)
{
  if (NS_WARN_IF(!aDocument) || NS_WARN_IF(!aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mTypeInState) {
    RefPtr<TypeInState> typeInState = mTypeInState;
    typeInState->OnSelectionChange(*aSelection);

    // We used a class which derived from nsISelectionListener to call
    // HTMLEditor::CheckSelectionStateForAnonymousButtons().  The lifetime of
    // the class was exactly same as mTypeInState.  So, call it only when
    // mTypeInState is not nullptr.
    if ((aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                    nsISelectionListener::KEYPRESS_REASON |
                    nsISelectionListener::SELECTALL_REASON)) && aSelection) {
      // the selection changed and we need to check if we have to
      // hide and/or redisplay resizing handles
      // FYI: This is an XPCOM method.  So, the caller, Selection, guarantees
      //      the lifetime of this instance.  So, don't need to grab this with
      //      local variable.
      CheckSelectionStateForAnonymousButtons(aSelection);
    }
  }

  if (mComposerCommandsUpdater) {
    RefPtr<ComposerCommandsUpdater> updater = mComposerCommandsUpdater;
    updater->OnSelectionChange();
  }

  return EditorBase::NotifySelectionChanged(aDocument, aSelection, aReason);
}

void
HTMLEditor::UpdateRootElement()
{
  // Use the HTML documents body element as the editor root if we didn't
  // get a root element during initialization.

  mRootElement = GetBodyElement();
  if (!mRootElement) {
    nsCOMPtr<nsIDocument> doc = GetDocument();
    if (doc) {
      // If there is no HTML body element,
      // we should use the document root element instead.
      mRootElement = doc->GetDocumentElement();
    }
    // else leave it null, for lack of anything better.
  }
}

already_AddRefed<nsIContent>
HTMLEditor::FindSelectionRoot(nsINode* aNode)
{
  if (NS_WARN_IF(!aNode)) {
    return nullptr;
  }

  MOZ_ASSERT(aNode->IsDocument() || aNode->IsContent(),
             "aNode must be content or document node");

  nsCOMPtr<nsIDocument> doc = aNode->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> content;
  if (aNode->IsInUncomposedDoc() &&
      (doc->HasFlag(NODE_IS_EDITABLE) || !aNode->IsContent())) {
    content = doc->GetRootElement();
    return content.forget();
  }
  content = aNode->AsContent();

  // XXX If we have readonly flag, shouldn't return the element which has
  // contenteditable="true"?  However, such case isn't there without chrome
  // permission script.
  if (IsReadonly()) {
    // We still want to allow selection in a readonly editor.
    content = do_QueryInterface(GetRoot());
    return content.forget();
  }

  if (!content->HasFlag(NODE_IS_EDITABLE)) {
    // If the content is in read-write state but is not editable itself,
    // return it as the selection root.
    if (content->IsElement() &&
        content->AsElement()->State().HasState(NS_EVENT_STATE_MOZ_READWRITE)) {
      return content.forget();
    }
    return nullptr;
  }

  // For non-readonly editors we want to find the root of the editable subtree
  // containing aContent.
  content = content->GetEditingHost();
  return content.forget();
}

void
HTMLEditor::CreateEventListeners()
{
  // Don't create the handler twice
  if (!mEventListener) {
    mEventListener = new HTMLEditorEventListener();
  }
}

nsresult
HTMLEditor::InstallEventListeners()
{
  if (NS_WARN_IF(!IsInitialized()) || NS_WARN_IF(!mEventListener)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // NOTE: HTMLEditor doesn't need to initialize mEventTarget here because
  // the target must be document node and it must be referenced as weak pointer.

  HTMLEditorEventListener* listener =
    reinterpret_cast<HTMLEditorEventListener*>(mEventListener.get());
  return listener->Connect(this);
}

void
HTMLEditor::RemoveEventListeners()
{
  if (!IsInitialized()) {
    return;
  }

  RefPtr<EventTarget> target = GetDOMEventTarget();

  if (target) {
    // Both mMouseMotionListenerP and mResizeEventListenerP can be
    // registerd with other targets than the DOM event receiver that
    // we can reach from here. But nonetheless, unregister the event
    // listeners with the DOM event reveiver (if it's registerd with
    // other targets, it'll get unregisterd once the target goes
    // away).

    if (mMouseMotionListenerP) {
      // mMouseMotionListenerP might be registerd either as bubbling or
      // capturing, unregister by both.
      target->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                  mMouseMotionListenerP, false);
      target->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                  mMouseMotionListenerP, true);
    }

    if (mResizeEventListenerP) {
      target->RemoveEventListener(NS_LITERAL_STRING("resize"),
                                  mResizeEventListenerP, false);
    }
  }

  mMouseMotionListenerP = nullptr;
  mResizeEventListenerP = nullptr;

  TextEditor::RemoveEventListeners();
}

NS_IMETHODIMP
HTMLEditor::SetFlags(uint32_t aFlags)
{
  nsresult rv = TextEditor::SetFlags(aFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // Sets mCSSAware to correspond to aFlags. This toggles whether CSS is
  // used to style elements in the editor. Note that the editor is only CSS
  // aware by default in Composer and in the mail editor.
  mCSSAware = !NoCSS() && !IsMailEditor();

  return NS_OK;
}

nsresult
HTMLEditor::InitRules()
{
  if (!mRules) {
    // instantiate the rules for the html editor
    mRules = new HTMLEditRules();
  }
  return mRules->Init(this);
}

NS_IMETHODIMP
HTMLEditor::BeginningOfDocument()
{
  return MaybeCollapseSelectionAtFirstEditableNode(false);
}

void
HTMLEditor::InitializeSelectionAncestorLimit(Selection& aSelection,
                                             nsIContent& aAncestorLimit)
{
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
  if (aSelection.RangeCount() == 1 && aSelection.IsCollapsed()) {
    Element* editingHost = GetActiveEditingHost();
    nsRange* range = aSelection.GetRangeAt(0);
    if (range->GetStartContainer() == editingHost &&
        !range->StartOffset()) {
      // JS or user operation has already collapsed selection at start of
      // the editing host.  So, we don't need to try to change selection
      // in this case.
      tryToCollapseSelectionAtFirstEditableNode = false;
    }
  }

  EditorBase::InitializeSelectionAncestorLimit(aSelection, aAncestorLimit);

  // XXX Do we need to check if we still need to change selection?  E.g.,
  //     we could have already lost focus while we're changing the ancestor
  //     limiter because it may causes "selectionchange" event.
  if (tryToCollapseSelectionAtFirstEditableNode) {
    MaybeCollapseSelectionAtFirstEditableNode(true);
  }
}

nsresult
HTMLEditor::MaybeCollapseSelectionAtFirstEditableNode(
              bool aIgnoreIfSelectionInEditingHost)
{
  // XXX Why doesn't this check if the document is alive?
  if (!IsInitialized()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Selection> selection = GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

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
  if (aIgnoreIfSelectionInEditingHost && selection->RangeCount() == 1) {
    nsRange* range = selection->GetRangeAt(0);
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
    wsObj.NextVisibleNode(pointToPutCaret,
                          address_of(visNode), &visOffset, &visType);

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
      NS_WARNING_ASSERTION(advanced,
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
      NS_WARNING_ASSERTION(advanced,
        "Failed to advance offset from the found empty block node");
    } else {
      pointToPutCaret.Set(visNode, 0);
    }
  }
  return selection->Collapse(pointToPutCaret);
}

nsresult
HTMLEditor::HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent)
{
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
        return NS_OK; // let it be used for focus switching
      }

      if (aKeyboardEvent->IsControl() || aKeyboardEvent->IsAlt() ||
          aKeyboardEvent->IsMeta() || aKeyboardEvent->IsOS()) {
        return NS_OK;
      }

      RefPtr<Selection> selection = GetSelection();
      NS_ENSURE_TRUE(selection && selection->RangeCount(), NS_ERROR_FAILURE);

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
        rv = Indent(aKeyboardEvent->IsShift()
                    ? NS_LITERAL_STRING("outdent")
                    : NS_LITERAL_STRING("indent"));
        handled = true;
      }
      NS_ENSURE_SUCCESS(rv, rv);
      if (handled) {
        aKeyboardEvent->PreventDefault(); // consumed
        return NS_OK;
      }
      if (aKeyboardEvent->IsShift()) {
        return NS_OK; // don't type text for shift tabs
      }
      aKeyboardEvent->PreventDefault();
      return OnInputText(NS_LITERAL_STRING("\t"));
    }
    case NS_VK_RETURN:
      if (!aKeyboardEvent->IsInputtingLineBreak()) {
        return NS_OK;
      }
      aKeyboardEvent->PreventDefault(); // consumed
      if (aKeyboardEvent->IsShift()) {
        // Only inserts a <br> element.
        return OnInputLineBreak();
      }
      // uses rules to figure out what to insert
      return OnInputParagraphSeparator();
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
bool
HTMLEditor::NodeIsBlockStatic(const nsINode* aElement)
{
  MOZ_ASSERT(aElement);

  // We want to treat these as block nodes even though nsHTMLElement says
  // they're not.
  if (aElement->IsAnyOfHTMLElements(nsGkAtoms::body,
                                    nsGkAtoms::head,
                                    nsGkAtoms::tbody,
                                    nsGkAtoms::thead,
                                    nsGkAtoms::tfoot,
                                    nsGkAtoms::tr,
                                    nsGkAtoms::th,
                                    nsGkAtoms::td,
                                    nsGkAtoms::dt,
                                    nsGkAtoms::dd)) {
    return true;
  }

  return nsHTMLElement::IsBlock(
    nsHTMLTags::AtomTagToId(aElement->NodeInfo()->NameAtom()));
}

NS_IMETHODIMP
HTMLEditor::NodeIsBlock(nsINode* aNode,
                        bool* aIsBlock)
{
  *aIsBlock = IsBlockNode(aNode);
  return NS_OK;
}

bool
HTMLEditor::IsBlockNode(nsINode* aNode)
{
  return aNode && NodeIsBlockStatic(aNode);
}

/**
 * GetBlockNodeParent returns enclosing block level ancestor, if any.
 */
Element*
HTMLEditor::GetBlockNodeParent(nsINode* aNode,
                               nsINode* aAncestorLimiter)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(!aAncestorLimiter ||
             aNode == aAncestorLimiter ||
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
Element*
HTMLEditor::GetBlock(nsINode& aNode,
                     nsINode* aAncestorLimiter)
{
  MOZ_ASSERT(!aAncestorLimiter ||
             &aNode == aAncestorLimiter ||
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
void
HTMLEditor::IsNextCharInNodeWhitespace(nsIContent* aContent,
                                       int32_t aOffset,
                                       bool* outIsSpace,
                                       bool* outIsNBSP,
                                       nsIContent** outNode,
                                       int32_t* outOffset)
{
  MOZ_ASSERT(aContent && outIsSpace && outIsNBSP);
  MOZ_ASSERT((outNode && outOffset) || (!outNode && !outOffset));
  *outIsSpace = false;
  *outIsNBSP = false;
  if (outNode && outOffset) {
    *outNode = nullptr;
    *outOffset = -1;
  }

  if (aContent->IsText() &&
      (uint32_t)aOffset < aContent->Length()) {
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
void
HTMLEditor::IsPrevCharInNodeWhitespace(nsIContent* aContent,
                                       int32_t aOffset,
                                       bool* outIsSpace,
                                       bool* outIsNBSP,
                                       nsIContent** outNode,
                                       int32_t* outOffset)
{
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

bool
HTMLEditor::IsVisibleBRElement(nsINode* aNode)
{
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
  nsCOMPtr<nsINode> unused;
  int32_t visOffset = 0;
  WSType visType;
  wsObj.NextVisibleNode(EditorRawDOMPoint(selNode, selOffset),
                        address_of(unused), &visOffset, &visType);
  if (visType & WSType::block) {
    return false;
  }

  return true;
}

NS_IMETHODIMP
HTMLEditor::UpdateBaseURL()
{
  nsCOMPtr<nsIDocument> doc = GetDocument();
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

nsresult
HTMLEditor::OnInputLineBreak()
{
  AutoPlaceholderBatch batch(this, nsGkAtoms::TypingTxnName);
  nsresult rv = InsertBrElementAtSelectionWithTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
HTMLEditor::TabInTable(bool inIsShift,
                       bool* outHandled)
{
  NS_ENSURE_TRUE(outHandled, NS_ERROR_NULL_POINTER);
  *outHandled = false;

  // Find enclosing table cell from selection (cell may be selected element)
  nsCOMPtr<Element> cellElement =
    GetElementOrParentByTagName(NS_LITERAL_STRING("td"), nullptr);
  // Do nothing -- we didn't find a table cell
  NS_ENSURE_TRUE(cellElement, NS_OK);

  // find enclosing table
  nsCOMPtr<Element> table = GetEnclosingTable(cellElement);
  NS_ENSURE_TRUE(table, NS_OK);

  // advance to next cell
  // first create an iterator over the table
  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();
  nsresult rv = iter->Init(table);
  NS_ENSURE_SUCCESS(rv, rv);
  // position iter at block
  rv = iter->PositionAt(cellElement);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINode> node;
  do {
    if (inIsShift) {
      iter->Prev();
    } else {
      iter->Next();
    }

    node = iter->GetCurrentNode();

    if (node && HTMLEditUtils::IsTableCell(node) &&
        GetEnclosingTable(node) == table) {
      CollapseSelectionToDeepestNonTableFirstChild(nullptr, node);
      *outHandled = true;
      return NS_OK;
    }
  } while (!iter->IsDone());

  if (!(*outHandled) && !inIsShift) {
    // If we haven't handled it yet, then we must have run off the end of the
    // table.  Insert a new row.
    rv = InsertTableRow(1, true);
    NS_ENSURE_SUCCESS(rv, rv);
    *outHandled = true;
    // Put selection in right place.  Use table code to get selection and index
    // to new row...
    RefPtr<Selection> selection;
    RefPtr<Element> tblElement, cell;
    int32_t row;
    rv = GetCellContext(getter_AddRefs(selection),
                        getter_AddRefs(tblElement),
                        getter_AddRefs(cell),
                        nullptr, nullptr,
                        &row, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
    // ...so that we can ask for first cell in that row...
    rv = GetCellAt(tblElement, row, 0, getter_AddRefs(cell));
    NS_ENSURE_SUCCESS(rv, rv);
    // ...and then set selection there.  (Note that normally you should use
    // CollapseSelectionToDeepestNonTableFirstChild(), but we know cell is an
    // empty new cell, so this works fine)
    if (cell) {
      selection->Collapse(cell, 0);
    }
  }

  return NS_OK;
}

nsresult
HTMLEditor::InsertBrElementAtSelectionWithTransaction()
{
  // calling it text insertion to trigger moz br treatment by rules
  AutoRules beginRulesSniffing(this, EditAction::insertText, nsIEditor::eNext);

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  if (!selection->IsCollapsed()) {
    nsresult rv = DeleteSelectionAsAction(eNone, eStrip);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  EditorRawDOMPoint atStartOfSelection(GetStartPoint(selection));
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // InsertBrElementWithTransaction() will set selection after the new <br>
  // element.
  RefPtr<Element> newBrElement =
    InsertBrElementWithTransaction(*selection, atStartOfSelection, eNext);
  if (NS_WARN_IF(!newBrElement)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
HTMLEditor::CollapseSelectionToDeepestNonTableFirstChild(Selection* aSelection,
                                                         nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  RefPtr<Selection> selection = aSelection;
  if (!selection) {
    selection = GetSelection();
  }
  if (!selection) {
    // Nothing to do
    return;
  }

  nsCOMPtr<nsINode> node = aNode;

  for (nsCOMPtr<nsIContent> child = node->GetFirstChild();
       child;
       child = child->GetFirstChild()) {
    // Stop if we find a table, don't want to go into nested tables
    if (HTMLEditUtils::IsTable(child) || !IsContainer(child)) {
      break;
    }
    node = child;
  }

  selection->Collapse(node, 0);
}


/**
 * This is mostly like InsertHTMLWithCharsetAndContext, but we can't use that
 * because it is selection-based and the rules code won't let us edit under the
 * <head> node
 */
NS_IMETHODIMP
HTMLEditor::ReplaceHeadContentsWithHTML(const nsAString& aSourceToInsert)
{
  // don't do any post processing, rules get confused
  AutoRules beginRulesSniffing(this, EditAction::ignore, nsIEditor::eNone);
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  CommitComposition();

  // Do not use AutoRules -- rules code won't let us insert in <head>.  Use
  // the head node as a parent and delete/insert directly.
  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsContentList> nodeList =
    document->GetElementsByTagName(NS_LITERAL_STRING("head"));
  NS_ENSURE_TRUE(nodeList, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> headNode = nodeList->Item(0);
  NS_ENSURE_TRUE(headNode, NS_ERROR_NULL_POINTER);

  // First, make sure there are no return chars in the source.  Bad things
  // happen if you insert returns (instead of dom newlines, \n) into an editor
  // document.
  nsAutoString inputString (aSourceToInsert);  // hope this does copy-on-write

  // Windows linebreaks: Map CRLF to LF:
  inputString.ReplaceSubstring(u"\r\n", u"\n");

  // Mac linebreaks: Map any remaining CR to LF:
  inputString.ReplaceSubstring(u"\r", u"\n");

  AutoPlaceholderBatch beginBatching(this);

  // Get the first range in the selection, for context:
  RefPtr<nsRange> range = selection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

  ErrorResult err;
  RefPtr<DocumentFragment> docfrag =
    range->CreateContextualFragment(inputString, err);

  // XXXX BUG 50965: This is not returning the text between <title>...</title>
  // Special code is needed in JS to handle title anyway, so it doesn't matter!

  if (err.Failed()) {
#ifdef DEBUG
    printf("Couldn't create contextual fragment: error was %X\n",
           err.ErrorCodeAsInt());
#endif
    return err.StealNSResult();
  }
  NS_ENSURE_TRUE(docfrag, NS_ERROR_NULL_POINTER);

  // First delete all children in head
  while (nsCOMPtr<nsIContent> child = headNode->GetFirstChild()) {
    nsresult rv = DeleteNodeWithTransaction(*child);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Now insert the new nodes
  int32_t offsetOfNewNode = 0;

  // Loop over the contents of the fragment and move into the document
  while (nsCOMPtr<nsIContent> child = docfrag->GetFirstChild()) {
    nsresult rv =
      InsertNodeWithTransaction(*child,
                                EditorRawDOMPoint(headNode, offsetOfNewNode++));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::RebuildDocumentFromSource(const nsAString& aSourceString)
{
  CommitComposition();

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

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
  AutoPlaceholderBatch beginBatching(this);

  nsReadingIterator<char16_t> endtotal;
  aSourceString.EndReading(endtotal);

  if (foundhead) {
    if (foundclosehead) {
      nsresult rv =
        ReplaceHeadContentsWithHTML(Substring(beginhead, beginclosehead));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (foundbody) {
      nsresult rv =
        ReplaceHeadContentsWithHTML(Substring(beginhead, beginbody));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // XXX Without recourse to some parser/content sink/docshell hackery we
      // don't really know where the head ends and the body begins so we assume
      // that there is no body
      nsresult rv = ReplaceHeadContentsWithHTML(Substring(beginhead, endtotal));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  } else {
    nsReadingIterator<char16_t> begintotal;
    aSourceString.BeginReading(begintotal);
    NS_NAMED_LITERAL_STRING(head, "<head>");
    if (foundclosehead) {
      nsresult rv =
        ReplaceHeadContentsWithHTML(head + Substring(begintotal,
                                                     beginclosehead));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (foundbody) {
      nsresult rv = ReplaceHeadContentsWithHTML(head + Substring(begintotal,
                                                                 beginbody));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // XXX Without recourse to some parser/content sink/docshell hackery we
      // don't really know where the head ends and the body begins so we assume
      // that there is no head
      nsresult rv = ReplaceHeadContentsWithHTML(head);
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

    RefPtr<Element> divElement =
      CreateElementWithDefaults(NS_LITERAL_STRING("div"));
    if (NS_WARN_IF(!divElement)) {
      return NS_ERROR_FAILURE;
    }
    CloneAttributesWithTransaction(*rootElement, *divElement);

    return BeginningOfDocument();
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

  RefPtr<nsRange> range = selection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  ErrorResult erv;
  RefPtr<DocumentFragment> docfrag =
    range->CreateContextualFragment(bodyTag, erv);
  NS_ENSURE_TRUE(!erv.Failed(), erv.StealNSResult());
  NS_ENSURE_TRUE(docfrag, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> child = docfrag->GetFirstChild();
  NS_ENSURE_TRUE(child && child->IsElement(), NS_ERROR_NULL_POINTER);

  // Copy all attributes from the div child to current body element
  CloneAttributesWithTransaction(*rootElement, *child->AsElement());

  // place selection at first editable content
  return BeginningOfDocument();
}

EditorRawDOMPoint
HTMLEditor::GetBetterInsertionPointFor(nsINode& aNodeToInsert,
                                       const EditorRawDOMPoint& aPointToInsert)
{
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

  WSRunObject wsObj(this, pointToInsert.GetContainer(),
                    pointToInsert.Offset());

  // If the insertion position is after the last visible item in a line,
  // i.e., the insertion position is just before a visible line break <br>,
  // we want to skip to the position just after the line break (see bug 68767).
  nsCOMPtr<nsINode> nextVisibleNode;
  int32_t nextVisibleOffset = 0;
  WSType nextVisibleType;
  wsObj.NextVisibleNode(pointToInsert, address_of(nextVisibleNode),
                        &nextVisibleOffset, &nextVisibleType);
  // So, if the next visible node isn't a <br> element, we can insert the block
  // level element to the point.
  if (!nextVisibleNode ||
      !(nextVisibleType & WSType::br)) {
    return pointToInsert;
  }

  // However, we must not skip next <br> element when the caret appears to be
  // positioned at the beginning of a block, in that case skipping the <br>
  // would not insert the <br> at the caret position, but after the current
  // empty line.
  nsCOMPtr<nsINode> previousVisibleNode;
  int32_t previousVisibleOffset = 0;
  WSType previousVisibleType;
  wsObj.PriorVisibleNode(pointToInsert, address_of(previousVisibleNode),
                         &previousVisibleOffset, &previousVisibleType);
  // So, if there is no previous visible node,
  // or, if both nodes of the insertion point is <br> elements,
  // or, if the previous visible node is different block,
  // we need to skip the following <br>.  So, otherwise, we can insert the
  // block at the insertion point.
  if (!previousVisibleNode ||
      (previousVisibleType & WSType::br) ||
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
HTMLEditor::InsertElementAtSelection(Element* aElement,
                                     bool aDeleteSelection)
{
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  CommitComposition();
  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::insertElement,
                               nsIEditor::eNext);

  RefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  // hand off to the rules system, see if it has anything to say about this
  bool cancel, handled;
  RulesInfo ruleInfo(EditAction::insertElement);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
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
        rv = DeleteSelectionAsAction(eNone, eNoStrip);
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
        selection->CollapseToStart(IgnoreErrors());
      } else {
        selection->CollapseToEnd(IgnoreErrors());
      }
    }

    if (selection->GetAnchorNode()) {
      EditorRawDOMPoint atAnchor(selection->AnchorRef());
      // Adjust position based on the node we are going to insert.
      EditorRawDOMPoint pointToInsert =
        GetBetterInsertionPointFor(*aElement, atAnchor);
      if (NS_WARN_IF(!pointToInsert.IsSet())) {
        return NS_ERROR_FAILURE;
      }

      EditorDOMPoint insertedPoint =
        InsertNodeIntoProperAncestorWithTransaction(
          *aElement, pointToInsert, SplitAtEdges::eAllowToCreateEmptyContainer);
      if (NS_WARN_IF(!insertedPoint.IsSet())) {
        return NS_ERROR_FAILURE;
      }
      // Set caret after element, but check for special case
      //  of inserting table-related elements: set in first cell instead
      if (!SetCaretInTableCell(aElement)) {
        rv = SetCaretAfterElement(aElement);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // check for inserting a whole table at the end of a block. If so insert
      // a br after it.
      if (HTMLEditUtils::IsTable(aElement) &&
          IsLastEditableChild(aElement)) {
        DebugOnly<bool> advanced = insertedPoint.AdvanceOffset();
        NS_WARNING_ASSERTION(advanced,
          "Failed to advance offset from inserted point");
        // Collapse selection to the new <br> element node after creating it.
        RefPtr<Element> newBrElement =
          InsertBrElementWithTransaction(*selection, insertedPoint, ePrevious);
        if (NS_WARN_IF(!newBrElement)) {
          return NS_ERROR_FAILURE;
        }
      }
    }
  }
  rv = rules->DidDoAction(selection, &ruleInfo, rv);
  return rv;
}

template<typename PT, typename CT>
EditorDOMPoint
HTMLEditor::InsertNodeIntoProperAncestorWithTransaction(
              nsIContent& aNode,
              const EditorDOMPointBase<PT, CT>& aPointToInsert,
              SplitAtEdges aSplitAtEdges)
{
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
      SplitNodeDeepWithTransaction(*pointToInsert.GetChild(),
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
HTMLEditor::SelectElement(Element* aElement)
{
  // Must be sure that element is contained in the document body
  if (!IsDescendantOfEditorRoot(aElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsINode* parent = aElement->GetParentNode();
  if (NS_WARN_IF(!parent)) {
    return NS_ERROR_FAILURE;
  }

  int32_t offsetInParent = parent->ComputeIndexOf(aElement);

  // Collapse selection to just before desired element,
  nsresult rv = selection->Collapse(parent, offsetInParent);
  if (NS_SUCCEEDED(rv)) {
    // then extend it to just after
    rv = selection->Extend(parent, offsetInParent + 1);
  }
  return rv;
}

NS_IMETHODIMP
HTMLEditor::SetCaretAfterElement(Element* aElement)
{
  // Be sure the element is contained in the document body
  if (!aElement || !IsDescendantOfEditorRoot(aElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsINode> parent = aElement->GetParentNode();
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
  // Collapse selection to just after desired element,
  EditorRawDOMPoint afterElement(aElement);
  if (NS_WARN_IF(!afterElement.AdvanceOffset())) {
    return NS_ERROR_FAILURE;
  }
  return selection->Collapse(afterElement);
}

NS_IMETHODIMP
HTMLEditor::SetParagraphFormat(const nsAString& aParagraphFormat)
{
  nsAutoString lowerCaseTagName(aParagraphFormat);
  ToLowerCase(lowerCaseTagName);
  RefPtr<nsAtom> tagName = NS_Atomize(lowerCaseTagName);
  MOZ_ASSERT(tagName);
  if (tagName == nsGkAtoms::dd || tagName == nsGkAtoms::dt) {
    return MakeDefinitionListItemWithTransaction(*tagName);
  }
  return InsertBasicBlockWithTransaction(*tagName);
}

NS_IMETHODIMP
HTMLEditor::GetParagraphState(bool* aMixed,
                              nsAString& outFormat)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  RefPtr<HTMLEditRules> htmlRules(mRules->AsHTMLEditRules());
  return htmlRules->GetParagraphState(aMixed, outFormat);
}

NS_IMETHODIMP
HTMLEditor::GetBackgroundColorState(bool* aMixed,
                                    nsAString& aOutColor)
{
  if (IsCSSEnabled()) {
    // if we are in CSS mode, we have to check if the containing block defines
    // a background color
    return GetCSSBackgroundColorState(aMixed, aOutColor, true);
  }
  // in HTML mode, we look only at page's background
  return GetHTMLBackgroundColorState(aMixed, aOutColor);
}

NS_IMETHODIMP
HTMLEditor::GetHighlightColorState(bool* aMixed,
                                   nsAString& aOutColor)
{
  *aMixed = false;
  aOutColor.AssignLiteral("transparent");
  if (!IsCSSEnabled()) {
    return NS_OK;
  }

  // in CSS mode, text background can be added by the Text Highlight button
  // we need to query the background of the selection without looking for
  // the block container of the ranges in the selection
  return GetCSSBackgroundColorState(aMixed, aOutColor, false);
}

nsresult
HTMLEditor::GetCSSBackgroundColorState(bool* aMixed,
                                       nsAString& aOutColor,
                                       bool aBlockLevel)
{
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  // the default background color is transparent
  aOutColor.AssignLiteral("transparent");

  // get selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection && selection->GetRangeAt(0));

  // get selection location
  nsCOMPtr<nsINode> parent = selection->GetRangeAt(0)->GetStartContainer();
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  // is the selection collapsed?
  nsCOMPtr<nsINode> nodeToExamine;
  if (selection->IsCollapsed() || IsTextNode(parent)) {
    // we want to look at the parent and ancestors
    nodeToExamine = parent;
  } else {
    // otherwise we want to look at the first editable node after
    // {parent,offset} and its ancestors for divs with alignment on them
    nodeToExamine = selection->GetRangeAt(0)->GetChildAtStartOffset();
    //GetNextNode(parent, offset, true, address_of(nodeToExamine));
  }

  NS_ENSURE_TRUE(nodeToExamine, NS_ERROR_NULL_POINTER);

  if (aBlockLevel) {
    // we are querying the block background (and not the text background), let's
    // climb to the block container
    nsCOMPtr<Element> blockParent = GetBlock(*nodeToExamine);
    NS_ENSURE_TRUE(blockParent, NS_OK);

    // Make sure to not walk off onto the Document node
    do {
      // retrieve the computed style of background-color for blockParent
      CSSEditUtils::GetComputedProperty(*blockParent,
                                        *nsGkAtoms::backgroundColor,
                                        aOutColor);
      blockParent = blockParent->GetParentElement();
      // look at parent if the queried color is transparent and if the node to
      // examine is not the root of the document
    } while (aOutColor.EqualsLiteral("transparent") && blockParent);
    if (aOutColor.EqualsLiteral("transparent")) {
      // we have hit the root of the document and the color is still transparent !
      // Grumble... Let's look at the default background color because that's the
      // color we are looking for
      CSSEditUtils::GetDefaultBackgroundColor(aOutColor);
    }
  }
  else {
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
        // yes it is a block; in that case, the text background color is transparent
        aOutColor.AssignLiteral("transparent");
        break;
      } else {
        // no, it's not; let's retrieve the computed style of background-color for the
        // node to examine
        CSSEditUtils::GetComputedProperty(*nodeToExamine,
                                          *nsGkAtoms::backgroundColor,
                                          aOutColor);
        if (!aOutColor.EqualsLiteral("transparent")) {
          break;
        }
      }
      nodeToExamine = nodeToExamine->GetParentNode();
    } while ( aOutColor.EqualsLiteral("transparent") && nodeToExamine );
  }
  return NS_OK;
}

nsresult
HTMLEditor::GetHTMLBackgroundColorState(bool* aMixed,
                                        nsAString& aOutColor)
{
  //TODO: We don't handle "mixed" correctly!
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  aOutColor.Truncate();

  RefPtr<Element> element;
  int32_t selectedCount;
  nsAutoString tagName;
  nsresult rv = GetSelectedOrParentTableElement(tagName,
                                                &selectedCount,
                                                getter_AddRefs(element));
  NS_ENSURE_SUCCESS(rv, rv);

  while (element) {
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
    // from nested cells/tables, so search up parent chain
    element = element->GetParentElement();
  }

  // If no table or cell found, get page body
  dom::Element* bodyElement = GetRoot();
  NS_ENSURE_TRUE(bodyElement, NS_ERROR_NULL_POINTER);

  bodyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::bgcolor, aOutColor);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetListState(bool* aMixed,
                         bool* aOL,
                         bool* aUL,
                         bool* aDL)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ENSURE_TRUE(aMixed && aOL && aUL && aDL, NS_ERROR_NULL_POINTER);
  RefPtr<HTMLEditRules> htmlRules(mRules->AsHTMLEditRules());
  return htmlRules->GetListState(aMixed, aOL, aUL, aDL);
}

NS_IMETHODIMP
HTMLEditor::GetListItemState(bool* aMixed,
                             bool* aLI,
                             bool* aDT,
                             bool* aDD)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ENSURE_TRUE(aMixed && aLI && aDT && aDD, NS_ERROR_NULL_POINTER);

  RefPtr<HTMLEditRules> htmlRules(mRules->AsHTMLEditRules());
  return htmlRules->GetListItemState(aMixed, aLI, aDT, aDD);
}

NS_IMETHODIMP
HTMLEditor::GetAlignment(bool* aMixed,
                         nsIHTMLEditor::EAlignment* aAlign)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ENSURE_TRUE(aMixed && aAlign, NS_ERROR_NULL_POINTER);

  RefPtr<HTMLEditRules> htmlRules(mRules->AsHTMLEditRules());
  return htmlRules->GetAlignment(aMixed, aAlign);
}

NS_IMETHODIMP
HTMLEditor::GetIndentState(bool* aCanIndent,
                           bool* aCanOutdent)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ENSURE_TRUE(aCanIndent && aCanOutdent, NS_ERROR_NULL_POINTER);

  RefPtr<HTMLEditRules> htmlRules(mRules->AsHTMLEditRules());
  return htmlRules->GetIndentState(aCanIndent, aCanOutdent);
}

NS_IMETHODIMP
HTMLEditor::MakeOrChangeList(const nsAString& aListType,
                             bool entireList,
                             const nsAString& aBulletType)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  bool cancel, handled;

  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::makeList, nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  RulesInfo ruleInfo(EditAction::makeList);
  ruleInfo.blockType = &aListType;
  ruleInfo.entireList = entireList;
  ruleInfo.bulletType = &aBulletType;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled && selection->IsCollapsed()) {
    nsRange* firstRange = selection->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorRawDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet()) ||
        NS_WARN_IF(!atStartOfSelection.GetContainerAsContent())) {
      return NS_ERROR_FAILURE;
    }

    // Have to find a place to put the list.
    EditorDOMPoint pointToInsertList(atStartOfSelection);

    RefPtr<nsAtom> listAtom = NS_Atomize(aListType);
    while (!CanContainTag(*pointToInsertList.GetContainer(), *listAtom)) {
      pointToInsertList.Set(pointToInsertList.GetContainer());
      if (NS_WARN_IF(!pointToInsertList.IsSet()) ||
          NS_WARN_IF(!pointToInsertList.GetContainerAsContent())) {
        return NS_ERROR_FAILURE;
      }
    }

    if (pointToInsertList.GetContainer() != atStartOfSelection.GetContainer()) {
      // We need to split up to the child of parent.
      SplitNodeResult splitNodeResult =
        SplitNodeDeepWithTransaction(
          *pointToInsertList.GetChild(), atStartOfSelection,
          SplitAtEdges::eAllowToCreateEmptyContainer);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
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
    EditorRawDOMPoint atStartOfNewList(newList, 0);
    RefPtr<Element> newItem =
      CreateNodeWithTransaction(*nsGkAtoms::li, atStartOfNewList);
    if (NS_WARN_IF(!newItem)) {
      return NS_ERROR_FAILURE;
    }
    ErrorResult error;
    selection->Collapse(RawRangeBoundary(newItem, 0), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  return rules->DidDoAction(selection, &ruleInfo, rv);
}

NS_IMETHODIMP
HTMLEditor::RemoveList(const nsAString& aListType)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  bool cancel, handled;

  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::removeList, nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  RulesInfo ruleInfo(EditAction::removeList);
  if (aListType.LowerCaseEqualsLiteral("ol")) {
    ruleInfo.bOrdered = true;
  } else {
    ruleInfo.bOrdered = false;
  }
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  // no default behavior for this yet.  what would it mean?

  return rules->DidDoAction(selection, &ruleInfo, rv);
}

nsresult
HTMLEditor::MakeDefinitionListItemWithTransaction(nsAtom& aTagName)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(&aTagName == nsGkAtoms::dt ||
             &aTagName == nsGkAtoms::dd);

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  bool cancel, handled;

  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::makeDefListItem,
                               nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  nsDependentAtomString tagName(&aTagName);
  RulesInfo ruleInfo(EditAction::makeDefListItem);
  ruleInfo.blockType = &tagName;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled) {
    // todo: no default for now.  we count on rules to handle it.
  }

  return rules->DidDoAction(selection, &ruleInfo, rv);
}

nsresult
HTMLEditor::InsertBasicBlockWithTransaction(nsAtom& aTagName)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(&aTagName != nsGkAtoms::dd &&
             &aTagName != nsGkAtoms::dt);

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  bool cancel, handled;

  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::makeBasicBlock,
                               nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsDependentAtomString tagName(&aTagName);
  RulesInfo ruleInfo(EditAction::makeBasicBlock);
  ruleInfo.blockType = &tagName;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled && selection->IsCollapsed()) {
    nsRange* firstRange = selection->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorRawDOMPoint atStartOfSelection(firstRange->StartRef());
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
      SplitNodeResult splitBlockResult =
        SplitNodeDeepWithTransaction(
          *pointToInsertBlock.GetChild(), atStartOfSelection,
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
    selection->Collapse(RawRangeBoundary(newBlock, 0), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  return rules->DidDoAction(selection, &ruleInfo, rv);
}

NS_IMETHODIMP
HTMLEditor::Indent(const nsAString& aIndent)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  bool cancel, handled;
  EditAction opID = EditAction::indent;
  if (aIndent.LowerCaseEqualsLiteral("outdent")) {
    opID = EditAction::outdent;
  }
  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, opID, nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  RulesInfo ruleInfo(opID);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled && selection->IsCollapsed() && aIndent.EqualsLiteral("indent")) {
    nsRange* firstRange = selection->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorRawDOMPoint atStartOfSelection(firstRange->StartRef());
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
      SplitNodeResult splitBlockquoteResult =
        SplitNodeDeepWithTransaction(
          *pointToInsertBlockquote.GetChild(), atStartOfSelection,
          SplitAtEdges::eAllowToCreateEmptyContainer);
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
    RefPtr<Element> newBQ =
      CreateNodeWithTransaction(*nsGkAtoms::blockquote,
                                pointToInsertBlockquote);
    if (NS_WARN_IF(!newBQ)) {
      return NS_ERROR_FAILURE;
    }
    // put a space in it so layout will draw the list item
    ErrorResult error;
    selection->Collapse(RawRangeBoundary(newBQ, 0), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    rv = InsertTextAsAction(NS_LITERAL_STRING(" "));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Reposition selection to before the space character.
    firstRange = selection->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }
    selection->Collapse(RawRangeBoundary(firstRange->GetStartContainer(), 0),
                        error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }
  return rules->DidDoAction(selection, &ruleInfo, rv);
}

//TODO: IMPLEMENT ALIGNMENT!

NS_IMETHODIMP
HTMLEditor::Align(const nsAString& aAlignType)
{
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::align, nsIEditor::eNext);

  bool cancel, handled;

  // Find out if the selection is collapsed:
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  RulesInfo ruleInfo(EditAction::align);
  ruleInfo.alignType = &aAlignType;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  return rules->DidDoAction(selection, &ruleInfo, rv);
}

already_AddRefed<Element>
HTMLEditor::GetElementOrParentByTagName(const nsAString& aTagName,
                                        nsINode* aNode)
{
  MOZ_ASSERT(!aTagName.IsEmpty());

  nsCOMPtr<nsINode> node = aNode;
  if (!node) {
    // If no node supplied, get it from anchor node of current selection
    RefPtr<Selection> selection = GetSelection();
    if (NS_WARN_IF(!selection)) {
      return nullptr;
    }

    const EditorDOMPoint atAnchor(selection->AnchorRef());
    if (NS_WARN_IF(!atAnchor.IsSet())) {
      return nullptr;
    }

    // Try to get the actual selected node
    if (atAnchor.GetContainer()->HasChildNodes() &&
        atAnchor.GetContainerAsContent()) {
      node = atAnchor.GetChild();
    }
    // Anchor node is probably a text node - just use that
    if (!node) {
      node = atAnchor.GetContainer();
    }
  }

  nsCOMPtr<Element> current;
  if (node->IsElement()) {
    current = node->AsElement();
  } else if (node->GetParentElement()) {
    current = node->GetParentElement();
  } else {
    // Neither aNode nor its parent is an element, so no ancestor is
    MOZ_ASSERT(!node->GetParentNode() ||
               !node->GetParentNode()->GetParentNode());
    return nullptr;
  }

  nsAutoString tagName(aTagName);
  ToLowerCase(tagName);
  bool getLink = IsLinkTag(tagName);
  bool getNamedAnchor = IsNamedAnchorTag(tagName);
  if (getLink || getNamedAnchor) {
    tagName.Assign('a');
  }
  bool findTableCell = tagName.EqualsLiteral("td");
  bool findList = tagName.EqualsLiteral("list");

  for (; current; current = current->GetParentElement()) {
    // Test if we have a link (an anchor with href set)
    if ((getLink && HTMLEditUtils::IsLink(current)) ||
        (getNamedAnchor && HTMLEditUtils::IsNamedAnchor(current))) {
      return current.forget();
    }
    if (findList) {
      // Match "ol", "ul", or "dl" for lists
      if (HTMLEditUtils::IsList(current)) {
        return current.forget();
      }
    } else if (findTableCell) {
      // Table cells are another special case: match either "td" or "th"
      if (HTMLEditUtils::IsTableCell(current)) {
        return current.forget();
      }
    } else if (current->NodeName().Equals(tagName,
                   nsCaseInsensitiveStringComparator())) {
      return current.forget();
    }

    // Stop searching if parent is a body tag.  Note: Originally used IsRoot to
    // stop at table cells, but that's too messy when you are trying to find
    // the parent table
    if (current->GetParentElement() &&
        current->GetParentElement()->IsHTMLElement(nsGkAtoms::body)) {
      break;
    }
  }

  return nullptr;
}

NS_IMETHODIMP
HTMLEditor::GetElementOrParentByTagName(const nsAString& aTagName,
                                        nsINode* aNode,
                                        Element** aReturn)
{
  NS_ENSURE_TRUE(!aTagName.IsEmpty(), NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aReturn, NS_ERROR_NULL_POINTER);

  RefPtr<Element> parent = GetElementOrParentByTagName(aTagName, aNode);

  if (!parent) {
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  parent.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetSelectedElement(const nsAString& aTagName,
                               nsISupports** aReturn)
{
  NS_ENSURE_TRUE(aReturn , NS_ERROR_NULL_POINTER);

  // default is null - no element found
  *aReturn = nullptr;

  // First look for a single element in selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  bool isCollapsed = selection->IsCollapsed();

  nsAutoString domTagName;
  nsAutoString TagName(aTagName);
  ToLowerCase(TagName);
  // Empty string indicates we should match any element tag
  bool anyTag = (TagName.IsEmpty());
  bool isLinkTag = IsLinkTag(TagName);
  bool isNamedAnchorTag = IsNamedAnchorTag(TagName);

  RefPtr<nsRange> range = selection->GetRangeAt(0);
  NS_ENSURE_STATE(range);

  nsCOMPtr<nsINode> startContainer = range->GetStartContainer();
  nsIContent* startNode = range->GetChildAtStartOffset();

  nsCOMPtr<nsINode> endContainer = range->GetEndContainer();
  nsIContent* endNode = range->GetChildAtEndOffset();

  // Optimization for a single selected element
  if (startContainer && startContainer == endContainer &&
      startNode && endNode && startNode->GetNextSibling() == endNode) {
    nsCOMPtr<nsINode> selectedNode = startNode;
    if (selectedNode) {
      domTagName = selectedNode->NodeName();
      ToLowerCase(domTagName);

      // Test for appropriate node type requested
      if (anyTag || (TagName == domTagName) ||
          (isLinkTag && HTMLEditUtils::IsLink(selectedNode)) ||
          (isNamedAnchorTag && HTMLEditUtils::IsNamedAnchor(selectedNode))) {
        selectedNode.forget(aReturn);
        return NS_OK;
      }
    }
  }

  bool bNodeFound = false;
  nsCOMPtr<Element> selectedElement;
  if (isLinkTag) {
    // Link tag is a special case - we return the anchor node
    //  found for any selection that is totally within a link,
    //  included a collapsed selection (just a caret in a link)
    const RangeBoundary& anchor = selection->AnchorRef();
    const RangeBoundary& focus = selection->FocusRef();
    // Link node must be the same for both ends of selection
    if (anchor.IsSet()) {
      RefPtr<Element> parentLinkOfAnchor =
        GetElementOrParentByTagName(NS_LITERAL_STRING("href"),
                                    anchor.Container());
      // XXX: ERROR_HANDLING  can parentLinkOfAnchor be null?
      if (parentLinkOfAnchor) {
        if (isCollapsed) {
          // We have just a caret in the link
          bNodeFound = true;
        } else if (focus.IsSet()) {
          // Link node must be the same for both ends of selection.
          RefPtr<Element> parentLinkOfFocus =
            GetElementOrParentByTagName(NS_LITERAL_STRING("href"),
                                        focus.Container());
          if (parentLinkOfFocus == parentLinkOfAnchor) {
            bNodeFound = true;
          }
        }

        // We found a link node parent
        if (bNodeFound) {
          parentLinkOfAnchor.forget(aReturn);
          return NS_OK;
        }
      } else if (anchor.GetChildAtOffset() && focus.GetChildAtOffset()) {
        // Check if link node is the only thing selected
        if (HTMLEditUtils::IsLink(anchor.GetChildAtOffset()) &&
            anchor.Container() == focus.Container() &&
            focus.GetChildAtOffset() ==
              anchor.GetChildAtOffset()->GetNextSibling()) {
          selectedElement = do_QueryInterface(anchor.GetChildAtOffset());
          bNodeFound = true;
        }
      }
    }
  }

  if (!isCollapsed) {
    RefPtr<nsRange> currange = selection->GetRangeAt(0);
    if (currange) {
      nsresult rv;
      nsCOMPtr<nsIContentIterator> iter =
        do_CreateInstance("@mozilla.org/content/post-content-iterator;1",
                          &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      iter->Init(currange);
      // loop through the content iterator for each content node
      while (!iter->IsDone()) {
        // Query interface to cast nsIContent to Element
        //  then get tagType to compare to  aTagName
        // Clone node of each desired type and append it to the aDomFrag
        nsINode* currentNode = iter->GetCurrentNode();
        selectedElement = currentNode && currentNode->IsElement() ?
          currentNode->AsElement() : nullptr;
        if (selectedElement) {
          // If we already found a node, then we have another element,
          //  thus there's not just one element selected
          if (bNodeFound) {
            bNodeFound = false;
            break;
          }

          domTagName = currentNode->NodeName();
          ToLowerCase(domTagName);

          if (anyTag) {
            // Get name of first selected element
            selectedElement->GetTagName(TagName);
            ToLowerCase(TagName);
            anyTag = false;
          }

          // The "A" tag is a pain,
          //  used for both link(href is set) and "Named Anchor"
          if ((isLinkTag &&
               HTMLEditUtils::IsLink(selectedElement)) ||
              (isNamedAnchorTag &&
               HTMLEditUtils::IsNamedAnchor(selectedElement))) {
            bNodeFound = true;
          } else if (TagName == domTagName) { // All other tag names are handled here
            bNodeFound = true;
          }
          if (!bNodeFound) {
            // Check if node we have is really part of the selection???
            break;
          }
        }
        iter->Next();
      }
    } else {
      // Should never get here?
      isCollapsed = true;
      NS_WARNING("isCollapsed was FALSE, but no elements found in selection\n");
    }
  }

  selectedElement.forget(aReturn);
  return NS_OK;
}

already_AddRefed<Element>
HTMLEditor::GetSelectedElement(const nsAString& aTagName)
{
  nsCOMPtr<nsISupports> domElement;
  GetSelectedElement(aTagName, getter_AddRefs(domElement));
  nsCOMPtr<Element> element = do_QueryInterface(domElement);
  return element.forget();
}

already_AddRefed<Element>
HTMLEditor::CreateElementWithDefaults(const nsAString& aTagName)
{
  MOZ_ASSERT(!aTagName.IsEmpty());

  nsAutoString tagName(aTagName);
  ToLowerCase(tagName);
  nsAutoString realTagName;

  if (IsLinkTag(tagName) || IsNamedAnchorTag(tagName)) {
    realTagName.Assign('a');
  } else {
    realTagName = tagName;
  }
  // We don't use editor's CreateElement because we don't want to go through
  // the transaction system

  // New call to use instead to get proper HTML element, bug 39919
  RefPtr<nsAtom> realTagAtom = NS_Atomize(realTagName);
  RefPtr<Element> newElement = CreateHTMLContent(realTagAtom);
  if (!newElement) {
    return nullptr;
  }

  // Mark the new element dirty, so it will be formatted
  ErrorResult rv;
  newElement->SetAttribute(NS_LITERAL_STRING("_moz_dirty"), EmptyString(), rv);

  // Set default values for new elements
  if (tagName.EqualsLiteral("table")) {
    newElement->SetAttribute(NS_LITERAL_STRING("cellpadding"),
                             NS_LITERAL_STRING("2"), rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return nullptr;
    }
    newElement->SetAttribute(NS_LITERAL_STRING("cellspacing"),
                             NS_LITERAL_STRING("2"), rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return nullptr;
    }
    newElement->SetAttribute(NS_LITERAL_STRING("border"),
                             NS_LITERAL_STRING("1"), rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return nullptr;
    }
  } else if (tagName.EqualsLiteral("td")) {
    nsresult rv =
      SetAttributeOrEquivalent(
        newElement, nsGkAtoms::valign, NS_LITERAL_STRING("top"), true);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }
  // ADD OTHER TAGS HERE

  return newElement.forget();
}

NS_IMETHODIMP
HTMLEditor::CreateElementWithDefaults(const nsAString& aTagName,
                                      Element** aReturn)
{
  NS_ENSURE_TRUE(!aTagName.IsEmpty() && aReturn, NS_ERROR_NULL_POINTER);
  *aReturn = nullptr;

  nsCOMPtr<Element> newElement = CreateElementWithDefaults(aTagName);
  NS_ENSURE_TRUE(newElement, NS_ERROR_FAILURE);

  newElement.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::InsertLinkAroundSelection(Element* aAnchorElement)
{
  NS_ENSURE_TRUE(aAnchorElement, NS_ERROR_NULL_POINTER);

  // We must have a real selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  if (selection->IsCollapsed()) {
    NS_WARNING("InsertLinkAroundSelection called but there is no selection!!!");
    return NS_OK;
  }


  // Be sure we were given an anchor element
  RefPtr<HTMLAnchorElement> anchor =
    HTMLAnchorElement::FromNodeOrNull(aAnchorElement);
  if (!anchor) {
    return NS_OK;
  }

  nsAutoString href;
  anchor->GetHref(href);
  if (href.IsEmpty()) {
    return NS_OK;
  }

  nsresult rv;
  AutoPlaceholderBatch beginBatching(this);

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

      rv = SetInlineProperty(nsGkAtoms::a, name, value);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

nsresult
HTMLEditor::SetHTMLBackgroundColorWithTransaction(const nsAString& aColor)
{
  MOZ_ASSERT(IsInitialized(), "The HTMLEditor hasn't been initialized yet");

  // Find a selected or enclosing table element to set background on
  RefPtr<Element> element;
  int32_t selectedCount;
  nsAutoString tagName;
  nsresult rv = GetSelectedOrParentTableElement(tagName, &selectedCount,
                                                getter_AddRefs(element));
  NS_ENSURE_SUCCESS(rv, rv);

  bool setColor = !aColor.IsEmpty();

  RefPtr<nsAtom> bgColorAtom = NS_Atomize("bgcolor");
  if (element) {
    if (selectedCount > 0) {
      // Traverse all selected cells
      RefPtr<Element> cell;
      rv = GetFirstSelectedCell(nullptr, getter_AddRefs(cell));
      if (NS_SUCCEEDED(rv) && cell) {
        while (cell) {
          rv = setColor ?
                 SetAttributeWithTransaction(*cell, *bgColorAtom, aColor) :
                 RemoveAttributeWithTransaction(*cell, *bgColorAtom);
          if (NS_FAILED(rv)) {
            return rv;
          }
          GetNextSelectedCell(nullptr, getter_AddRefs(cell));
        }
        return NS_OK;
      }
    }
    // If we failed to find a cell, fall through to use originally-found element
  } else {
    // No table element -- set the background color on the body tag
    element = GetRoot();
    if (NS_WARN_IF(!element)) {
      return NS_ERROR_FAILURE;
    }
  }
  // Use the editor method that goes through the transaction system
  return setColor ?
           SetAttributeWithTransaction(*element, *bgColorAtom, aColor) :
           RemoveAttributeWithTransaction(*element, *bgColorAtom);
}

NS_IMETHODIMP
HTMLEditor::SetBodyAttribute(const nsAString& aAttribute,
                             const nsAString& aValue)
{
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level

  MOZ_ASSERT(IsInitialized(), "The HTMLEditor hasn't been initialized yet");

  // Set the background color attribute on the body tag
  RefPtr<Element> rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  // Use the editor method that goes through the transaction system
  RefPtr<nsAtom> attributeAtom = NS_Atomize(aAttribute);
  return SetAttributeWithTransaction(*rootElement, *attributeAtom, aValue);
}

NS_IMETHODIMP
HTMLEditor::GetLinkedObjects(nsIArray** aNodeList)
{
  NS_ENSURE_TRUE(aNodeList, NS_ERROR_NULL_POINTER);

  nsresult rv;
  nsCOMPtr<nsIMutableArray> nodes = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIContentIterator> iter =
    do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &rv);
  NS_ENSURE_TRUE(iter, NS_ERROR_NULL_POINTER);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDocument> doc = GetDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    iter->Init(doc->GetRootElement());

    // loop through the content iterator for each content node
    while (!iter->IsDone()) {
      nsCOMPtr<nsINode> node = iter->GetCurrentNode();
      if (node) {
        // Let nsURIRefObject make the hard decisions:
        nsCOMPtr<nsIURIRefObject> refObject;
        rv = NS_NewHTMLURIRefObject(getter_AddRefs(refObject), node);
        if (NS_SUCCEEDED(rv)) {
          nodes->AppendElement(refObject);
        }
      }
      iter->Next();
    }
  }

  nodes.forget(aNodeList);
  return NS_OK;
}


NS_IMETHODIMP
HTMLEditor::AddStyleSheet(const nsAString& aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL)) {
    return NS_OK;
  }

  // Lose the previously-loaded sheet so there's nothing to replace
  // This pattern is different from Override methods because
  //  we must wait to remove mLastStyleSheetURL and add new sheet
  //  at the same time (in StyleSheetLoaded callback) so they are undoable together
  mLastStyleSheetURL.Truncate();
  return ReplaceStyleSheet(aURL);
}

NS_IMETHODIMP
HTMLEditor::ReplaceStyleSheet(const nsAString& aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL)) {
    // Disable last sheet if not the same as new one
    if (!mLastStyleSheetURL.IsEmpty() && !mLastStyleSheetURL.Equals(aURL)) {
      return EnableStyleSheet(mLastStyleSheetURL, false);
    }
    return NS_OK;
  }

  // Make sure the pres shell doesn't disappear during the load.
  if (NS_WARN_IF(!IsInitialized())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIURI> uaURI;
  nsresult rv = NS_NewURI(getter_AddRefs(uaURI), aURL);
  NS_ENSURE_SUCCESS(rv, rv);

  return ps->GetDocument()->CSSLoader()->LoadSheet(
    uaURI, false, nullptr, nullptr, this);
}

NS_IMETHODIMP
HTMLEditor::RemoveStyleSheet(const nsAString& aURL)
{
  return RemoveStyleSheetWithTransaction(aURL);
}

nsresult
HTMLEditor::RemoveStyleSheetWithTransaction(const nsAString& aURL)
{
  RefPtr<StyleSheet> sheet = GetStyleSheetForURL(aURL);
  if (NS_WARN_IF(!sheet)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<RemoveStyleSheetTransaction> transaction =
    RemoveStyleSheetTransaction::Create(*this, *sheet);
  nsresult rv = DoTransaction(transaction);
  if (NS_SUCCEEDED(rv)) {
    mLastStyleSheetURL.Truncate();        // forget it
  }
  // Remove it from our internal list
  return RemoveStyleSheetFromList(aURL);
}


NS_IMETHODIMP
HTMLEditor::AddOverrideStyleSheet(const nsAString& aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL)) {
    return NS_OK;
  }

  // Make sure the pres shell doesn't disappear during the load.
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIURI> uaURI;
  nsresult rv = NS_NewURI(getter_AddRefs(uaURI), aURL);
  NS_ENSURE_SUCCESS(rv, rv);

  // We MUST ONLY load synchronous local files (no @import)
  // XXXbz Except this will actually try to load remote files
  // synchronously, of course..
  RefPtr<StyleSheet> sheet;
  // Editor override style sheets may want to style Gecko anonymous boxes
  rv = ps->GetDocument()->CSSLoader()->
    LoadSheetSync(uaURI, mozilla::css::eAgentSheetFeatures, true,
                  &sheet);

  // Synchronous loads should ALWAYS return completed
  NS_ENSURE_TRUE(sheet, NS_ERROR_NULL_POINTER);

  // Add the override style sheet
  // (This checks if already exists)
  ps->AddOverrideStyleSheet(sheet);
  ps->ApplicableStylesChanged();

  // Save as the last-loaded sheet
  mLastOverrideStyleSheetURL = aURL;

  //Add URL and style sheet to our lists
  return AddNewStyleSheetToList(aURL, sheet);
}

NS_IMETHODIMP
HTMLEditor::ReplaceOverrideStyleSheet(const nsAString& aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL)) {
    // Disable last sheet if not the same as new one
    if (!mLastOverrideStyleSheetURL.IsEmpty() &&
        !mLastOverrideStyleSheetURL.Equals(aURL)) {
      return EnableStyleSheet(mLastOverrideStyleSheetURL, false);
    }
    return NS_OK;
  }
  // Remove the previous sheet
  if (!mLastOverrideStyleSheetURL.IsEmpty()) {
    RemoveOverrideStyleSheet(mLastOverrideStyleSheetURL);
  }
  return AddOverrideStyleSheet(aURL);
}

// Do NOT use transaction system for override style sheets
NS_IMETHODIMP
HTMLEditor::RemoveOverrideStyleSheet(const nsAString& aURL)
{
  RefPtr<StyleSheet> sheet = GetStyleSheetForURL(aURL);

  // Make sure we remove the stylesheet from our internal list in all
  // cases.
  nsresult rv = RemoveStyleSheetFromList(aURL);

  NS_ENSURE_TRUE(sheet, NS_OK); /// Don't fail if sheet not found

  if (NS_WARN_IF(!IsInitialized())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  ps->RemoveOverrideStyleSheet(sheet);
  ps->ApplicableStylesChanged();

  // Remove it from our internal list
  return rv;
}

NS_IMETHODIMP
HTMLEditor::EnableStyleSheet(const nsAString& aURL,
                             bool aEnable)
{
  RefPtr<StyleSheet> sheet = GetStyleSheetForURL(aURL);
  NS_ENSURE_TRUE(sheet, NS_OK); // Don't fail if sheet not found

  // Ensure the style sheet is owned by our document.
  nsCOMPtr<nsIDocument> document = GetDocument();
  sheet->SetAssociatedDocumentOrShadowRoot(
    document, StyleSheet::NotOwnedByDocumentOrShadowRoot);

  sheet->SetDisabled(!aEnable);
  return NS_OK;
}

bool
HTMLEditor::EnableExistingStyleSheet(const nsAString& aURL)
{
  RefPtr<StyleSheet> sheet = GetStyleSheetForURL(aURL);

  // Enable sheet if already loaded.
  if (!sheet) {
    return false;
  }

  // Ensure the style sheet is owned by our document.
  nsCOMPtr<nsIDocument> document = GetDocument();
  sheet->SetAssociatedDocumentOrShadowRoot(
    document, StyleSheet::NotOwnedByDocumentOrShadowRoot);

  // FIXME: This used to do sheet->SetDisabled(false), figure out if we can
  // just remove all this code in bug 1449522, since it seems unused.
  return true;
}

nsresult
HTMLEditor::AddNewStyleSheetToList(const nsAString& aURL,
                                   StyleSheet* aStyleSheet)
{
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

nsresult
HTMLEditor::RemoveStyleSheetFromList(const nsAString& aURL)
{
  // is it already in the list?
  size_t foundIndex;
  foundIndex = mStyleSheetURLs.IndexOf(aURL);
  if (foundIndex == mStyleSheetURLs.NoIndex) {
    return NS_ERROR_FAILURE;
  }

  // Attempt both removals; if one fails there's not much we can do.
  mStyleSheets.RemoveElementAt(foundIndex);
  mStyleSheetURLs.RemoveElementAt(foundIndex);

  return NS_OK;
}

StyleSheet*
HTMLEditor::GetStyleSheetForURL(const nsAString& aURL)
{
  // is it already in the list?
  size_t foundIndex;
  foundIndex = mStyleSheetURLs.IndexOf(aURL);
  if (foundIndex == mStyleSheetURLs.NoIndex) {
    return nullptr;
  }

  MOZ_ASSERT(mStyleSheets[foundIndex]);
  return mStyleSheets[foundIndex];
}

void
HTMLEditor::GetURLForStyleSheet(StyleSheet* aStyleSheet,
                                nsAString& aURL)
{
  // is it already in the list?
  int32_t foundIndex = mStyleSheets.IndexOf(aStyleSheet);

  // Don't fail if we don't find it in our list
  if (foundIndex == -1) {
    return;
  }

  // Found it in the list!
  aURL = mStyleSheetURLs[foundIndex];
}

NS_IMETHODIMP
HTMLEditor::GetEmbeddedObjects(nsIArray** aNodeList)
{
  NS_ENSURE_TRUE(aNodeList, NS_ERROR_NULL_POINTER);

  nsresult rv;
  nsCOMPtr<nsIMutableArray> nodes = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContentIterator> iter =
      do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &rv);
  NS_ENSURE_TRUE(iter, NS_ERROR_NULL_POINTER);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  iter->Init(doc->GetRootElement());

  // Loop through the content iterator for each content node.
  while (!iter->IsDone()) {
    nsINode* node = iter->GetCurrentNode();
    if (node->IsElement()) {
      dom::Element* element = node->AsElement();

      // See if it's an image or an embed and also include all links.
      // Let mail decide which link to send or not
      if (element->IsAnyOfHTMLElements(nsGkAtoms::img, nsGkAtoms::embed,
                                       nsGkAtoms::a) ||
          (element->IsHTMLElement(nsGkAtoms::body) &&
           element->HasAttr(kNameSpaceID_None, nsGkAtoms::background))) {
        nodes->AppendElement(node);
       }
     }
     iter->Next();
   }

  nodes.forget(aNodeList);
  return rv;
}

nsresult
HTMLEditor::DeleteSelectionWithTransaction(EDirection aAction,
                                           EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  nsresult rv =
    TextEditor::DeleteSelectionWithTransaction(aAction, aStripWrappers);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we weren't asked to strip any wrappers, we're done.
  if (aStripWrappers == eNoStrip) {
    return NS_OK;
  }

  RefPtr<Selection> selection = GetSelection();
  // Just checking that the selection itself is collapsed doesn't seem to work
  // right in the multi-range case
  NS_ENSURE_STATE(selection);
  NS_ENSURE_STATE(selection->GetAnchorFocusRange());
  NS_ENSURE_STATE(selection->GetAnchorFocusRange()->Collapsed());

  NS_ENSURE_STATE(selection->GetAnchorNode()->IsContent());
  nsCOMPtr<nsIContent> content = selection->GetAnchorNode()->AsContent();

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
  NS_ENSURE_SUCCESS(rv, rv);
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
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
HTMLEditor::DeleteNodeWithTransaction(nsINode& aNode)
{
  if (NS_WARN_IF(!aNode.IsContent())) {
    return NS_ERROR_INVALID_ARG;
  }
  // Do nothing if the node is read-only.
  // XXX This is not a override method of EditorBase's method.  This might
  //     cause not called accidentally.  We need to investigate this issue.
  if (NS_WARN_IF(!IsModifiableNode(aNode.AsContent()) &&
                 !IsMozEditorBogusNode(aNode.AsContent()))) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = EditorBase::DeleteNodeWithTransaction(aNode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::DeleteNode(nsINode* aNode)
{
  if (NS_WARN_IF(!aNode)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv = DeleteNodeWithTransaction(*aNode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
HTMLEditor::DeleteTextWithTransaction(CharacterData& aCharData,
                                      uint32_t aOffset,
                                      uint32_t aLength)
{
  // Do nothing if the node is read-only
  if (!IsModifiableNode(&aCharData)) {
    return NS_ERROR_FAILURE;
  }

  return EditorBase::DeleteTextWithTransaction(aCharData, aOffset, aLength);
}

nsresult
HTMLEditor::InsertTextWithTransaction(
              nsIDocument& aDocument,
              const nsAString& aStringToInsert,
              const EditorRawDOMPoint& aPointToInsert,
              EditorRawDOMPoint* aPointAfterInsertedString)
{
  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  // Do nothing if the node is read-only
  if (!IsModifiableNode(aPointToInsert.GetContainer())) {
    return NS_ERROR_FAILURE;
  }

  return EditorBase::InsertTextWithTransaction(aDocument, aStringToInsert,
                                               aPointToInsert,
                                               aPointAfterInsertedString);
}

void
HTMLEditor::ContentAppended(nsIContent* aFirstNewContent)
{
  DoContentInserted(aFirstNewContent, eAppended);
}

void
HTMLEditor::ContentInserted(nsIContent* aChild)
{
  DoContentInserted(aChild, eInserted);
}

bool
HTMLEditor::IsInObservedSubtree(nsIContent* aChild)
{
  if (!aChild) {
    return false;
  }

  Element* root = GetRoot();
  // To be super safe here, check both ChromeOnlyAccess and GetBindingParent.
  // That catches (also unbound) native anonymous content, XBL and ShadowDOM.
  if (root &&
      (root->ChromeOnlyAccess() != aChild->ChromeOnlyAccess() ||
       root->GetBindingParent() != aChild->GetBindingParent())) {
    return false;
  }

  return !aChild->ChromeOnlyAccess() && !aChild->GetBindingParent();
}

void
HTMLEditor::DoContentInserted(nsIContent* aChild,
                              InsertedOrAppended aInsertedOrAppended)
{
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

  if (ShouldReplaceRootElement()) {
    UpdateRootElement();
    nsContentUtils::AddScriptRunner(
      NewRunnableMethod("HTMLEditor::NotifyRootChanged",
                        this,
                        &HTMLEditor::NotifyRootChanged));
  }
  // We don't need to handle our own modifications
  else if (!mAction && container->IsEditable()) {
    if (IsMozEditorBogusNode(aChild)) {
      // Ignore insertion of the bogus node
      return;
    }
    // Protect the edit rules object from dying
    RefPtr<TextEditRules> rules(mRules);
    rules->DocumentModified();

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

void
HTMLEditor::ContentRemoved(nsIContent* aChild,
                           nsIContent* aPreviousSibling)
{
  if (!IsInObservedSubtree(aChild)) {
    return;
  }

  // XXX Why do we need to do this?  This method is a mutation observer's
  //     method.  Therefore, the caller should guarantee that this won't be
  //     deleted during the call.
  RefPtr<HTMLEditor> kungFuDeathGrip(this);

  if (SameCOMIdentity(aChild, mRootElement)) {
    mRootElement = nullptr;
    nsContentUtils::AddScriptRunner(
      NewRunnableMethod("HTMLEditor::NotifyRootChanged",
                        this,
                        &HTMLEditor::NotifyRootChanged));
  // We don't need to handle our own modifications
  } else if (!mAction && aChild->GetParentNode()->IsEditable()) {
    if (aChild && IsMozEditorBogusNode(aChild)) {
      // Ignore removal of the bogus node
      return;
    }
    // Protect the edit rules object from dying
    RefPtr<TextEditRules> rules(mRules);
    rules->DocumentModified();
  }
}

bool
HTMLEditor::IsModifiableNode(nsINode* aNode)
{
  return !aNode || aNode->IsEditable();
}

NS_IMETHODIMP
HTMLEditor::DebugUnitTests(int32_t* outNumTests,
                           int32_t* outNumTestsFailed)
{
#ifdef DEBUG
  NS_ENSURE_TRUE(outNumTests && outNumTestsFailed, NS_ERROR_NULL_POINTER);

  TextEditorTest *tester = new TextEditorTest();
  NS_ENSURE_TRUE(tester, NS_ERROR_OUT_OF_MEMORY);

  tester->Run(this, outNumTests, outNumTestsFailed);
  delete tester;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
HTMLEditor::StyleSheetLoaded(StyleSheet* aSheet,
                             bool aWasDeferred,
                             nsresult aStatus)
{
  AutoPlaceholderBatch batchIt(this);

  if (!mLastStyleSheetURL.IsEmpty()) {
    RemoveStyleSheetWithTransaction(mLastStyleSheetURL);
  }

  RefPtr<AddStyleSheetTransaction> transaction =
    AddStyleSheetTransaction::Create(*this, *aSheet);
  nsresult rv = DoTransaction(transaction);
  if (NS_SUCCEEDED(rv)) {
    // Get the URI, then url spec from the sheet
    nsAutoCString spec;
    rv = aSheet->GetSheetURI()->GetSpec(spec);

    if (NS_SUCCEEDED(rv)) {
      // Save it so we can remove before applying the next one
      CopyASCIItoUTF16(spec, mLastStyleSheetURL);

      // Also save in our arrays of urls and sheets
      AddNewStyleSheetToList(mLastStyleSheetURL, aSheet);
    }
  }

  return NS_OK;
}

/**
 * All editor operations which alter the doc should be prefaced
 * with a call to StartOperation, naming the action and direction.
 */
nsresult
HTMLEditor::StartOperation(EditAction opID,
                           nsIEditor::EDirection aDirection)
{
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  EditorBase::StartOperation(opID, aDirection);  // will set mAction, mDirection
  if (rules) {
    return rules->BeforeEdit(mAction, mDirection);
  }
  return NS_OK;
}

/**
 * All editor operations which alter the doc should be followed
 * with a call to EndOperation.
 */
nsresult
HTMLEditor::EndOperation()
{
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  // post processing
  nsresult rv = rules ? rules->AfterEdit(mAction, mDirection) : NS_OK;
  EditorBase::EndOperation();  // will clear mAction, mDirection
  return rv;
}

bool
HTMLEditor::TagCanContainTag(nsAtom& aParentTag,
                             nsAtom& aChildTag) const
{
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

bool
HTMLEditor::IsContainer(nsINode* aNode)
{
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

nsresult
HTMLEditor::SelectEntireDocument(Selection* aSelection)
{
  if (!aSelection || !mRules) {
    return NS_ERROR_NULL_POINTER;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  // is doc empty?
  if (rules->DocumentIsEmpty()) {
    // get editor root node
    Element* rootElement = GetRoot();

    // if its empty dont select entire doc - that would select the bogus node
    return aSelection->Collapse(rootElement, 0);
  }

  return EditorBase::SelectEntireDocument(aSelection);
}

NS_IMETHODIMP
HTMLEditor::SelectAll()
{
  CommitComposition();

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  nsINode* anchorNode = selection->GetAnchorNode();
  if (NS_WARN_IF(!anchorNode) || NS_WARN_IF(!anchorNode->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  nsIContent* anchorContent = anchorNode->AsContent();
  nsIContent* rootContent;
  if (anchorContent->HasIndependentSelection()) {
    selection->SetAncestorLimiter(nullptr);
    rootContent = mRootElement;
  } else {
    nsCOMPtr<nsIPresShell> ps = GetPresShell();
    rootContent = anchorContent->GetSelectionRootContent(ps);
  }

  NS_ENSURE_TRUE(rootContent, NS_ERROR_UNEXPECTED);

  Maybe<mozilla::dom::Selection::AutoUserInitiated> userSelection;
  if (!rootContent->IsEditable()) {
    userSelection.emplace(selection);
  }
  ErrorResult errorResult;
  selection->SelectAllChildren(*rootContent, errorResult);
  return errorResult.StealNSResult();
}

// this will NOT find aAttribute unless aAttribute has a non-null value
// so singleton attributes like <Table border> will not be matched!
bool
HTMLEditor::IsTextPropertySetByContent(nsINode* aNode,
                                       nsAtom* aProperty,
                                       nsAtom* aAttribute,
                                       const nsAString* aValue,
                                       nsAString* outValue)
{
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

bool
HTMLEditor::SetCaretInTableCell(Element* aElement)
{
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
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, false);

  return NS_SUCCEEDED(selection->Collapse(node, 0));
}

/**
 * GetEnclosingTable() finds ancestor who is a table, if any.
 */
Element*
HTMLEditor::GetEnclosingTable(nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  for (nsCOMPtr<Element> block = GetBlockNodeParent(aNode);
       block;
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
nsresult
HTMLEditor::CollapseAdjacentTextNodes(nsRange* aInRange)
{
  NS_ENSURE_TRUE(aInRange, NS_ERROR_NULL_POINTER);
  AutoTransactionsConserveSelection dontChangeMySelection(this);
  nsTArray<nsCOMPtr<nsINode>> textNodes;
  // we can't actually do anything during iteration, so store the text nodes in an array
  // don't bother ref counting them because we know we can hold them for the
  // lifetime of this method


  // build a list of editable text nodes
  nsresult rv = NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIContentIterator> iter =
    do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  iter->Init(aInRange);

  while (!iter->IsDone()) {
    nsINode* node = iter->GetCurrentNode();
    if (node->NodeType() == nsINode::TEXT_NODE &&
        IsEditable(node->AsContent())) {
      textNodes.AppendElement(node);
    }

    iter->Next();
  }

  // now that I have a list of text nodes, collapse adjacent text nodes
  // NOTE: assumption that JoinNodes keeps the righthand node
  while (textNodes.Length() > 1) {
    // we assume a textNodes entry can't be nullptr
    nsINode* leftTextNode = textNodes[0];
    nsINode* rightTextNode = textNodes[1];
    NS_ASSERTION(leftTextNode && rightTextNode,"left or rightTextNode null in CollapseAdjacentTextNodes");

    // get the prev sibling of the right node, and see if its leftTextNode
    nsCOMPtr<nsINode> prevSibOfRightNode = rightTextNode->GetPreviousSibling();
    if (prevSibOfRightNode && prevSibOfRightNode == leftTextNode) {
      rv = JoinNodesWithTransaction(*leftTextNode, *rightTextNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    textNodes.RemoveElementAt(0); // remove the leftmost text node from the list
  }

  return NS_OK;
}

nsresult
HTMLEditor::SetSelectionAtDocumentStart(Selection* aSelection)
{
  dom::Element* rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER);

  return aSelection->Collapse(rootElement, 0);
}

/**
 * Remove aNode, reparenting any children into the parent of aNode.  In
 * addition, insert any br's needed to preserve identity of removed block.
 */
nsresult
HTMLEditor::RemoveBlockContainerWithTransaction(Element& aElement)
{
  RefPtr<Selection> selection = GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }

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
        InsertBrElementWithTransaction(*selection,
                                       EditorRawDOMPoint(&aElement, 0));
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
        EditorRawDOMPoint endOfNode;
        endOfNode.SetToEndOf(&aElement);
        RefPtr<Element> brElement =
          InsertBrElementWithTransaction(*selection, endOfNode);
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
          InsertBrElementWithTransaction(*selection,
                                         EditorRawDOMPoint(&aElement, 0));
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
nsIContent*
HTMLEditor::GetPriorHTMLSibling(nsINode* aNode)
{
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
nsIContent*
HTMLEditor::GetNextHTMLSibling(nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  nsIContent* node = aNode->GetNextSibling();
  while (node && !IsEditable(node)) {
    node = node->GetNextSibling();
  }

  return node;
}

nsIContent*
HTMLEditor::GetPreviousHTMLElementOrTextInternal(nsINode& aNode,
                                                 bool aNoBlockCrossing)
{
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetPreviousElementOrTextInBlock(aNode) :
                            GetPreviousElementOrText(aNode);
}

template<typename PT, typename CT>
nsIContent*
HTMLEditor::GetPreviousHTMLElementOrTextInternal(
              const EditorDOMPointBase<PT, CT>& aPoint,
              bool aNoBlockCrossing)
{
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetPreviousElementOrTextInBlock(aPoint) :
                            GetPreviousElementOrText(aPoint);
}

nsIContent*
HTMLEditor::GetPreviousEditableHTMLNodeInternal(nsINode& aNode,
                                                bool aNoBlockCrossing)
{
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetPreviousEditableNodeInBlock(aNode) :
                            GetPreviousEditableNode(aNode);
}

template<typename PT, typename CT>
nsIContent*
HTMLEditor::GetPreviousEditableHTMLNodeInternal(
              const EditorDOMPointBase<PT, CT>& aPoint,
              bool aNoBlockCrossing)
{
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetPreviousEditableNodeInBlock(aPoint) :
                            GetPreviousEditableNode(aPoint);
}

nsIContent*
HTMLEditor::GetNextHTMLElementOrTextInternal(nsINode& aNode,
                                             bool aNoBlockCrossing)
{
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetNextElementOrTextInBlock(aNode) :
                            GetNextElementOrText(aNode);
}

template<typename PT, typename CT>
nsIContent*
HTMLEditor::GetNextHTMLElementOrTextInternal(
              const EditorDOMPointBase<PT, CT>& aPoint,
              bool aNoBlockCrossing)
{
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetNextElementOrTextInBlock(aPoint) :
                            GetNextElementOrText(aPoint);
}

nsIContent*
HTMLEditor::GetNextEditableHTMLNodeInternal(nsINode& aNode,
                                            bool aNoBlockCrossing)
{
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetNextEditableNodeInBlock(aNode) :
                            GetNextEditableNode(aNode);
}

template<typename PT, typename CT>
nsIContent*
HTMLEditor::GetNextEditableHTMLNodeInternal(
              const EditorDOMPointBase<PT, CT>& aPoint,
              bool aNoBlockCrossing)
{
  if (!GetActiveEditingHost()) {
    return nullptr;
  }
  return aNoBlockCrossing ? GetNextEditableNodeInBlock(aPoint) :
                            GetNextEditableNode(aPoint);
}

bool
HTMLEditor::IsFirstEditableChild(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  // find first editable child and compare it to aNode
  nsCOMPtr<nsINode> parent = aNode->GetParentNode();
  if (NS_WARN_IF(!parent)) {
    return false;
  }
  return (GetFirstEditableChild(*parent) == aNode);
}

bool
HTMLEditor::IsLastEditableChild(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  // find last editable child and compare it to aNode
  nsCOMPtr<nsINode> parent = aNode->GetParentNode();
  if (NS_WARN_IF(!parent)) {
    return false;
  }
  return (GetLastEditableChild(*parent) == aNode);
}

nsIContent*
HTMLEditor::GetFirstEditableChild(nsINode& aNode)
{
  nsCOMPtr<nsIContent> child = aNode.GetFirstChild();

  while (child && !IsEditable(child)) {
    child = child->GetNextSibling();
  }

  return child;
}

nsIContent*
HTMLEditor::GetLastEditableChild(nsINode& aNode)
{
  nsCOMPtr<nsIContent> child = aNode.GetLastChild();

  while (child && !IsEditable(child)) {
    child = child->GetPreviousSibling();
  }

  return child;
}

nsIContent*
HTMLEditor::GetFirstEditableLeaf(nsINode& aNode)
{
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

nsIContent*
HTMLEditor::GetLastEditableLeaf(nsINode& aNode)
{
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

bool
HTMLEditor::IsInVisibleTextFrames(Text& aText)
{
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
  nsresult rv =
    selectionController->CheckVisibilityContent(&aText,
                                                0, aText.TextDataLength(),
                                                &isVisible);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return isVisible;
}

bool
HTMLEditor::IsVisibleTextNode(Text& aText)
{
  if (!aText.TextDataLength()) {
    return false;
  }

  if (!aText.TextIsOnlyWhitespace()) {
    return true;
  }

  WSRunObject wsRunObj(this, &aText, 0);
  nsCOMPtr<nsINode> nextVisibleNode;
  int32_t unused = 0;
  WSType visibleNodeType;
  wsRunObj.NextVisibleNode(EditorRawDOMPoint(&aText, 0),
                           address_of(nextVisibleNode),
                           &unused, &visibleNodeType);
  return (visibleNodeType == WSType::normalWS ||
          visibleNodeType == WSType::text) &&
         &aText == nextVisibleNode;
}

/**
 * IsEmptyNode() figures out if aNode is an empty node.  A block can have
 * children and still be considered empty, if the children are empty or
 * non-editable.
 */
nsresult
HTMLEditor::IsEmptyNode(nsINode* aNode,
                        bool* outIsEmptyNode,
                        bool aSingleBRDoesntCount,
                        bool aListOrCellNotEmpty,
                        bool aSafeToAskFrames)
{
  NS_ENSURE_TRUE(aNode && outIsEmptyNode, NS_ERROR_NULL_POINTER);
  *outIsEmptyNode = true;
  bool seenBR = false;
  return IsEmptyNodeImpl(aNode, outIsEmptyNode, aSingleBRDoesntCount,
                         aListOrCellNotEmpty, aSafeToAskFrames, &seenBR);
}

/**
 * IsEmptyNodeImpl() is workhorse for IsEmptyNode().
 */
nsresult
HTMLEditor::IsEmptyNodeImpl(nsINode* aNode,
                            bool* outIsEmptyNode,
                            bool aSingleBRDoesntCount,
                            bool aListOrCellNotEmpty,
                            bool aSafeToAskFrames,
                            bool* aSeenBR)
{
  NS_ENSURE_TRUE(aNode && outIsEmptyNode && aSeenBR, NS_ERROR_NULL_POINTER);

  if (Text* text = aNode->GetAsText()) {
    *outIsEmptyNode = aSafeToAskFrames ? !IsInVisibleTextFrames(*text) :
                                         !IsVisibleTextNode(*text);
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
       (aListOrCellNotEmpty &&
        (HTMLEditUtils::IsListItem(aNode) ||
         HTMLEditUtils::IsTableCell(aNode))))) {
    *outIsEmptyNode = false;
    return NS_OK;
  }

  // need this for later
  bool isListItemOrCell = HTMLEditUtils::IsListItem(aNode) ||
                          HTMLEditUtils::IsTableCell(aNode);

  // loop over children of node. if no children, or all children are either
  // empty text nodes or non-editable, then node qualifies as empty
  for (nsCOMPtr<nsIContent> child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    // Is the child editable and non-empty?  if so, return false
    if (EditorBase::IsEditable(child)) {
      if (Text* text = child->GetAsText()) {
        // break out if we find we aren't empty
        *outIsEmptyNode = aSafeToAskFrames ? !IsInVisibleTextFrames(*text) :
                                             !IsVisibleTextNode(*text);
        if (!*outIsEmptyNode){
          return NS_OK;
        }
      } else {
        // An editable, non-text node. We need to check its content.
        // Is it the node we are iterating over?
        if (child == aNode) {
          break;
        }

        if (aSingleBRDoesntCount && !*aSeenBR && child->IsHTMLElement(nsGkAtoms::br)) {
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
          nsresult rv = IsEmptyNodeImpl(child, &isEmptyNode,
                                        aSingleBRDoesntCount,
                                        aListOrCellNotEmpty, aSafeToAskFrames,
                                        aSeenBR);
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
nsresult
HTMLEditor::SetAttributeOrEquivalent(Element* aElement,
                                     nsAtom* aAttribute,
                                     const nsAString& aValue,
                                     bool aSuppressTransaction)
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aAttribute);

  nsAutoScriptBlocker scriptBlocker;

  if (!IsCSSEnabled() || !mCSSEditUtils) {
    // we are not in an HTML+CSS editor; let's set the attribute the HTML way
    if (mCSSEditUtils) {
      mCSSEditUtils->RemoveCSSEquivalentToHTMLStyle(aElement, nullptr,
                                                    aAttribute, nullptr,
                                                    aSuppressTransaction);
    }
    return aSuppressTransaction ?
             aElement->SetAttr(kNameSpaceID_None, aAttribute, aValue, true) :
             SetAttributeWithTransaction(*aElement, *aAttribute, aValue);
  }

  int32_t count =
    mCSSEditUtils->SetCSSEquivalentToHTMLStyle(aElement, nullptr,
                                               aAttribute, &aValue,
                                               aSuppressTransaction);
  if (count) {
    // we found an equivalence ; let's remove the HTML attribute itself if it
    // is set
    nsAutoString existingValue;
    if (!aElement->GetAttr(kNameSpaceID_None, aAttribute, existingValue)) {
      return NS_OK;
    }

    return aSuppressTransaction ?
             aElement->UnsetAttr(kNameSpaceID_None, aAttribute, true) :
             RemoveAttributeWithTransaction(*aElement, *aAttribute);
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
    return aSuppressTransaction ?
       aElement->SetAttr(kNameSpaceID_None, aAttribute, existingValue, true) :
      SetAttributeWithTransaction(*aElement, *aAttribute, existingValue);
  }

  // we have no CSS equivalence for this attribute and it is not the style
  // attribute; let's set it the good'n'old HTML way
  return aSuppressTransaction ?
           aElement->SetAttr(kNameSpaceID_None, aAttribute, aValue, true) :
           SetAttributeWithTransaction(*aElement, *aAttribute, aValue);
}

nsresult
HTMLEditor::RemoveAttributeOrEquivalent(Element* aElement,
                                        nsAtom* aAttribute,
                                        bool aSuppressTransaction)
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aAttribute);

  if (IsCSSEnabled() && mCSSEditUtils) {
    nsresult rv =
      mCSSEditUtils->RemoveCSSEquivalentToHTMLStyle(
        aElement, nullptr, aAttribute, nullptr, aSuppressTransaction);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!aElement->HasAttr(kNameSpaceID_None, aAttribute)) {
    return NS_OK;
  }

  return aSuppressTransaction ?
    aElement->UnsetAttr(kNameSpaceID_None, aAttribute, /* aNotify = */ true) :
    RemoveAttributeWithTransaction(*aElement, *aAttribute);
}

nsresult
HTMLEditor::SetIsCSSEnabled(bool aIsCSSPrefChecked)
{
  if (!mCSSEditUtils) {
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
nsresult
HTMLEditor::SetCSSBackgroundColorWithTransaction(const nsAString& aColor)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);
  CommitComposition();

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  bool isCollapsed = selection->IsCollapsed();

  AutoPlaceholderBatch batchIt(this);
  AutoRules beginRulesSniffing(this, EditAction::insertElement,
                               nsIEditor::eNext);
  AutoSelectionRestorer selectionRestorer(selection, this);
  AutoTransactionsConserveSelection dontChangeMySelection(this);

  bool cancel, handled;
  RulesInfo ruleInfo(EditAction::setTextProperty);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!cancel && !handled) {
    // Loop through the ranges in the selection
    for (uint32_t i = 0; i < selection->RangeCount(); i++) {
      RefPtr<nsRange> range = selection->GetRangeAt(i);
      NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

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
          mCSSEditUtils->SetCSSEquivalentToHTMLStyle(blockParent, nullptr,
                                                     nsGkAtoms::bgcolor,
                                                     &aColor, false);
        }
      } else if (startNode == endNode &&
                 startNode->IsHTMLElement(nsGkAtoms::body) && isCollapsed) {
        // No block in the document, let's apply the background to the body
        mCSSEditUtils->SetCSSEquivalentToHTMLStyle(startNode->AsElement(),
                                                   nullptr, nsGkAtoms::bgcolor,
                                                   &aColor,
                                                   false);
      } else if (startNode == endNode && (endOffset - startOffset == 1 ||
                                          (!startOffset && !endOffset))) {
        // A unique node is selected, let's also apply the background color to
        // the containing block, possibly the node itself
        nsCOMPtr<nsIContent> selectedNode = range->GetChildAtStartOffset();
        nsCOMPtr<Element> blockParent = GetBlock(*selectedNode);
        if (blockParent && cachedBlockParent != blockParent) {
          cachedBlockParent = blockParent;
          mCSSEditUtils->SetCSSEquivalentToHTMLStyle(blockParent, nullptr,
                                                     nsGkAtoms::bgcolor,
                                                     &aColor, false);
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

        OwningNonNull<nsIContentIterator> iter =
          NS_NewContentSubtreeIterator();

        nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
        nsCOMPtr<nsINode> node;

        // Iterate range and build up array
        rv = iter->Init(range);
        // Init returns an error if no nodes in range.  This can easily happen
        // with the subtree iterator if the selection doesn't contain any
        // *whole* nodes.
        if (NS_SUCCEEDED(rv)) {
          for (; !iter->IsDone(); iter->Next()) {
            node = do_QueryInterface(iter->GetCurrentNode());
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
            mCSSEditUtils->SetCSSEquivalentToHTMLStyle(blockParent, nullptr,
                                                       nsGkAtoms::bgcolor,
                                                       &aColor,
                                                       false);
          }
        }

        // Then loop through the list, set the property on each node
        for (auto& node : arrayOfNodes) {
          nsCOMPtr<Element> blockParent = GetBlock(node);
          if (blockParent && cachedBlockParent != blockParent) {
            cachedBlockParent = blockParent;
            mCSSEditUtils->SetCSSEquivalentToHTMLStyle(blockParent, nullptr,
                                                       nsGkAtoms::bgcolor,
                                                       &aColor,
                                                       false);
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
            mCSSEditUtils->SetCSSEquivalentToHTMLStyle(blockParent, nullptr,
                                                       nsGkAtoms::bgcolor,
                                                       &aColor,
                                                       false);
          }
        }
      }
    }
  }
  if (!cancel) {
    // Post-process
    rv = rules->DidDoAction(selection, &ruleInfo, rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetBackgroundColor(const nsAString& aColor)
{
  if (IsCSSEnabled()) {
    // if we are in CSS mode, we have to apply the background color to the
    // containing block (or the body if we have no block-level element in
    // the document)
    return SetCSSBackgroundColorWithTransaction(aColor);
  }

  // but in HTML mode, we can only set the document's background color
  return SetHTMLBackgroundColorWithTransaction(aColor);
}

/**
 * NodesSameType() does these nodes have the same tag?
 */
bool
HTMLEditor::AreNodesSameType(nsIContent* aNode1,
                             nsIContent* aNode2)
{
  MOZ_ASSERT(aNode1);
  MOZ_ASSERT(aNode2);

  if (aNode1->NodeInfo()->NameAtom() != aNode2->NodeInfo()->NameAtom()) {
    return false;
  }

  if (!IsCSSEnabled() || !aNode1->IsHTMLElement(nsGkAtoms::span)) {
    return true;
  }

  if (!aNode1->IsElement() || !aNode2->IsElement()) {
    return false;
  }

  // If CSS is enabled, we are stricter about span nodes.
  return CSSEditUtils::ElementsSameStyle(aNode1->AsElement(),
                                         aNode2->AsElement());
}

nsresult
HTMLEditor::CopyLastEditableChildStylesWithTransaction(
              Element& aPreviousBlock,
              Element& aNewBlock,
              RefPtr<Element>* aNewBrElement)
{
  if (NS_WARN_IF(!aNewBrElement)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aNewBrElement = nullptr;

  RefPtr<Element> previousBlock(&aPreviousBlock);
  RefPtr<Element> newBlock(&aNewBlock);

  // First, clear out aNewBlock.  Contract is that we want only the styles
  // from aPreviousBlock.
  for (nsCOMPtr<nsINode> child = newBlock->GetFirstChild();
       child;
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
  for (nsCOMPtr<nsIContent> child = previousBlock.get();
       child;
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
      deepestEditableContent->IsElement() ?
        deepestEditableContent->AsElement() :
        deepestEditableContent->GetParentElement();
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
      EditorRawDOMPoint atStartOfNewBlock(newBlock, 0);
      firstClonsedElement = lastClonedElement =
        CreateNodeWithTransaction(*tagName, atStartOfNewBlock);
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
    lastClonedElement =
      InsertContainerWithTransaction(*lastClonedElement, *tagName);
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

  RefPtr<Selection> selection = GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<Element> brElement =
    InsertBrElementWithTransaction(*selection,
                                   EditorRawDOMPoint(firstClonsedElement, 0));
  if (NS_WARN_IF(!brElement)) {
    return NS_ERROR_FAILURE;
  }
  *aNewBrElement = brElement.forget();
  return NS_OK;
}

nsresult
HTMLEditor::GetElementOrigin(Element& aElement,
                             int32_t& aX,
                             int32_t& aY)
{
  aX = 0;
  aY = 0;

  if (NS_WARN_IF(!IsInitialized())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  nsIFrame* frame = aElement.GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_OK);

  nsIFrame *container = ps->GetAbsoluteContainingBlock(frame);
  NS_ENSURE_TRUE(container, NS_OK);
  nsPoint off = frame->GetOffsetTo(container);
  aX = nsPresContext::AppUnitsToIntCSSPixels(off.x);
  aY = nsPresContext::AppUnitsToIntCSSPixels(off.y);

  return NS_OK;
}

nsresult
HTMLEditor::EndUpdateViewBatch()
{
  nsresult rv = EditorBase::EndUpdateViewBatch();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mUpdateCount) {
    return NS_OK;
  }

  // We may need to show resizing handles or update existing ones after
  // all transactions are done. This way of doing is preferred to DOM
  // mutation events listeners because all the changes the user can apply
  // to a document may result in multiple events, some of them quite hard
  // to listen too (in particular when an ancestor of the selection is
  // changed but the selection itself is not changed).
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);
  return CheckSelectionStateForAnonymousButtons(selection);
}

NS_IMETHODIMP
HTMLEditor::GetSelectionContainer(Element** aReturn)
{
  RefPtr<Element> container = GetSelectionContainer();
  container.forget(aReturn);
  return NS_OK;
}

Element*
HTMLEditor::GetSelectionContainer()
{
  // If we don't get the selection, just skip this
  NS_ENSURE_TRUE(GetSelection(), nullptr);

  OwningNonNull<Selection> selection = *GetSelection();

  nsCOMPtr<nsINode> focusNode;

  if (selection->IsCollapsed()) {
    focusNode = selection->GetFocusNode();
  } else {
    int32_t rangeCount = selection->RangeCount();

    if (rangeCount == 1) {
      RefPtr<nsRange> range = selection->GetRangeAt(0);

      nsCOMPtr<nsINode> startContainer = range->GetStartContainer();
      int32_t startOffset = range->StartOffset();
      nsCOMPtr<nsINode> endContainer = range->GetEndContainer();
      int32_t endOffset = range->EndOffset();

      if (startContainer == endContainer && startOffset + 1 == endOffset) {
        MOZ_ASSERT(!focusNode, "How did it get set already?");
        focusNode = GetSelectedElement(EmptyString());
      }
      if (!focusNode) {
        focusNode = range->GetCommonAncestor();
      }
    } else {
      for (int32_t i = 0; i < rangeCount; i++) {
        RefPtr<nsRange> range = selection->GetRangeAt(i);

        nsCOMPtr<nsINode> startContainer = range->GetStartContainer();
        if (!focusNode) {
          focusNode = startContainer;
        } else if (focusNode != startContainer) {
          focusNode = startContainer->GetParentNode();
          break;
        }
      }
    }
  }

  if (focusNode && focusNode->GetAsText()) {
    focusNode = focusNode->GetParentNode();
  }

  if (focusNode && focusNode->IsElement()) {
    return focusNode->AsElement();
  }

  return nullptr;
}

NS_IMETHODIMP
HTMLEditor::IsAnonymousElement(Element* aElement,
                               bool* aReturn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);
  *aReturn = aElement->IsRootOfNativeAnonymousSubtree();
  return NS_OK;
}

nsresult
HTMLEditor::SetReturnInParagraphCreatesNewParagraph(bool aCreatesNewParagraph)
{
  mCRInParagraphCreatesParagraph = aCreatesNewParagraph;
  return NS_OK;
}

bool
HTMLEditor::GetReturnInParagraphCreatesNewParagraph()
{
  return mCRInParagraphCreatesParagraph;
}

nsresult
HTMLEditor::GetReturnInParagraphCreatesNewParagraph(bool* aCreatesNewParagraph)
{
  *aCreatesNewParagraph = mCRInParagraphCreatesParagraph;
  return NS_OK;
}

nsIContent*
HTMLEditor::GetFocusedContent()
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nullptr);

  nsCOMPtr<nsIContent> focusedContent = fm->GetFocusedElement();

  nsCOMPtr<nsIDocument> document = GetDocument();
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
      nsContentUtils::ContentIsDescendantOf(focusedContent, document) ?
        focusedContent.get() : nullptr;
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

already_AddRefed<nsIContent>
HTMLEditor::GetFocusedContentForIME()
{
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (!focusedContent) {
    return nullptr;
  }

  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  return document->HasFlag(NODE_IS_EDITABLE) ? nullptr :
                                               focusedContent.forget();
}

bool
HTMLEditor::IsActiveInDOMWindow()
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);

  nsCOMPtr<nsIDocument> document = GetDocument();
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
  nsIContent* content =
    nsFocusManager::GetFocusedDescendant(ourWindow,
                                         nsFocusManager::eOnlyCurrentWindow,
                                         getter_AddRefs(win));
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

Element*
HTMLEditor::GetActiveEditingHost()
{
  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  if (document->HasFlag(NODE_IS_EDITABLE)) {
    return document->GetBodyElement();
  }

  // We're HTML editor for contenteditable
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, nullptr);
  nsINode* focusNode = selection->GetFocusNode();
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

EventTarget*
HTMLEditor::GetDOMEventTarget()
{
  // Don't use getDocument here, because we have no way of knowing
  // whether Init() was ever called.  So we need to get the document
  // ourselves, if it exists.
  MOZ_ASSERT(IsInitialized(), "The HTMLEditor has not been initialized yet");
  nsCOMPtr<mozilla::dom::EventTarget> target = GetDocument();
  return target;
}

bool
HTMLEditor::ShouldReplaceRootElement()
{
  if (!mRootElement) {
    // If we don't know what is our root element, we should find our root.
    return true;
  }

  // If we temporary set document root element to mRootElement, but there is
  // body element now, we should replace the root element by the body element.
  return mRootElement != GetBodyElement();
}

void
HTMLEditor::NotifyRootChanged()
{
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  RemoveEventListeners();
  nsresult rv = InstallEventListeners();
  if (NS_FAILED(rv)) {
    return;
  }

  UpdateRootElement();
  if (!mRootElement) {
    return;
  }

  rv = BeginningOfDocument();
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

Element*
HTMLEditor::GetBodyElement()
{
  MOZ_ASSERT(IsInitialized(), "The HTMLEditor hasn't been initialized yet");
  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  return document->GetBody();
}

already_AddRefed<nsINode>
HTMLEditor::GetFocusedNode()
{
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

  nsCOMPtr<nsIDocument> document = GetDocument();
  return document.forget();
}

bool
HTMLEditor::OurWindowHasFocus()
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);
  nsCOMPtr<mozIDOMWindowProxy> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow) {
    return false;
  }
  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return false;
  }
  nsPIDOMWindowOuter* ourWindow = document->GetWindow();
  return ourWindow == focusedWindow;
}

bool
HTMLEditor::IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent)
{
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

  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return false;
  }

  if (document->HasFlag(NODE_IS_EDITABLE)) {
    // If this editor is in designMode and the event target is the document,
    // the event is for this editor.
    nsCOMPtr<nsIDocument> targetDocument = do_QueryInterface(target);
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
    if (!nsContentUtils::ContentIsDescendantOf(targetContent, editingHost)) {
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

nsresult
HTMLEditor::GetPreferredIMEState(IMEState* aState)
{
  // HTML editor don't prefer the CSS ime-mode because IE didn't do so too.
  aState->mOpen = IMEState::DONT_CHANGE_OPEN_STATE;
  if (IsReadonly() || IsDisabled()) {
    aState->mEnabled = IMEState::DISABLED;
  } else {
    aState->mEnabled = IMEState::ENABLED;
  }
  return NS_OK;
}

already_AddRefed<nsIContent>
HTMLEditor::GetInputEventTargetContent()
{
  nsCOMPtr<nsIContent> target = GetActiveEditingHost();
  return target.forget();
}

Element*
HTMLEditor::GetEditorRoot()
{
  return GetActiveEditingHost();
}

nsHTMLDocument*
HTMLEditor::GetHTMLDocument() const
{
  nsIDocument* doc = GetDocument();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }
  if (!doc->IsHTMLOrXHTML()) {
    return nullptr;
  }
  return doc->AsHTMLDocument();
}

} // namespace mozilla
