/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/EventStates.h"
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

#include "nsIDOMMozNamedAttrMap.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDocumentInlines.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsISelectionController.h"
#include "nsIDOMHTMLDocument.h"
#include "nsILinkHandler.h"
#include "nsIInlineSpellChecker.h"

#include "mozilla/css/Loader.h"
#include "nsIDOMStyleSheet.h"

#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIMutableArray.h"
#include "nsContentUtils.h"
#include "nsIDocumentEncoder.h"
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
#include "nsIParserService.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "nsTextFragment.h"
#include "nsContentList.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

namespace mozilla {

using namespace dom;
using namespace widget;

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

HTMLEditor::HTMLEditor()
  : mCRInParagraphCreatesParagraph(false)
  , mCSSAware(false)
  , mSelectedCellIndex(0)
  , mIsObjectResizingEnabled(true)
  , mIsResizing(false)
  , mPreserveRatio(false)
  , mResizedObjectIsAnImage(false)
  , mIsAbsolutelyPositioningEnabled(true)
  , mResizedObjectIsAbsolutelyPositioned(false)
  , mGrabberClicked(false)
  , mIsMoving(false)
  , mSnapToGridEnabled(false)
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
}

HTMLEditor::~HTMLEditor()
{
  // remove the rules as an action listener.  Else we get a bad
  // ownership loop later on.  it's ok if the rules aren't a listener;
  // we ignore the error.
  nsCOMPtr<nsIEditActionListener> mListener = do_QueryInterface(mRules);
  RemoveEditActionListener(mListener);

  //the autopointers will clear themselves up.
  //but we need to also remove the listeners or we have a leak
  RefPtr<Selection> selection = GetSelection();
  // if we don't get the selection, just skip this
  if (selection) {
    nsCOMPtr<nsISelectionListener>listener;
    listener = do_QueryInterface(mTypeInState);
    if (listener) {
      selection->RemoveSelectionListener(listener);
    }
    listener = do_QueryInterface(mSelectionListenerP);
    if (listener) {
      selection->RemoveSelectionListener(listener);
    }
  }

  mTypeInState = nullptr;
  mSelectionListenerP = nullptr;

  // free any default style propItems
  RemoveAllDefaultProperties();

  if (mLinkHandler && IsInitialized()) {
    nsCOMPtr<nsIPresShell> ps = GetPresShell();

    if (ps && ps->GetPresContext()) {
      ps->GetPresContext()->SetLinkHandler(mLinkHandler);
    }
  }

  RemoveEventListeners();
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStyleSheets)

  tmp->HideAnonymousEditingUIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLEditor, TextEditor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTypeInState)
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelectionListenerP)
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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(HTMLEditor, EditorBase)
NS_IMPL_RELEASE_INHERITED(HTMLEditor, EditorBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLObjectResizer)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLAbsPosEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLInlineTableEditor)
  NS_INTERFACE_MAP_ENTRY(nsITableEditor)
  NS_INTERFACE_MAP_ENTRY(nsIEditorStyleSheets)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END_INHERITING(TextEditor)

NS_IMETHODIMP
HTMLEditor::Init(nsIDOMDocument* aDoc,
                 nsIContent* aRoot,
                 nsISelectionController* aSelCon,
                 uint32_t aFlags,
                 const nsAString& aInitialValue)
{
  NS_PRECONDITION(aDoc && !aSelCon, "bad arg");
  NS_ENSURE_TRUE(aDoc, NS_ERROR_NULL_POINTER);
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
    nsCOMPtr<nsINode> document = do_QueryInterface(aDoc);
    document->AddMutationObserverUnlessExists(this);

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

    // init the selection listener for image resizing
    mSelectionListenerP = new ResizerSelectionListener(*this);

    if (!IsInteractionAllowed()) {
      // ignore any errors from this in case the file is missing
      AddOverrideStyleSheet(NS_LITERAL_STRING("resource://gre/res/EditorOverride.css"));
    }

    RefPtr<Selection> selection = GetSelection();
    if (selection) {
      nsCOMPtr<nsISelectionListener>listener;
      listener = do_QueryInterface(mTypeInState);
      if (listener) {
        selection->AddSelectionListener(listener);
      }
      listener = do_QueryInterface(mSelectionListenerP);
      if (listener) {
        selection->AddSelectionListener(listener);
      }
    }
  }
  NS_ENSURE_SUCCESS(rulesRv, rulesRv);

  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::PreDestroy(bool aDestroyingFrames)
{
  if (mDidPreDestroy) {
    return NS_OK;
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

  return TextEditor::PreDestroy(aDestroyingFrames);
}

NS_IMETHODIMP
HTMLEditor::GetRootElement(nsIDOMElement** aRootElement)
{
  NS_ENSURE_ARG_POINTER(aRootElement);

  if (mRootElement) {
    return EditorBase::GetRootElement(aRootElement);
  }

  *aRootElement = nullptr;

  // Use the HTML documents body element as the editor root if we didn't
  // get a root element during initialization.

  nsCOMPtr<nsIDOMElement> rootElement;
  nsCOMPtr<nsIDOMHTMLElement> bodyElement;
  nsresult rv = GetBodyElement(getter_AddRefs(bodyElement));
  NS_ENSURE_SUCCESS(rv, rv);

  if (bodyElement) {
    rootElement = bodyElement;
  } else {
    // If there is no HTML body element,
    // we should use the document root element instead.
    nsCOMPtr<nsIDOMDocument> domDocument = GetDOMDocument();
    if (NS_WARN_IF(!domDocument)) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    rv = domDocument->GetDocumentElement(getter_AddRefs(rootElement));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Document can have no elements
    if (!rootElement) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  mRootElement = do_QueryInterface(rootElement);
  rootElement.forget(aRootElement);

  return NS_OK;
}

already_AddRefed<nsIContent>
HTMLEditor::FindSelectionRoot(nsINode* aNode)
{
  NS_PRECONDITION(aNode->IsNodeOfType(nsINode::eDOCUMENT) ||
                  aNode->IsNodeOfType(nsINode::eCONTENT),
                  "aNode must be content or document node");

  nsCOMPtr<nsIDocument> doc = aNode->GetUncomposedDoc();
  if (!doc) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> content;
  if (doc->HasFlag(NODE_IS_EDITABLE) || !aNode->IsContent()) {
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

  nsCOMPtr<nsIDOMEventTarget> target = GetDOMEventTarget();

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

NS_IMETHODIMP
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
  // XXX Why doesn't this check if the document is alive?
  if (!IsInitialized()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Get the selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);

  // Get the root element.
  nsCOMPtr<Element> rootElement = GetRoot();
  if (!rootElement) {
    NS_WARNING("GetRoot() returned a null pointer (mRootElement is null)");
    return NS_OK;
  }

  // Find first editable thingy
  bool done = false;
  nsCOMPtr<nsINode> curNode = rootElement.get(), selNode;
  int32_t curOffset = 0, selOffset = 0;
  while (!done) {
    WSRunObject wsObj(this, curNode, curOffset);
    int32_t visOffset = 0;
    WSType visType;
    nsCOMPtr<nsINode> visNode;
    wsObj.NextVisibleNode(curNode, curOffset, address_of(visNode), &visOffset,
                          &visType);
    if (visType == WSType::normalWS || visType == WSType::text) {
      selNode = visNode;
      selOffset = visOffset;
      done = true;
    } else if (visType == WSType::br || visType == WSType::special) {
      selNode = visNode->GetParentNode();
      selOffset = selNode ? selNode->IndexOf(visNode) : -1;
      done = true;
    } else if (visType == WSType::otherBlock) {
      // By definition of WSRunObject, a block element terminates a
      // whitespace run. That is, although we are calling a method that is
      // named "NextVisibleNode", the node returned might not be
      // visible/editable!
      //
      // If the given block does not contain any visible/editable items, we
      // want to skip it and continue our search.

      if (!IsContainer(visNode)) {
        // However, we were given a block that is not a container.  Since the
        // block can not contain anything that's visible, such a block only
        // makes sense if it is visible by itself, like a <hr>.  We want to
        // place the caret in front of that block.
        selNode = visNode->GetParentNode();
        selOffset = selNode ? selNode->IndexOf(visNode) : -1;
        done = true;
      } else {
        bool isEmptyBlock;
        if (NS_SUCCEEDED(IsEmptyNode(visNode, &isEmptyBlock)) &&
            isEmptyBlock) {
          // Skip the empty block
          curNode = visNode->GetParentNode();
          curOffset = curNode ? curNode->IndexOf(visNode) : -1;
          curOffset++;
        } else {
          curNode = visNode;
          curOffset = 0;
        }
        // Keep looping
      }
    } else {
      // Else we found nothing useful
      selNode = curNode;
      selOffset = curOffset;
      done = true;
    }
  }
  return selection->Collapse(selNode, selOffset);
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
    case NS_VK_BACK:
    case NS_VK_DELETE:
      // These keys are handled on EditorBase, so, we can bypass
      // TextEditor.
      return EditorBase::HandleKeyPressEvent(aKeyboardEvent);
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
      return TypedText(NS_LITERAL_STRING("\t"), eTypedText);
    }
    case NS_VK_RETURN:
      if (aKeyboardEvent->IsControl() || aKeyboardEvent->IsAlt() ||
          aKeyboardEvent->IsMeta() || aKeyboardEvent->IsOS()) {
        return NS_OK;
      }
      aKeyboardEvent->PreventDefault(); // consumed
      if (aKeyboardEvent->IsShift()) {
        // only inserts a br node
        return TypedText(EmptyString(), eTypedBR);
      }
      // uses rules to figure out what to insert
      return TypedText(EmptyString(), eTypedBreak);
  }

  // NOTE: On some keyboard layout, some characters are inputted with Control
  // key or Alt key, but at that time, widget sets FALSE to these keys.
  if (!aKeyboardEvent->mCharCode || aKeyboardEvent->IsControl() ||
      aKeyboardEvent->IsAlt() || aKeyboardEvent->IsMeta() ||
      aKeyboardEvent->IsOS()) {
    // we don't PreventDefault() here or keybindings like control-x won't work
    return NS_OK;
  }
  aKeyboardEvent->PreventDefault();
  nsAutoString str(aKeyboardEvent->mCharCode);
  return TypedText(str, eTypedText);
}

static void
AssertParserServiceIsCorrect(nsIAtom* aTag, bool aIsBlock)
{
#ifdef DEBUG
  // Check this against what we would have said with the old code:
  if (aTag == nsGkAtoms::p ||
      aTag == nsGkAtoms::div ||
      aTag == nsGkAtoms::blockquote ||
      aTag == nsGkAtoms::h1 ||
      aTag == nsGkAtoms::h2 ||
      aTag == nsGkAtoms::h3 ||
      aTag == nsGkAtoms::h4 ||
      aTag == nsGkAtoms::h5 ||
      aTag == nsGkAtoms::h6 ||
      aTag == nsGkAtoms::ul ||
      aTag == nsGkAtoms::ol ||
      aTag == nsGkAtoms::dl ||
      aTag == nsGkAtoms::noscript ||
      aTag == nsGkAtoms::form ||
      aTag == nsGkAtoms::hr ||
      aTag == nsGkAtoms::table ||
      aTag == nsGkAtoms::fieldset ||
      aTag == nsGkAtoms::address ||
      aTag == nsGkAtoms::col ||
      aTag == nsGkAtoms::colgroup ||
      aTag == nsGkAtoms::li ||
      aTag == nsGkAtoms::dt ||
      aTag == nsGkAtoms::dd ||
      aTag == nsGkAtoms::legend) {
    if (!aIsBlock) {
      nsAutoString assertmsg (NS_LITERAL_STRING("Parser and editor disagree on blockness: "));

      nsAutoString tagName;
      aTag->ToString(tagName);
      assertmsg.Append(tagName);
      char* assertstr = ToNewCString(assertmsg);
      NS_ASSERTION(aIsBlock, assertstr);
      free(assertstr);
    }
  }
#endif // DEBUG
}

/**
 * Returns true if the id represents an element of block type.
 * Can be used to determine if a new paragraph should be started.
 */
bool
HTMLEditor::NodeIsBlockStatic(const nsINode* aElement)
{
  MOZ_ASSERT(aElement);

  // Nodes we know we want to treat as block
  // even though the parser says they're not:
  if (aElement->IsAnyOfHTMLElements(nsGkAtoms::body,
                                    nsGkAtoms::head,
                                    nsGkAtoms::tbody,
                                    nsGkAtoms::thead,
                                    nsGkAtoms::tfoot,
                                    nsGkAtoms::tr,
                                    nsGkAtoms::th,
                                    nsGkAtoms::td,
                                    nsGkAtoms::li,
                                    nsGkAtoms::dt,
                                    nsGkAtoms::dd,
                                    nsGkAtoms::pre)) {
    return true;
  }

  bool isBlock;
#ifdef DEBUG
  // XXX we can't use DebugOnly here because VC++ is stupid (bug 802884)
  nsresult rv =
#endif
    nsContentUtils::GetParserService()->
    IsBlock(nsContentUtils::GetParserService()->HTMLAtomTagToId(
              aElement->NodeInfo()->NameAtom()),
            isBlock);
  MOZ_ASSERT(rv == NS_OK);

  AssertParserServiceIsCorrect(aElement->NodeInfo()->NameAtom(), isBlock);

  return isBlock;
}

nsresult
HTMLEditor::NodeIsBlockStatic(nsIDOMNode* aNode,
                              bool* aIsBlock)
{
  if (!aNode || !aIsBlock) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<dom::Element> element = do_QueryInterface(aNode);
  *aIsBlock = element && NodeIsBlockStatic(element);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::NodeIsBlock(nsIDOMNode* aNode,
                        bool* aIsBlock)
{
  return NodeIsBlockStatic(aNode, aIsBlock);
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
HTMLEditor::GetBlockNodeParent(nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  nsCOMPtr<nsINode> p = aNode->GetParentNode();

  while (p) {
    if (NodeIsBlockStatic(p)) {
      return p->AsElement();
    }
    p = p->GetParentNode();
  }

  return nullptr;
}

nsIDOMNode*
HTMLEditor::GetBlockNodeParent(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);

  if (!node) {
    NS_NOTREACHED("null node passed to GetBlockNodeParent()");
    return nullptr;
  }

  return GetAsDOMNode(GetBlockNodeParent(node));
}

/**
 * Returns the node if it's a block, otherwise GetBlockNodeParent
 */
Element*
HTMLEditor::GetBlock(nsINode& aNode)
{
  if (NodeIsBlockStatic(&aNode)) {
    return aNode.AsElement();
  }
  return GetBlockNodeParent(&aNode);
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

  if (aContent->IsNodeOfType(nsINode::eTEXT) &&
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

  if (aContent->IsNodeOfType(nsINode::eTEXT) && aOffset > 0) {
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
HTMLEditor::IsVisBreak(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  if (!TextEditUtils::IsBreak(aNode)) {
    return false;
  }
  // Check if there is a later node in block after br
  nsCOMPtr<nsINode> nextNode = GetNextHTMLNode(aNode, true);
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
  nsCOMPtr<nsINode> priorNode = GetPriorHTMLNode(aNode, true);
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
  wsObj.NextVisibleNode(selNode, selOffset, address_of(unused),
                        &visOffset, &visType);
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

/**
 * This routine is needed to provide a bottleneck for typing for logging
 * purposes.  Can't use HandleKeyPress() (above) for that since it takes
 * a nsIDOMKeyEvent* parameter.  So instead we pass enough info through
 * to TypedText() to determine what action to take, but without passing
 * an event.
 */
NS_IMETHODIMP
HTMLEditor::TypedText(const nsAString& aString,
                      ETypingAction aAction)
{
  MOZ_ASSERT(!aString.IsEmpty() || aAction != eTypedText);

  AutoPlaceHolderBatch batch(this, nsGkAtoms::TypingTxnName);

  if (aAction == eTypedBR) {
    // only inserts a br node
    nsCOMPtr<nsIDOMNode> brNode;
    return InsertBR(address_of(brNode));
  }

  return TextEditor::TypedText(aString, aAction);
}

NS_IMETHODIMP
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
    nsCOMPtr<nsIDOMElement> tblElement, cell;
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

already_AddRefed<Element>
HTMLEditor::CreateBR(nsINode* aNode,
                     int32_t aOffset,
                     EDirection aSelect)
{
  nsCOMPtr<nsIDOMNode> parent = GetAsDOMNode(aNode);
  int32_t offset = aOffset;
  nsCOMPtr<nsIDOMNode> outBRNode;
  // We assume everything is fine if the br is not null, irrespective of retval
  CreateBRImpl(address_of(parent), &offset, address_of(outBRNode), aSelect);
  nsCOMPtr<Element> ret = do_QueryInterface(outBRNode);
  return ret.forget();
}

NS_IMETHODIMP
HTMLEditor::CreateBR(nsIDOMNode* aNode,
                     int32_t aOffset,
                     nsCOMPtr<nsIDOMNode>* outBRNode,
                     EDirection aSelect)
{
  nsCOMPtr<nsIDOMNode> parent = aNode;
  int32_t offset = aOffset;
  return CreateBRImpl(address_of(parent), &offset, outBRNode, aSelect);
}

nsresult
HTMLEditor::InsertBR(nsCOMPtr<nsIDOMNode>* outBRNode)
{
  NS_ENSURE_TRUE(outBRNode, NS_ERROR_NULL_POINTER);
  *outBRNode = nullptr;

  // calling it text insertion to trigger moz br treatment by rules
  AutoRules beginRulesSniffing(this, EditAction::insertText, nsIEditor::eNext);

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  if (!selection->Collapsed()) {
    nsresult rv = DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIDOMNode> selNode;
  int32_t selOffset;
  nsresult rv =
    GetStartNodeAndOffset(selection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CreateBR(selNode, selOffset, outBRNode);
  NS_ENSURE_SUCCESS(rv, rv);

  // position selection after br
  selNode = GetNodeLocation(*outBRNode, &selOffset);
  selection->SetInterlinePosition(true);
  return selection->Collapse(selNode, selOffset+1);
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

  ForceCompositionEnd();

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

  AutoEditBatch beginBatching(this);

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
    nsresult rv = DeleteNode(child);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Now insert the new nodes
  int32_t offsetOfNewNode = 0;

  // Loop over the contents of the fragment and move into the document
  while (nsCOMPtr<nsIContent> child = docfrag->GetFirstChild()) {
    nsresult rv = InsertNode(*child, *headNode, offsetOfNewNode++);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::RebuildDocumentFromSource(const nsAString& aSourceString)
{
  ForceCompositionEnd();

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsCOMPtr<Element> bodyElement = GetRoot();
  NS_ENSURE_TRUE(bodyElement, NS_ERROR_NULL_POINTER);

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
  AutoEditBatch beginBatching(this);

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

    nsCOMPtr<Element> divElement =
      CreateElementWithDefaults(NS_LITERAL_STRING("div"));
    NS_ENSURE_TRUE(divElement, NS_ERROR_FAILURE);

    CloneAttributes(bodyElement, divElement);

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
  CloneAttributes(bodyElement, child->AsElement());

  // place selection at first editable content
  return BeginningOfDocument();
}

void
HTMLEditor::NormalizeEOLInsertPosition(nsINode* firstNodeToInsert,
                                       nsCOMPtr<nsIDOMNode>* insertParentNode,
                                       int32_t* insertOffset)
{
  /*
    This function will either correct the position passed in,
    or leave the position unchanged.

    When the (first) item to insert is a block level element,
    and our insertion position is after the last visible item in a line,
    i.e. the insertion position is just before a visible line break <br>,
    we want to skip to the position just after the line break (see bug 68767)

    However, our logic to detect whether we should skip or not
    needs to be more clever.
    We must not skip when the caret appears to be positioned at the beginning
    of a block, in that case skipping the <br> would not insert the <br>
    at the caret position, but after the current empty line.

    So we have several cases to test:

    1) We only ever want to skip, if the next visible thing after the current position is a break

    2) We do not want to skip if there is no previous visible thing at all
       That is detected if the call to PriorVisibleNode gives us an offset of zero.
       Because PriorVisibleNode always positions after the prior node, we would
       see an offset > 0, if there were a prior node.

    3) We do not want to skip, if both the next and the previous visible things are breaks.

    4) We do not want to skip if the previous visible thing is in a different block
       than the insertion position.
  */

  if (!IsBlockNode(firstNodeToInsert)) {
    return;
  }

  WSRunObject wsObj(this, *insertParentNode, *insertOffset);
  nsCOMPtr<nsINode> nextVisNode, prevVisNode;
  int32_t nextVisOffset=0;
  WSType nextVisType;
  int32_t prevVisOffset=0;
  WSType prevVisType;

  nsCOMPtr<nsINode> parent(do_QueryInterface(*insertParentNode));
  wsObj.NextVisibleNode(parent, *insertOffset, address_of(nextVisNode), &nextVisOffset, &nextVisType);
  if (!nextVisNode) {
    return;
  }

  if (!(nextVisType & WSType::br)) {
    return;
  }

  wsObj.PriorVisibleNode(parent, *insertOffset, address_of(prevVisNode), &prevVisOffset, &prevVisType);
  if (!prevVisNode) {
    return;
  }

  if (prevVisType & WSType::br) {
    return;
  }

  if (prevVisType & WSType::thisBlock) {
    return;
  }

  int32_t brOffset=0;
  nsCOMPtr<nsIDOMNode> brNode = GetNodeLocation(GetAsDOMNode(nextVisNode), &brOffset);

  *insertParentNode = brNode;
  *insertOffset = brOffset + 1;
}

NS_IMETHODIMP
HTMLEditor::InsertElementAtSelection(nsIDOMElement* aElement,
                                     bool aDeleteSelection)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);

  ForceCompositionEnd();
  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::insertElement,
                               nsIEditor::eNext);

  RefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  // hand off to the rules system, see if it has anything to say about this
  bool cancel, handled;
  TextRulesInfo ruleInfo(EditAction::insertElement);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled) {
    if (aDeleteSelection) {
      if (!IsBlockNode(element)) {
        // E.g., inserting an image.  In this case we don't need to delete any
        // inline wrappers before we do the insertion.  Otherwise we let
        // DeleteSelectionAndPrepareToCreateNode do the deletion for us, which
        // calls DeleteSelection with aStripWrappers = eStrip.
        rv = DeleteSelection(nsIEditor::eNone, nsIEditor::eNoStrip);
        NS_ENSURE_SUCCESS(rv, rv);
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
      if (HTMLEditUtils::IsNamedAnchor(node)) {
        selection->CollapseToStart();
      } else {
        selection->CollapseToEnd();
      }
    }

    nsCOMPtr<nsIDOMNode> parentSelectedNode;
    int32_t offsetForInsert;
    rv = selection->GetAnchorNode(getter_AddRefs(parentSelectedNode));
    // XXX: ERROR_HANDLING bad XPCOM usage
    if (NS_SUCCEEDED(rv) &&
        NS_SUCCEEDED(selection->GetAnchorOffset(&offsetForInsert)) &&
        parentSelectedNode) {
      // Adjust position based on the node we are going to insert.
      NormalizeEOLInsertPosition(element, address_of(parentSelectedNode),
                                 &offsetForInsert);

      rv = InsertNodeAtPoint(node, address_of(parentSelectedNode),
                             &offsetForInsert, false);
      NS_ENSURE_SUCCESS(rv, rv);
      // Set caret after element, but check for special case
      //  of inserting table-related elements: set in first cell instead
      if (!SetCaretInTableCell(aElement)) {
        rv = SetCaretAfterElement(aElement);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // check for inserting a whole table at the end of a block. If so insert
      // a br after it.
      if (HTMLEditUtils::IsTable(node)) {
        if (IsLastEditableChild(element)) {
          nsCOMPtr<nsIDOMNode> brNode;
          rv = CreateBR(parentSelectedNode, offsetForInsert + 1,
                        address_of(brNode));
          NS_ENSURE_SUCCESS(rv, rv);
          selection->Collapse(parentSelectedNode, offsetForInsert+1);
        }
      }
    }
  }
  rv = rules->DidDoAction(selection, &ruleInfo, rv);
  return rv;
}


/**
 * InsertNodeAtPoint() attempts to insert aNode into the document, at a point
 * specified by {*ioParent,*ioOffset}.  Checks with strict dtd to see if
 * containment is allowed.  If not allowed, will attempt to find a parent in
 * the parent hierarchy of *ioParent that will accept aNode as a child.  If
 * such a parent is found, will split the document tree from
 * {*ioParent,*ioOffset} up to parent, and then insert aNode.
 * ioParent & ioOffset are then adjusted to point to the actual location that
 * aNode was inserted at.  aNoEmptyNodes specifies if the splitting process
 * is allowed to reslt in empty nodes.
 *
 * @param aNode             Node to insert.
 * @param ioParent          Insertion parent.
 * @param ioOffset          Insertion offset.
 * @param aNoEmptyNodes     Splitting can result in empty nodes?
 */
nsresult
HTMLEditor::InsertNodeAtPoint(nsIDOMNode* aNode,
                              nsCOMPtr<nsIDOMNode>* ioParent,
                              int32_t* ioOffset,
                              bool aNoEmptyNodes)
{
  nsCOMPtr<nsIContent> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(ioParent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(*ioParent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(ioOffset, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> parent = do_QueryInterface(*ioParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIContent> topChild = parent;
  nsCOMPtr<nsIContent> origParent = parent;

  // Search up the parent chain to find a suitable container
  while (!CanContain(*parent, *node)) {
    // If the current parent is a root (body or table element)
    // then go no further - we can't insert
    if (parent->IsHTMLElement(nsGkAtoms::body) ||
        HTMLEditUtils::IsTableElement(parent)) {
      return NS_ERROR_FAILURE;
    }
    // Get the next parent
    NS_ENSURE_TRUE(parent->GetParentNode(), NS_ERROR_FAILURE);
    if (!IsEditable(parent->GetParentNode())) {
      // There's no suitable place to put the node in this editing host.  Maybe
      // someone is trying to put block content in a span.  So just put it
      // where we were originally asked.
      parent = topChild = origParent;
      break;
    }
    topChild = parent;
    parent = parent->GetParent();
  }
  if (parent != topChild) {
    // we need to split some levels above the original selection parent
    int32_t offset = SplitNodeDeep(*topChild, *origParent, *ioOffset,
                                   aNoEmptyNodes ? EmptyContainers::no
                                                 : EmptyContainers::yes);
    NS_ENSURE_STATE(offset != -1);
    *ioParent = GetAsDOMNode(parent);
    *ioOffset = offset;
  }
  // Now we can insert the new node
  return InsertNode(*node, *parent, *ioOffset);
}

NS_IMETHODIMP
HTMLEditor::SelectElement(nsIDOMElement* aElement)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_STATE(element || !aElement);

  // Must be sure that element is contained in the document body
  if (!IsDescendantOfEditorRoot(element)) {
    return NS_ERROR_NULL_POINTER;
  }

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode>parent;
  nsresult rv = aElement->GetParentNode(getter_AddRefs(parent));
  if (NS_SUCCEEDED(rv) && parent) {
    int32_t offsetInParent = GetChildOffset(aElement, parent);

    // Collapse selection to just before desired element,
    rv = selection->Collapse(parent, offsetInParent);
    if (NS_SUCCEEDED(rv)) {
      // then extend it to just after
      rv = selection->Extend(parent, offsetInParent + 1);
    }
  }
  return rv;
}

NS_IMETHODIMP
HTMLEditor::SetCaretAfterElement(nsIDOMElement* aElement)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_STATE(element || !aElement);

  // Be sure the element is contained in the document body
  if (!aElement || !IsDescendantOfEditorRoot(element)) {
    return NS_ERROR_NULL_POINTER;
  }

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode>parent;
  nsresult rv = aElement->GetParentNode(getter_AddRefs(parent));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
  int32_t offsetInParent = GetChildOffset(aElement, parent);
  // Collapse selection to just after desired element,
  return selection->Collapse(parent, offsetInParent + 1);
}

NS_IMETHODIMP
HTMLEditor::SetParagraphFormat(const nsAString& aParagraphFormat)
{
  nsAutoString tag; tag.Assign(aParagraphFormat);
  ToLowerCase(tag);
  if (tag.EqualsLiteral("dd") || tag.EqualsLiteral("dt")) {
    return MakeDefinitionItem(tag);
  }
  return InsertBasicBlock(tag);
}

NS_IMETHODIMP
HTMLEditor::GetParagraphState(bool* aMixed,
                              nsAString& outFormat)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  RefPtr<HTMLEditRules> htmlRules =
    static_cast<HTMLEditRules*>(mRules.get());

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
  int32_t offset = selection->GetRangeAt(0)->StartOffset();
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  // is the selection collapsed?
  nsCOMPtr<nsINode> nodeToExamine;
  if (selection->Collapsed() || IsTextNode(parent)) {
    // we want to look at the parent and ancestors
    nodeToExamine = parent;
  } else {
    // otherwise we want to look at the first editable node after
    // {parent,offset} and its ancestors for divs with alignment on them
    nodeToExamine = parent->GetChildAt(offset);
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
      mCSSEditUtils->GetComputedProperty(*blockParent,
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
      mCSSEditUtils->GetDefaultBackgroundColor(aOutColor);
    }
  }
  else {
    // no, we are querying the text background for the Text Highlight button
    if (IsTextNode(nodeToExamine)) {
      // if the node of interest is a text node, let's climb a level
      nodeToExamine = nodeToExamine->GetParentNode();
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
        mCSSEditUtils->GetComputedProperty(*nodeToExamine,
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

NS_IMETHODIMP
HTMLEditor::GetHTMLBackgroundColorState(bool* aMixed,
                                        nsAString& aOutColor)
{
  //TODO: We don't handle "mixed" correctly!
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  aOutColor.Truncate();

  nsCOMPtr<nsIDOMElement> domElement;
  int32_t selectedCount;
  nsAutoString tagName;
  nsresult rv = GetSelectedOrParentTableElement(tagName,
                                                &selectedCount,
                                                getter_AddRefs(domElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<dom::Element> element = do_QueryInterface(domElement);

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
  RefPtr<HTMLEditRules> htmlRules =
    static_cast<HTMLEditRules*>(mRules.get());

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

  RefPtr<HTMLEditRules> htmlRules =
    static_cast<HTMLEditRules*>(mRules.get());

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
  RefPtr<HTMLEditRules> htmlRules =
    static_cast<HTMLEditRules*>(mRules.get());

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

  RefPtr<HTMLEditRules> htmlRules =
    static_cast<HTMLEditRules*>(mRules.get());

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
  nsCOMPtr<nsIEditRules> rules(mRules);

  bool cancel, handled;

  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::makeList, nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  TextRulesInfo ruleInfo(EditAction::makeList);
  ruleInfo.blockType = &aListType;
  ruleInfo.entireList = entireList;
  ruleInfo.bulletType = &aBulletType;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled) {
    // Find out if the selection is collapsed:
    bool isCollapsed = selection->Collapsed();

    NS_ENSURE_TRUE(selection->GetRangeAt(0) &&
                   selection->GetRangeAt(0)->GetStartContainer() &&
                   selection->GetRangeAt(0)->GetStartContainer()->IsContent(),
                   NS_ERROR_FAILURE);
    OwningNonNull<nsIContent> node =
      *selection->GetRangeAt(0)->GetStartContainer()->AsContent();
    int32_t offset = selection->GetRangeAt(0)->StartOffset();

    if (isCollapsed) {
      // have to find a place to put the list
      nsCOMPtr<nsIContent> parent = node;
      nsCOMPtr<nsIContent> topChild = node;

      nsCOMPtr<nsIAtom> listAtom = NS_Atomize(aListType);
      while (!CanContainTag(*parent, *listAtom)) {
        topChild = parent;
        parent = parent->GetParent();
      }

      if (parent != node) {
        // we need to split up to the child of parent
        offset = SplitNodeDeep(*topChild, *node, offset);
        NS_ENSURE_STATE(offset != -1);
      }

      // make a list
      nsCOMPtr<Element> newList = CreateNode(listAtom, parent, offset);
      NS_ENSURE_STATE(newList);
      // make a list item
      nsCOMPtr<Element> newItem = CreateNode(nsGkAtoms::li, newList, 0);
      NS_ENSURE_STATE(newItem);
      rv = selection->Collapse(newItem, 0);
      NS_ENSURE_SUCCESS(rv, rv);
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
  nsCOMPtr<nsIEditRules> rules(mRules);

  bool cancel, handled;

  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::removeList, nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  TextRulesInfo ruleInfo(EditAction::removeList);
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
HTMLEditor::MakeDefinitionItem(const nsAString& aItemType)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  bool cancel, handled;

  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::makeDefListItem,
                               nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  TextRulesInfo ruleInfo(EditAction::makeDefListItem);
  ruleInfo.blockType = &aItemType;
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
HTMLEditor::InsertBasicBlock(const nsAString& aBlockType)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  bool cancel, handled;

  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::makeBasicBlock,
                               nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  TextRulesInfo ruleInfo(EditAction::makeBasicBlock);
  ruleInfo.blockType = &aBlockType;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled) {
    // Find out if the selection is collapsed:
    bool isCollapsed = selection->Collapsed();

    NS_ENSURE_TRUE(selection->GetRangeAt(0) &&
                   selection->GetRangeAt(0)->GetStartContainer() &&
                   selection->GetRangeAt(0)->GetStartContainer()->IsContent(),
                   NS_ERROR_FAILURE);
    OwningNonNull<nsIContent> node =
      *selection->GetRangeAt(0)->GetStartContainer()->AsContent();
    int32_t offset = selection->GetRangeAt(0)->StartOffset();

    if (isCollapsed) {
      // have to find a place to put the block
      nsCOMPtr<nsIContent> parent = node;
      nsCOMPtr<nsIContent> topChild = node;

      nsCOMPtr<nsIAtom> blockAtom = NS_Atomize(aBlockType);
      while (!CanContainTag(*parent, *blockAtom)) {
        NS_ENSURE_TRUE(parent->GetParent(), NS_ERROR_FAILURE);
        topChild = parent;
        parent = parent->GetParent();
      }

      if (parent != node) {
        // we need to split up to the child of parent
        offset = SplitNodeDeep(*topChild, *node, offset);
        NS_ENSURE_STATE(offset != -1);
      }

      // make a block
      nsCOMPtr<Element> newBlock = CreateNode(blockAtom, parent, offset);
      NS_ENSURE_STATE(newBlock);

      // reposition selection to inside the block
      rv = selection->Collapse(newBlock, 0);
      NS_ENSURE_SUCCESS(rv, rv);
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
  nsCOMPtr<nsIEditRules> rules(mRules);

  bool cancel, handled;
  EditAction opID = EditAction::indent;
  if (aIndent.LowerCaseEqualsLiteral("outdent")) {
    opID = EditAction::outdent;
  }
  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, opID, nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  TextRulesInfo ruleInfo(opID);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  if (!handled) {
    // Do default - insert a blockquote node if selection collapsed
    bool isCollapsed = selection->Collapsed();

    NS_ENSURE_TRUE(selection->GetRangeAt(0) &&
                   selection->GetRangeAt(0)->GetStartContainer() &&
                   selection->GetRangeAt(0)->GetStartContainer()->IsContent(),
                   NS_ERROR_FAILURE);
    OwningNonNull<nsIContent> node =
      *selection->GetRangeAt(0)->GetStartContainer()->AsContent();
    int32_t offset = selection->GetRangeAt(0)->StartOffset();

    if (aIndent.EqualsLiteral("indent")) {
      if (isCollapsed) {
        // have to find a place to put the blockquote
        nsCOMPtr<nsIContent> parent = node;
        nsCOMPtr<nsIContent> topChild = node;
        while (!CanContainTag(*parent, *nsGkAtoms::blockquote)) {
          NS_ENSURE_TRUE(parent->GetParent(), NS_ERROR_FAILURE);
          topChild = parent;
          parent = parent->GetParent();
        }

        if (parent != node) {
          // we need to split up to the child of parent
          offset = SplitNodeDeep(*topChild, *node, offset);
          NS_ENSURE_STATE(offset != -1);
        }

        // make a blockquote
        nsCOMPtr<Element> newBQ = CreateNode(nsGkAtoms::blockquote, parent, offset);
        NS_ENSURE_STATE(newBQ);
        // put a space in it so layout will draw the list item
        rv = selection->Collapse(newBQ, 0);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = InsertText(NS_LITERAL_STRING(" "));
        NS_ENSURE_SUCCESS(rv, rv);
        // reposition selection to before the space character
        NS_ENSURE_STATE(selection->GetRangeAt(0));
        rv = selection->Collapse(selection->GetRangeAt(0)->GetStartContainer(),
                                 0);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  return rules->DidDoAction(selection, &ruleInfo, rv);
}

//TODO: IMPLEMENT ALIGNMENT!

NS_IMETHODIMP
HTMLEditor::Align(const nsAString& aAlignType)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::align, nsIEditor::eNext);

  bool cancel, handled;

  // Find out if the selection is collapsed:
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  TextRulesInfo ruleInfo(EditAction::align);
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
    NS_ENSURE_TRUE(selection, nullptr);

    nsCOMPtr<nsINode> anchorNode = selection->GetAnchorNode();
    NS_ENSURE_TRUE(anchorNode, nullptr);

    // Try to get the actual selected node
    if (anchorNode->HasChildNodes() && anchorNode->IsContent()) {
      node = anchorNode->GetChildAt(selection->AnchorOffset());
    }
    // Anchor node is probably a text node - just use that
    if (!node) {
      node = anchorNode;
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
                                        nsIDOMNode* aNode,
                                        nsIDOMElement** aReturn)
{
  NS_ENSURE_TRUE(!aTagName.IsEmpty(), NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aReturn, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  nsCOMPtr<Element> parent =
    GetElementOrParentByTagName(aTagName, node);
  nsCOMPtr<nsIDOMElement> ret = do_QueryInterface(parent);

  if (!ret) {
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  ret.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetSelectedElement(const nsAString& aTagName,
                               nsIDOMElement** aReturn)
{
  NS_ENSURE_TRUE(aReturn , NS_ERROR_NULL_POINTER);

  // default is null - no element found
  *aReturn = nullptr;

  // First look for a single element in selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  bool bNodeFound = false;
  bool isCollapsed = selection->Collapsed();

  nsAutoString domTagName;
  nsAutoString TagName(aTagName);
  ToLowerCase(TagName);
  // Empty string indicates we should match any element tag
  bool anyTag = (TagName.IsEmpty());
  bool isLinkTag = IsLinkTag(TagName);
  bool isNamedAnchorTag = IsNamedAnchorTag(TagName);

  nsCOMPtr<nsIDOMElement> selectedElement;
  RefPtr<nsRange> range = selection->GetRangeAt(0);
  NS_ENSURE_STATE(range);

  nsCOMPtr<nsIDOMNode> startContainer;
  int32_t startOffset, endOffset;
  nsresult rv = range->GetStartContainer(getter_AddRefs(startContainer));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = range->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> endContainer;
  rv = range->GetEndContainer(getter_AddRefs(endContainer));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = range->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Optimization for a single selected element
  if (startContainer && startContainer == endContainer &&
      endOffset - startOffset == 1) {
    nsCOMPtr<nsIDOMNode> selectedNode = GetChildAt(startContainer, startOffset);
    NS_ENSURE_SUCCESS(rv, NS_OK);
    if (selectedNode) {
      selectedNode->GetNodeName(domTagName);
      ToLowerCase(domTagName);

      // Test for appropriate node type requested
      if (anyTag || (TagName == domTagName) ||
          (isLinkTag && HTMLEditUtils::IsLink(selectedNode)) ||
          (isNamedAnchorTag && HTMLEditUtils::IsNamedAnchor(selectedNode))) {
        bNodeFound = true;
        selectedElement = do_QueryInterface(selectedNode);
      }
    }
  }

  if (!bNodeFound) {
    if (isLinkTag) {
      // Link tag is a special case - we return the anchor node
      //  found for any selection that is totally within a link,
      //  included a collapsed selection (just a caret in a link)
      nsCOMPtr<nsIDOMNode> anchorNode;
      rv = selection->GetAnchorNode(getter_AddRefs(anchorNode));
      NS_ENSURE_SUCCESS(rv, rv);
      int32_t anchorOffset = -1;
      if (anchorNode) {
        selection->GetAnchorOffset(&anchorOffset);
      }

      nsCOMPtr<nsIDOMNode> focusNode;
      rv = selection->GetFocusNode(getter_AddRefs(focusNode));
      NS_ENSURE_SUCCESS(rv, rv);
      int32_t focusOffset = -1;
      if (focusNode) {
        selection->GetFocusOffset(&focusOffset);
      }

      // Link node must be the same for both ends of selection
      if (NS_SUCCEEDED(rv) && anchorNode) {
        nsCOMPtr<nsIDOMElement> parentLinkOfAnchor;
        rv = GetElementOrParentByTagName(NS_LITERAL_STRING("href"),
                                         anchorNode,
                                         getter_AddRefs(parentLinkOfAnchor));
        // XXX: ERROR_HANDLING  can parentLinkOfAnchor be null?
        if (NS_SUCCEEDED(rv) && parentLinkOfAnchor) {
          if (isCollapsed) {
            // We have just a caret in the link
            bNodeFound = true;
          } else if (focusNode) {
            // Link node must be the same for both ends of selection.
            nsCOMPtr<nsIDOMElement> parentLinkOfFocus;
            rv = GetElementOrParentByTagName(NS_LITERAL_STRING("href"),
                                             focusNode,
                                             getter_AddRefs(parentLinkOfFocus));
            if (NS_SUCCEEDED(rv) && parentLinkOfFocus == parentLinkOfAnchor) {
              bNodeFound = true;
            }
          }

          // We found a link node parent
          if (bNodeFound) {
            // GetElementOrParentByTagName addref'd this, so we don't need to do it here
            *aReturn = parentLinkOfAnchor;
            NS_IF_ADDREF(*aReturn);
            return NS_OK;
          }
        } else if (anchorOffset >= 0) {
          // Check if link node is the only thing selected
          nsCOMPtr<nsIDOMNode> anchorChild;
          anchorChild = GetChildAt(anchorNode,anchorOffset);
          if (anchorChild && HTMLEditUtils::IsLink(anchorChild) &&
              anchorNode == focusNode && focusOffset == anchorOffset + 1) {
            selectedElement = do_QueryInterface(anchorChild);
            bNodeFound = true;
          }
        }
      }
    }

    if (!isCollapsed) {
      RefPtr<nsRange> currange = selection->GetRangeAt(0);
      if (currange) {
        nsCOMPtr<nsIContentIterator> iter =
          do_CreateInstance("@mozilla.org/content/post-content-iterator;1",
                            &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        iter->Init(currange);
        // loop through the content iterator for each content node
        while (!iter->IsDone()) {
          // Query interface to cast nsIContent to nsIDOMNode
          //  then get tagType to compare to  aTagName
          // Clone node of each desired type and append it to the aDomFrag
          selectedElement = do_QueryInterface(iter->GetCurrentNode());
          if (selectedElement) {
            // If we already found a node, then we have another element,
            //  thus there's not just one element selected
            if (bNodeFound) {
              bNodeFound = false;
              break;
            }

            selectedElement->GetNodeName(domTagName);
            ToLowerCase(domTagName);

            if (anyTag) {
              // Get name of first selected element
              selectedElement->GetTagName(TagName);
              ToLowerCase(TagName);
              anyTag = false;
            }

            // The "A" tag is a pain,
            //  used for both link(href is set) and "Named Anchor"
            nsCOMPtr<nsIDOMNode> selectedNode = do_QueryInterface(selectedElement);
            if ((isLinkTag &&
                 HTMLEditUtils::IsLink(selectedNode)) ||
                (isNamedAnchorTag &&
                 HTMLEditUtils::IsNamedAnchor(selectedNode))) {
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
  }

  if (!bNodeFound) {
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  *aReturn = selectedElement;
  if (selectedElement) {
    // Getters must addref
    NS_ADDREF(*aReturn);
  }
  return rv;
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
  nsCOMPtr<nsIAtom> realTagAtom = NS_Atomize(realTagName);
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
                                      nsIDOMElement** aReturn)
{
  NS_ENSURE_TRUE(!aTagName.IsEmpty() && aReturn, NS_ERROR_NULL_POINTER);
  *aReturn = nullptr;

  nsCOMPtr<Element> newElement = CreateElementWithDefaults(aTagName);
  nsCOMPtr<nsIDOMElement> ret = do_QueryInterface(newElement);
  NS_ENSURE_TRUE(ret, NS_ERROR_FAILURE);

  ret.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
  NS_ENSURE_TRUE(aAnchorElement, NS_ERROR_NULL_POINTER);

  // We must have a real selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  if (selection->Collapsed()) {
    NS_WARNING("InsertLinkAroundSelection called but there is no selection!!!");
    return NS_OK;
  }

  // Be sure we were given an anchor element
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aAnchorElement);
  if (!anchor) {
    return NS_OK;
  }

  nsAutoString href;
  nsresult rv = anchor->GetHref(href);
  NS_ENSURE_SUCCESS(rv, rv);
  if (href.IsEmpty()) {
    return NS_OK;
  }

  AutoEditBatch beginBatching(this);

  // Set all attributes found on the supplied anchor element
  nsCOMPtr<nsIDOMMozNamedAttrMap> attrMap;
  aAnchorElement->GetAttributes(getter_AddRefs(attrMap));
  NS_ENSURE_TRUE(attrMap, NS_ERROR_FAILURE);

  uint32_t count;
  attrMap->GetLength(&count);
  nsAutoString name, value;

  for (uint32_t i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMAttr> attribute;
    rv = attrMap->Item(i, getter_AddRefs(attribute));
    NS_ENSURE_SUCCESS(rv, rv);

    if (attribute) {
      // We must clear the string buffers
      //   because GetName, GetValue appends to previous string!
      name.Truncate();
      value.Truncate();

      rv = attribute->GetName(name);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = attribute->GetValue(value);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = SetInlineProperty(nsGkAtoms::a, name, value);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

nsresult
HTMLEditor::SetHTMLBackgroundColor(const nsAString& aColor)
{
  MOZ_ASSERT(IsInitialized(), "The HTMLEditor hasn't been initialized yet");

  // Find a selected or enclosing table element to set background on
  nsCOMPtr<nsIDOMElement> element;
  int32_t selectedCount;
  nsAutoString tagName;
  nsresult rv = GetSelectedOrParentTableElement(tagName, &selectedCount,
                                                getter_AddRefs(element));
  NS_ENSURE_SUCCESS(rv, rv);

  bool setColor = !aColor.IsEmpty();

  NS_NAMED_LITERAL_STRING(bgcolor, "bgcolor");
  if (element) {
    if (selectedCount > 0) {
      // Traverse all selected cells
      nsCOMPtr<nsIDOMElement> cell;
      rv = GetFirstSelectedCell(nullptr, getter_AddRefs(cell));
      if (NS_SUCCEEDED(rv) && cell) {
        while (cell) {
          rv = setColor ? SetAttribute(cell, bgcolor, aColor) :
                          RemoveAttribute(cell, bgcolor);
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
    element = do_QueryInterface(GetRoot());
    NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);
  }
  // Use the editor method that goes through the transaction system
  return setColor ? SetAttribute(element, bgcolor, aColor) :
                    RemoveAttribute(element, bgcolor);
}

NS_IMETHODIMP
HTMLEditor::SetBodyAttribute(const nsAString& aAttribute,
                             const nsAString& aValue)
{
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level

  MOZ_ASSERT(IsInitialized(), "The HTMLEditor hasn't been initialized yet");

  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement = do_QueryInterface(GetRoot());
  NS_ENSURE_TRUE(bodyElement, NS_ERROR_NULL_POINTER);

  // Use the editor method that goes through the transaction system
  return SetAttribute(bodyElement, aAttribute, aValue);
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
      nsCOMPtr<nsIDOMNode> node (do_QueryInterface(iter->GetCurrentNode()));
      if (node) {
        // Let nsURIRefObject make the hard decisions:
        nsCOMPtr<nsIURIRefObject> refObject;
        rv = NS_NewHTMLURIRefObject(getter_AddRefs(refObject), node);
        if (NS_SUCCEEDED(rv)) {
          nodes->AppendElement(refObject, false);
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

  return ps->GetDocument()->CSSLoader()->
    LoadSheet(uaURI, false, nullptr, EmptyCString(), this);
}

NS_IMETHODIMP
HTMLEditor::RemoveStyleSheet(const nsAString& aURL)
{
  RefPtr<StyleSheet> sheet = GetStyleSheetForURL(aURL);
  NS_ENSURE_TRUE(sheet, NS_ERROR_UNEXPECTED);

  RefPtr<RemoveStyleSheetTransaction> transaction =
    CreateTxnForRemoveStyleSheet(sheet);
  if (!transaction) {
    return NS_ERROR_NULL_POINTER;
  }

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

  ps->RestyleForCSSRuleChanges();

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
  ps->RestyleForCSSRuleChanges();

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
  sheet->SetAssociatedDocument(document, StyleSheet::NotOwnedByDocument);

  return sheet->SetDisabled(!aEnable);
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
  sheet->SetAssociatedDocument(document, StyleSheet::NotOwnedByDocument);

  if (sheet->IsServo()) {
    // XXXheycam ServoStyleSheets don't support being enabled/disabled yet.
    NS_ERROR("stylo: ServoStyleSheets can't be disabled yet");
    return true;
  }
  sheet->AsGecko()->SetDisabled(false);
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
        nsCOMPtr<nsIDOMNode> domNode = do_QueryInterface(node);
        nodes->AppendElement(domNode, false);
       }
     }
     iter->Next();
   }

  nodes.forget(aNodeList);
  return rv;
}

NS_IMETHODIMP
HTMLEditor::DeleteSelectionImpl(EDirection aAction,
                                EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  nsresult rv = EditorBase::DeleteSelectionImpl(aAction, aStripWrappers);
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
    rv = DeleteNode(content);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
HTMLEditor::DeleteNode(nsINode* aNode)
{
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode);
  return DeleteNode(node);
}

NS_IMETHODIMP
HTMLEditor::DeleteNode(nsIDOMNode* aNode)
{
  // do nothing if the node is read-only
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (NS_WARN_IF(!IsModifiableNode(aNode) && !IsMozEditorBogusNode(content))) {
    return NS_ERROR_FAILURE;
  }

  return EditorBase::DeleteNode(aNode);
}

nsresult
HTMLEditor::DeleteText(nsGenericDOMDataNode& aCharData,
                       uint32_t aOffset,
                       uint32_t aLength)
{
  // Do nothing if the node is read-only
  if (!IsModifiableNode(&aCharData)) {
    return NS_ERROR_FAILURE;
  }

  return EditorBase::DeleteText(aCharData, aOffset, aLength);
}

nsresult
HTMLEditor::InsertTextImpl(const nsAString& aStringToInsert,
                           nsCOMPtr<nsINode>* aInOutNode,
                           int32_t* aInOutOffset,
                           nsIDocument* aDoc)
{
  // Do nothing if the node is read-only
  if (!IsModifiableNode(*aInOutNode)) {
    return NS_ERROR_FAILURE;
  }

  return EditorBase::InsertTextImpl(aStringToInsert, aInOutNode, aInOutOffset,
                                    aDoc);
}

void
HTMLEditor::ContentAppended(nsIDocument* aDocument,
                            nsIContent* aContainer,
                            nsIContent* aFirstNewContent,
                            int32_t aIndexInContainer)
{
  DoContentInserted(aDocument, aContainer, aFirstNewContent, aIndexInContainer,
                    eAppended);
}

void
HTMLEditor::ContentInserted(nsIDocument* aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            int32_t aIndexInContainer)
{
  DoContentInserted(aDocument, aContainer, aChild, aIndexInContainer,
                    eInserted);
}

bool
HTMLEditor::IsInObservedSubtree(nsIDocument* aDocument,
                                nsIContent* aContainer,
                                nsIContent* aChild)
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
HTMLEditor::DoContentInserted(nsIDocument* aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              int32_t aIndexInContainer,
                              InsertedOrAppended aInsertedOrAppended)
{
  if (!IsInObservedSubtree(aDocument, aContainer, aChild)) {
    return;
  }

  nsCOMPtr<nsIHTMLEditor> kungFuDeathGrip(this);

  if (ShouldReplaceRootElement()) {
    nsContentUtils::AddScriptRunner(
      NewRunnableMethod("HTMLEditor::ResetRootElementAndEventTarget",
                        this,
                        &HTMLEditor::ResetRootElementAndEventTarget));
  }
  // We don't need to handle our own modifications
  else if (!mAction && (aContainer ? aContainer->IsEditable() : aDocument->IsEditable())) {
    if (IsMozEditorBogusNode(aChild)) {
      // Ignore insertion of the bogus node
      return;
    }
    // Protect the edit rules object from dying
    nsCOMPtr<nsIEditRules> rules(mRules);
    rules->DocumentModified();

    // Update spellcheck for only the newly-inserted node (bug 743819)
    if (mInlineSpellChecker) {
      RefPtr<nsRange> range = new nsRange(aChild);
      int32_t endIndex = aIndexInContainer + 1;
      if (aInsertedOrAppended == eAppended) {
        // Count all the appended nodes
        nsIContent* sibling = aChild->GetNextSibling();
        while (sibling) {
          endIndex++;
          sibling = sibling->GetNextSibling();
        }
      }
      nsresult rv = range->SetStartAndEnd(aContainer, aIndexInContainer,
                                          aContainer, endIndex);
      if (NS_SUCCEEDED(rv)) {
        mInlineSpellChecker->SpellCheckRange(range);
      }
    }
  }
}

void
HTMLEditor::ContentRemoved(nsIDocument* aDocument,
                           nsIContent* aContainer,
                           nsIContent* aChild,
                           int32_t aIndexInContainer,
                           nsIContent* aPreviousSibling)
{
  if (!IsInObservedSubtree(aDocument, aContainer, aChild)) {
    return;
  }

  nsCOMPtr<nsIHTMLEditor> kungFuDeathGrip(this);

  if (SameCOMIdentity(aChild, mRootElement)) {
    nsContentUtils::AddScriptRunner(
      NewRunnableMethod("HTMLEditor::ResetRootElementAndEventTarget",
                        this,
                        &HTMLEditor::ResetRootElementAndEventTarget));
  }
  // We don't need to handle our own modifications
  else if (!mAction && (aContainer ? aContainer->IsEditable() : aDocument->IsEditable())) {
    if (aChild && IsMozEditorBogusNode(aChild)) {
      // Ignore removal of the bogus node
      return;
    }
    // Protect the edit rules object from dying
    nsCOMPtr<nsIEditRules> rules(mRules);
    rules->DocumentModified();
  }
}

NS_IMETHODIMP_(bool)
HTMLEditor::IsModifiableNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return IsModifiableNode(node);
}

bool
HTMLEditor::IsModifiableNode(nsINode* aNode)
{
  return !aNode || aNode->IsEditable();
}

NS_IMETHODIMP
HTMLEditor::GetIsSelectionEditable(bool* aIsSelectionEditable)
{
  MOZ_ASSERT(aIsSelectionEditable);

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // Per the editing spec as of June 2012: we have to have a selection whose
  // start and end nodes are editable, and which share an ancestor editing
  // host.  (Bug 766387.)
  *aIsSelectionEditable = selection->RangeCount() &&
                          selection->GetAnchorNode()->IsEditable() &&
                          selection->GetFocusNode()->IsEditable();

  if (*aIsSelectionEditable) {
    nsINode* commonAncestor =
      selection->GetAnchorFocusRange()->GetCommonAncestor();
    while (commonAncestor && !commonAncestor->IsEditable()) {
      commonAncestor = commonAncestor->GetParentNode();
    }
    if (!commonAncestor) {
      // No editable common ancestor
      *aIsSelectionEditable = false;
    }
  }

  return NS_OK;
}

static nsresult
SetSelectionAroundHeadChildren(Selection* aSelection,
                               nsCOMPtr<nsIDocument>& aDocument)
{
  MOZ_ASSERT(aDocument);

  // Set selection around <head> node
  dom::Element* headNode = aDocument->GetHeadElement();
  NS_ENSURE_STATE(headNode);

  // Collapse selection to before first child of the head,
  nsresult rv = aSelection->Collapse(headNode, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  // Then extend it to just after.
  uint32_t childCount = headNode->GetChildCount();
  return aSelection->Extend(headNode, childCount + 1);
}

NS_IMETHODIMP
HTMLEditor::GetHeadContentsAsHTML(nsAString& aOutputString)
{
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // Save current selection
  AutoSelectionRestorer selectionRestorer(selection, this);

  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsresult rv = SetSelectionAroundHeadChildren(selection, document);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = OutputToString(NS_LITERAL_STRING("text/html"),
                      nsIDocumentEncoder::OutputSelectionOnly,
                      aOutputString);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Selection always includes <body></body>,
  //  so terminate there
  nsReadingIterator<char16_t> findIter,endFindIter;
  aOutputString.BeginReading(findIter);
  aOutputString.EndReading(endFindIter);
  //counting on our parser to always lower case!!!
  if (CaseInsensitiveFindInReadable(NS_LITERAL_STRING("<body"),
                                    findIter, endFindIter)) {
    nsReadingIterator<char16_t> beginIter;
    aOutputString.BeginReading(beginIter);
    int32_t offset = Distance(beginIter, findIter);//get the distance

    nsWritingIterator<char16_t> writeIter;
    aOutputString.BeginWriting(writeIter);
    // Ensure the string ends in a newline
    char16_t newline ('\n');
    findIter.advance(-1);
    if (!offset || (offset > 0 && (*findIter) != newline)) {
      writeIter.advance(offset);
      *writeIter = newline;
      aOutputString.Truncate(offset+1);
    }
  }
  return NS_OK;
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
                             bool aWasAlternate,
                             nsresult aStatus)
{
  AutoEditBatch batchIt(this);

  if (!mLastStyleSheetURL.IsEmpty())
    RemoveStyleSheet(mLastStyleSheetURL);

  RefPtr<AddStyleSheetTransaction> transaction =
    CreateTxnForAddStyleSheet(aSheet);
  if (!transaction) {
    return NS_OK;
  }

  nsresult rv = DoTransaction(transaction);
  if (NS_SUCCEEDED(rv)) {
    // Get the URI, then url spec from the sheet
    nsAutoCString spec;
    rv = aSheet->GetSheetURI()->GetSpec(spec);

    if (NS_SUCCEEDED(rv)) {
      // Save it so we can remove before applying the next one
      mLastStyleSheetURL.AssignWithConversion(spec.get());

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
NS_IMETHODIMP
HTMLEditor::StartOperation(EditAction opID,
                           nsIEditor::EDirection aDirection)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

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
NS_IMETHODIMP
HTMLEditor::EndOperation()
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  // post processing
  nsresult rv = rules ? rules->AfterEdit(mAction, mDirection) : NS_OK;
  EditorBase::EndOperation();  // will clear mAction, mDirection
  return rv;
}

bool
HTMLEditor::TagCanContainTag(nsIAtom& aParentTag,
                             nsIAtom& aChildTag)
{
  nsIParserService* parserService = nsContentUtils::GetParserService();

  int32_t childTagEnum;
  // XXX Should this handle #cdata-section too?
  if (&aChildTag == nsGkAtoms::textTagName) {
    childTagEnum = eHTMLTag_text;
  } else {
    childTagEnum = parserService->HTMLAtomTagToId(&aChildTag);
  }

  int32_t parentTagEnum = parserService->HTMLAtomTagToId(&aParentTag);
  return HTMLEditUtils::CanContain(parentTagEnum, childTagEnum);
}

bool
HTMLEditor::IsContainer(nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  int32_t tagEnum;
  // XXX Should this handle #cdata-section too?
  if (aNode->IsNodeOfType(nsINode::eTEXT)) {
    tagEnum = eHTMLTag_text;
  } else {
    tagEnum =
      nsContentUtils::GetParserService()->HTMLStringTagToId(aNode->NodeName());
  }

  return HTMLEditUtils::IsContainer(tagEnum);
}

bool
HTMLEditor::IsContainer(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  if (!node) {
    return false;
  }
  return IsContainer(node);
}


nsresult
HTMLEditor::SelectEntireDocument(Selection* aSelection)
{
  if (!aSelection || !mRules) {
    return NS_ERROR_NULL_POINTER;
  }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

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
  ForceCompositionEnd();

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  nsCOMPtr<nsIDOMNode> anchorNode;
  nsresult rv = selection->GetAnchorNode(getter_AddRefs(anchorNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> anchorContent = do_QueryInterface(anchorNode, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIContent *rootContent;
  if (anchorContent->HasIndependentSelection()) {
    rv = selection->SetAncestorLimiter(nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
    rootContent = mRootElement;
  } else {
    nsCOMPtr<nsIPresShell> ps = GetPresShell();
    rootContent = anchorContent->GetSelectionRootContent(ps);
  }

  NS_ENSURE_TRUE(rootContent, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMNode> rootElement = do_QueryInterface(rootContent, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  Maybe<mozilla::dom::Selection::AutoUserInitiated> userSelection;
  if (!rootContent->IsEditable()) {
    userSelection.emplace(selection);
  }
  return selection->SelectAllChildren(rootElement);
}

// this will NOT find aAttribute unless aAttribute has a non-null value
// so singleton attributes like <Table border> will not be matched!
bool
HTMLEditor::IsTextPropertySetByContent(nsINode* aNode,
                                       nsIAtom* aProperty,
                                       const nsAString* aAttribute,
                                       const nsAString* aValue,
                                       nsAString* outValue)
{
  MOZ_ASSERT(aNode && aProperty);
  bool isSet;
  IsTextPropertySetByContent(aNode->AsDOMNode(), aProperty, aAttribute, aValue,
                             isSet, outValue);
  return isSet;
}

void
HTMLEditor::IsTextPropertySetByContent(nsIDOMNode* aNode,
                                       nsIAtom* aProperty,
                                       const nsAString* aAttribute,
                                       const nsAString* aValue,
                                       bool& aIsSet,
                                       nsAString* outValue)
{
  aIsSet = false;  // must be initialized to false for code below to work
  nsAutoString propName;
  aProperty->ToString(propName);
  nsCOMPtr<nsIDOMNode>node = aNode;

  while (node) {
    nsCOMPtr<nsIDOMElement>element;
    element = do_QueryInterface(node);
    if (element) {
      nsAutoString tag, value;
      element->GetTagName(tag);
      if (propName.Equals(tag, nsCaseInsensitiveStringComparator())) {
        bool found = false;
        if (aAttribute && !aAttribute->IsEmpty()) {
          element->GetAttribute(*aAttribute, value);
          if (outValue) {
            *outValue = value;
          }
          if (!value.IsEmpty()) {
            if (!aValue) {
              found = true;
            } else {
              nsString tString(*aValue);
              if (tString.Equals(value, nsCaseInsensitiveStringComparator())) {
                found = true;
              } else {
                // We found the prop with the attribute, but the value doesn't
                // match.
                break;
              }
            }
          }
        } else {
          found = true;
        }
        if (found) {
          aIsSet = true;
          break;
        }
      }
    }
    nsCOMPtr<nsIDOMNode>temp;
    if (NS_SUCCEEDED(node->GetParentNode(getter_AddRefs(temp))) && temp) {
      node = temp;
    } else {
      node = nullptr;
    }
  }
}

bool
HTMLEditor::SetCaretInTableCell(nsIDOMElement* aElement)
{
  nsCOMPtr<dom::Element> element = do_QueryInterface(aElement);
  if (!element || !element->IsHTMLElement() ||
      !HTMLEditUtils::IsTableElement(element) ||
      !IsDescendantOfEditorRoot(element)) {
    return false;
  }

  nsIContent* node = element;
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

nsIDOMNode*
HTMLEditor::GetEnclosingTable(nsIDOMNode* aNode)
{
  NS_PRECONDITION(aNode, "null node passed to HTMLEditor::GetEnclosingTable");
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, nullptr);
  nsCOMPtr<Element> table = GetEnclosingTable(node);
  nsCOMPtr<nsIDOMNode> ret = do_QueryInterface(table);
  return ret;
}


/**
 * This method scans the selection for adjacent text nodes
 * and collapses them into a single text node.
 * "adjacent" means literally adjacent siblings of the same parent.
 * Uses EditorBase::JoinNodes so action is undoable.
 * Should be called within the context of a batch transaction.
 */
nsresult
HTMLEditor::CollapseAdjacentTextNodes(nsRange* aInRange)
{
  NS_ENSURE_TRUE(aInRange, NS_ERROR_NULL_POINTER);
  AutoTransactionsConserveSelection dontSpazMySelection(this);
  nsTArray<nsCOMPtr<nsIDOMNode> > textNodes;
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
    if (node->NodeType() == nsIDOMNode::TEXT_NODE &&
        IsEditable(static_cast<nsIContent*>(node))) {
      nsCOMPtr<nsIDOMNode> domNode = do_QueryInterface(node);
      textNodes.AppendElement(domNode);
    }

    iter->Next();
  }

  // now that I have a list of text nodes, collapse adjacent text nodes
  // NOTE: assumption that JoinNodes keeps the righthand node
  while (textNodes.Length() > 1) {
    // we assume a textNodes entry can't be nullptr
    nsIDOMNode *leftTextNode = textNodes[0];
    nsIDOMNode *rightTextNode = textNodes[1];
    NS_ASSERTION(leftTextNode && rightTextNode,"left or rightTextNode null in CollapseAdjacentTextNodes");

    // get the prev sibling of the right node, and see if its leftTextNode
    nsCOMPtr<nsIDOMNode> prevSibOfRightNode;
    rv = rightTextNode->GetPreviousSibling(getter_AddRefs(prevSibOfRightNode));
    NS_ENSURE_SUCCESS(rv, rv);
    if (prevSibOfRightNode && prevSibOfRightNode == leftTextNode) {
      nsCOMPtr<nsIDOMNode> parent;
      rv = rightTextNode->GetParentNode(getter_AddRefs(parent));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
      rv = JoinNodes(leftTextNode, rightTextNode, parent);
      NS_ENSURE_SUCCESS(rv, rv);
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
HTMLEditor::RemoveBlockContainer(nsIContent& aNode)
{
  // Two possibilities: the container could be empty of editable content.  If
  // that is the case, we need to compare what is before and after aNode to
  // determine if we need a br.
  //
  // Or it could be not empty, in which case we have to compare previous
  // sibling and first child to determine if we need a leading br, and compare
  // following sibling and last child to determine if we need a trailing br.

  nsCOMPtr<nsIContent> child = GetFirstEditableChild(aNode);

  if (child) {
    // The case of aNode not being empty.  We need a br at start unless:
    // 1) previous sibling of aNode is a block, OR
    // 2) previous sibling of aNode is a br, OR
    // 3) first child of aNode is a block OR
    // 4) either is null

    nsCOMPtr<nsIContent> sibling = GetPriorHTMLSibling(&aNode);
    if (sibling && !IsBlockNode(sibling) &&
        !sibling->IsHTMLElement(nsGkAtoms::br) && !IsBlockNode(child)) {
      // Insert br node
      nsCOMPtr<Element> br = CreateBR(&aNode, 0);
      NS_ENSURE_STATE(br);
    }

    // We need a br at end unless:
    // 1) following sibling of aNode is a block, OR
    // 2) last child of aNode is a block, OR
    // 3) last child of aNode is a br OR
    // 4) either is null

    sibling = GetNextHTMLSibling(&aNode);
    if (sibling && !IsBlockNode(sibling)) {
      child = GetLastEditableChild(aNode);
      MOZ_ASSERT(child, "aNode has first editable child but not last?");
      if (!IsBlockNode(child) && !child->IsHTMLElement(nsGkAtoms::br)) {
        // Insert br node
        nsCOMPtr<Element> br = CreateBR(&aNode, aNode.Length());
        NS_ENSURE_STATE(br);
      }
    }
  } else {
    // The case of aNode being empty.  We need a br at start unless:
    // 1) previous sibling of aNode is a block, OR
    // 2) previous sibling of aNode is a br, OR
    // 3) following sibling of aNode is a block, OR
    // 4) following sibling of aNode is a br OR
    // 5) either is null
    nsCOMPtr<nsIContent> sibling = GetPriorHTMLSibling(&aNode);
    if (sibling && !IsBlockNode(sibling) &&
        !sibling->IsHTMLElement(nsGkAtoms::br)) {
      sibling = GetNextHTMLSibling(&aNode);
      if (sibling && !IsBlockNode(sibling) &&
          !sibling->IsHTMLElement(nsGkAtoms::br)) {
        // Insert br node
        nsCOMPtr<Element> br = CreateBR(&aNode, 0);
        NS_ENSURE_STATE(br);
      }
    }
  }

  // Now remove container
  nsresult rv = RemoveContainer(&aNode);
  NS_ENSURE_SUCCESS(rv, rv);

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

nsresult
HTMLEditor::GetPriorHTMLSibling(nsIDOMNode* inNode,
                                nsCOMPtr<nsIDOMNode>* outNode)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  *outNode = nullptr;

  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  *outNode = do_QueryInterface(GetPriorHTMLSibling(node));
  return NS_OK;
}

/**
 * GetPriorHTMLSibling() returns the previous editable sibling, if there is
 * one within the parent.  just like above routine but takes a parent/offset
 * instead of a node.
 */
nsIContent*
HTMLEditor::GetPriorHTMLSibling(nsINode* aParent,
                                int32_t aOffset)
{
  MOZ_ASSERT(aParent);

  nsIContent* node = aParent->GetChildAt(aOffset - 1);
  if (!node || IsEditable(node)) {
    return node;
  }

  return GetPriorHTMLSibling(node);
}

nsresult
HTMLEditor::GetPriorHTMLSibling(nsIDOMNode* inParent,
                                int32_t inOffset,
                                nsCOMPtr<nsIDOMNode>* outNode)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  *outNode = nullptr;

  nsCOMPtr<nsINode> parent = do_QueryInterface(inParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  *outNode = do_QueryInterface(GetPriorHTMLSibling(parent, inOffset));
  return NS_OK;
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

nsresult
HTMLEditor::GetNextHTMLSibling(nsIDOMNode* inNode,
                               nsCOMPtr<nsIDOMNode>* outNode)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  *outNode = nullptr;

  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  *outNode = do_QueryInterface(GetNextHTMLSibling(node));
  return NS_OK;
}

/**
 * GetNextHTMLSibling() returns the next editable sibling, if there is
 * one within the parent.  just like above routine but takes a parent/offset
 * instead of a node.
 */
nsIContent*
HTMLEditor::GetNextHTMLSibling(nsINode* aParent,
                               int32_t aOffset)
{
  MOZ_ASSERT(aParent);

  nsIContent* node = aParent->GetChildAt(aOffset + 1);
  if (!node || IsEditable(node)) {
    return node;
  }

  return GetNextHTMLSibling(node);
}

nsresult
HTMLEditor::GetNextHTMLSibling(nsIDOMNode* inParent,
                               int32_t inOffset,
                               nsCOMPtr<nsIDOMNode>* outNode)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  *outNode = nullptr;

  nsCOMPtr<nsINode> parent = do_QueryInterface(inParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  *outNode = do_QueryInterface(GetNextHTMLSibling(parent, inOffset));
  return NS_OK;
}

/**
 * GetPriorHTMLNode() returns the previous editable leaf node, if there is
 * one within the <body>.
 */
nsIContent*
HTMLEditor::GetPriorHTMLNode(nsINode* aNode,
                             bool aNoBlockCrossing)
{
  MOZ_ASSERT(aNode);

  if (!GetActiveEditingHost()) {
    return nullptr;
  }

  return GetPriorNode(aNode, true, aNoBlockCrossing);
}

nsresult
HTMLEditor::GetPriorHTMLNode(nsIDOMNode* aNode,
                             nsCOMPtr<nsIDOMNode>* aResultNode,
                             bool aNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetPriorHTMLNode(node, aNoBlockCrossing));
  return NS_OK;
}

/**
 * GetPriorHTMLNode() is same as above but takes {parent,offset} instead of
 * node.
 */
nsIContent*
HTMLEditor::GetPriorHTMLNode(nsINode* aParent,
                             int32_t aOffset,
                             bool aNoBlockCrossing)
{
  MOZ_ASSERT(aParent);

  if (!GetActiveEditingHost()) {
    return nullptr;
  }

  return GetPriorNode(aParent, aOffset, true, aNoBlockCrossing);
}

nsresult
HTMLEditor::GetPriorHTMLNode(nsIDOMNode* aNode,
                             int32_t aOffset,
                             nsCOMPtr<nsIDOMNode>* aResultNode,
                             bool aNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetPriorHTMLNode(node, aOffset,
                                                    aNoBlockCrossing));
  return NS_OK;
}

/**
 * GetNextHTMLNode() returns the next editable leaf node, if there is
 * one within the <body>.
 */
nsIContent*
HTMLEditor::GetNextHTMLNode(nsINode* aNode,
                            bool aNoBlockCrossing)
{
  MOZ_ASSERT(aNode);

  nsIContent* result = GetNextNode(aNode, true, aNoBlockCrossing);

  if (result && !IsDescendantOfEditorRoot(result)) {
    return nullptr;
  }

  return result;
}

nsresult
HTMLEditor::GetNextHTMLNode(nsIDOMNode* aNode,
                            nsCOMPtr<nsIDOMNode>* aResultNode,
                            bool aNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetNextHTMLNode(node, aNoBlockCrossing));
  return NS_OK;
}

/**
 * GetNextHTMLNode() is same as above but takes {parent,offset} instead of node.
 */
nsIContent*
HTMLEditor::GetNextHTMLNode(nsINode* aParent,
                            int32_t aOffset,
                            bool aNoBlockCrossing)
{
  nsIContent* content = GetNextNode(aParent, aOffset, true, aNoBlockCrossing);
  if (content && !IsDescendantOfEditorRoot(content)) {
    return nullptr;
  }
  return content;
}

nsresult
HTMLEditor::GetNextHTMLNode(nsIDOMNode* aNode,
                            int32_t aOffset,
                            nsCOMPtr<nsIDOMNode>* aResultNode,
                            bool aNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetNextHTMLNode(node, aOffset,
                                                   aNoBlockCrossing));
  return NS_OK;
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
    child = GetNextHTMLNode(child);

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
    child = GetPriorHTMLNode(child);

    // Only accept nodes that are descendants of aNode
    if (!aNode.Contains(child)) {
      return nullptr;
    }
  }

  return child;
}

/**
 * IsVisTextNode() figures out if textnode aTextNode has any visible content.
 */
nsresult
HTMLEditor::IsVisTextNode(nsIContent* aNode,
                          bool* outIsEmptyNode,
                          bool aSafeToAskFrames)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(aNode->NodeType() == nsIDOMNode::TEXT_NODE);
  MOZ_ASSERT(outIsEmptyNode);

  *outIsEmptyNode = true;

  uint32_t length = aNode->TextLength();
  if (aSafeToAskFrames) {
    nsCOMPtr<nsISelectionController> selectionController =
      GetSelectionController();
    if (NS_WARN_IF(!selectionController)) {
      return NS_ERROR_FAILURE;
    }
    bool isVisible = false;
    // ask the selection controller for information about whether any
    // of the data in the node is really rendered.  This is really
    // something that frames know about, but we aren't supposed to talk to frames.
    // So we put a call in the selection controller interface, since it's already
    // in bed with frames anyway.  (this is a fix for bug 22227, and a
    // partial fix for bug 46209)
    nsresult rv = selectionController->CheckVisibilityContent(aNode, 0, length,
                                                              &isVisible);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isVisible) {
      *outIsEmptyNode = false;
    }
  } else if (length) {
    if (aNode->TextIsOnlyWhitespace()) {
      WSRunObject wsRunObj(this, aNode, 0);
      nsCOMPtr<nsINode> visNode;
      int32_t outVisOffset=0;
      WSType visType;
      wsRunObj.NextVisibleNode(aNode, 0, address_of(visNode),
                               &outVisOffset, &visType);
      if (visType == WSType::normalWS || visType == WSType::text) {
        *outIsEmptyNode = (aNode != visNode);
      }
    } else {
      *outIsEmptyNode = false;
    }
  }
  return NS_OK;
}

/**
 * IsEmptyNode() figures out if aNode is an empty node.  A block can have
 * children and still be considered empty, if the children are empty or
 * non-editable.
 */
nsresult
HTMLEditor::IsEmptyNode(nsIDOMNode*aNode,
                        bool* outIsEmptyNode,
                        bool aSingleBRDoesntCount,
                        bool aListOrCellNotEmpty,
                        bool aSafeToAskFrames)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return IsEmptyNode(node, outIsEmptyNode, aSingleBRDoesntCount,
                     aListOrCellNotEmpty, aSafeToAskFrames);
}

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

  if (aNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    return IsVisTextNode(static_cast<nsIContent*>(aNode), outIsEmptyNode, aSafeToAskFrames);
  }

  // if it's not a text node (handled above) and it's not a container,
  // then we don't call it empty (it's an <hr>, or <br>, etc.).
  // Also, if it's an anchor then don't treat it as empty - even though
  // anchors are containers, named anchors are "empty" but we don't
  // want to treat them as such.  Also, don't call ListItems or table
  // cells empty if caller desires.  Form Widgets not empty.
  if (!IsContainer(aNode->AsDOMNode()) ||
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
      if (child->NodeType() == nsIDOMNode::TEXT_NODE) {
        nsresult rv = IsVisTextNode(child, outIsEmptyNode, aSafeToAskFrames);
        NS_ENSURE_SUCCESS(rv, rv);
        // break out if we find we aren't emtpy
        if (!*outIsEmptyNode) {
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
                                     nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     bool aSuppressTransaction)
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aAttribute);

  nsAutoScriptBlocker scriptBlocker;

  if (!IsCSSEnabled() || !mCSSEditUtils) {
    // we are not in an HTML+CSS editor; let's set the attribute the HTML way
    return aSuppressTransaction ?
             aElement->SetAttr(kNameSpaceID_None, aAttribute, aValue, true) :
             SetAttribute(aElement, aAttribute, aValue);
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
             RemoveAttribute(aElement, aAttribute);
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
      SetAttribute(aElement, aAttribute, existingValue);
  }

  // we have no CSS equivalence for this attribute and it is not the style
  // attribute; let's set it the good'n'old HTML way
  return aSuppressTransaction ?
           aElement->SetAttr(kNameSpaceID_None, aAttribute, aValue, true) :
           SetAttribute(aElement, aAttribute, aValue);
}

nsresult
HTMLEditor::RemoveAttributeOrEquivalent(Element* aElement,
                                        nsIAtom* aAttribute,
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
    RemoveAttribute(aElement, aAttribute);
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
HTMLEditor::SetCSSBackgroundColor(const nsAString& aColor)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);
  ForceCompositionEnd();

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  bool isCollapsed = selection->Collapsed();

  AutoEditBatch batchIt(this);
  AutoRules beginRulesSniffing(this, EditAction::insertElement,
                               nsIEditor::eNext);
  AutoSelectionRestorer selectionRestorer(selection, this);
  AutoTransactionsConserveSelection dontSpazMySelection(this);

  bool cancel, handled;
  TextRulesInfo ruleInfo(EditAction::setTextProperty);
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
        nsCOMPtr<nsIContent> selectedNode = startNode->GetChildAt(startOffset);
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
    return SetCSSBackgroundColor(aColor);
  }

  // but in HTML mode, we can only set the document's background color
  return SetHTMLBackgroundColor(aColor);
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

  // If CSS is enabled, we are stricter about span nodes.
  return mCSSEditUtils->ElementsSameStyle(aNode1->AsDOMNode(),
                                          aNode2->AsDOMNode());
}

nsresult
HTMLEditor::CopyLastEditableChildStyles(nsIDOMNode* aPreviousBlock,
                                        nsIDOMNode* aNewBlock,
                                        Element** aOutBrNode)
{
  nsCOMPtr<nsINode> newBlock = do_QueryInterface(aNewBlock);
  NS_ENSURE_STATE(newBlock || !aNewBlock);
  *aOutBrNode = nullptr;
  nsCOMPtr<nsIDOMNode> child, tmp;
  // first, clear out aNewBlock.  Contract is that we want only the styles from previousBlock.
  nsresult rv = aNewBlock->GetFirstChild(getter_AddRefs(child));
  while (NS_SUCCEEDED(rv) && child) {
    rv = DeleteNode(child);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aNewBlock->GetFirstChild(getter_AddRefs(child));
  }
  // now find and clone the styles
  child = aPreviousBlock;
  tmp = aPreviousBlock;
  while (tmp) {
    child = tmp;
    nsCOMPtr<nsINode> child_ = do_QueryInterface(child);
    NS_ENSURE_STATE(child_ || !child);
    tmp = GetAsDOMNode(GetLastEditableChild(*child_));
  }
  while (child && TextEditUtils::IsBreak(child)) {
    nsCOMPtr<nsIDOMNode> priorNode;
    rv = GetPriorHTMLNode(child, address_of(priorNode));
    NS_ENSURE_SUCCESS(rv, rv);
    child = priorNode;
  }
  nsCOMPtr<Element> newStyles, deepestStyle;
  nsCOMPtr<nsINode> childNode = do_QueryInterface(child);
  nsCOMPtr<Element> childElement;
  if (childNode) {
    childElement = childNode->IsElement() ? childNode->AsElement()
                                          : childNode->GetParentElement();
  }
  while (childElement && (childElement->AsDOMNode() != aPreviousBlock)) {
    if (HTMLEditUtils::IsInlineStyle(childElement) ||
        childElement->IsHTMLElement(nsGkAtoms::span)) {
      if (newStyles) {
        newStyles = InsertContainerAbove(newStyles,
                                         childElement->NodeInfo()->NameAtom());
        NS_ENSURE_STATE(newStyles);
      } else {
        deepestStyle = newStyles =
          CreateNode(childElement->NodeInfo()->NameAtom(), newBlock, 0);
        NS_ENSURE_STATE(newStyles);
      }
      CloneAttributes(newStyles, childElement);
    }
    childElement = childElement->GetParentElement();
  }
  if (deepestStyle) {
    RefPtr<Element> retVal = CreateBR(deepestStyle, 0);
    retVal.forget(aOutBrNode);
    NS_ENSURE_STATE(*aOutBrNode);
  }
  return NS_OK;
}

nsresult
HTMLEditor::GetElementOrigin(nsIDOMElement* aElement,
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

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsIFrame *frame = content->GetPrimaryFrame();
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
HTMLEditor::GetSelectionContainer(nsIDOMElement** aReturn)
{
  nsCOMPtr<nsIDOMElement> container =
    static_cast<nsIDOMElement*>(GetAsDOMNode(GetSelectionContainer()));
  NS_ENSURE_TRUE(container, NS_ERROR_FAILURE);
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

  if (selection->Collapsed()) {
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
        nsCOMPtr<nsIDOMElement> focusElement;
        nsresult rv = GetSelectedElement(EmptyString(),
                                         getter_AddRefs(focusElement));
        NS_ENSURE_SUCCESS(rv, nullptr);
        if (focusElement) {
          focusNode = do_QueryInterface(focusElement);
        }
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
HTMLEditor::IsAnonymousElement(nsIDOMElement* aElement,
                               bool* aReturn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  *aReturn = content->IsRootOfNativeAnonymousSubtree();
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

already_AddRefed<nsIContent>
HTMLEditor::GetFocusedContent()
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nullptr);

  nsCOMPtr<nsIContent> focusedContent = fm->GetFocusedContent();

  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }
  bool inDesignMode = document->HasFlag(NODE_IS_EDITABLE);
  if (!focusedContent) {
    // in designMode, nobody gets focus in most cases.
    if (inDesignMode && OurWindowHasFocus()) {
      nsCOMPtr<nsIContent> rootContent = document->GetRootElement();
      return rootContent.forget();
    }
    return nullptr;
  }

  if (inDesignMode) {
    return OurWindowHasFocus() &&
      nsContentUtils::ContentIsDescendantOf(focusedContent, document) ?
        focusedContent.forget() : nullptr;
  }

  // We're HTML editor for contenteditable

  // If the focused content isn't editable, or it has independent selection,
  // we don't have focus.
  if (!focusedContent->HasFlag(NODE_IS_EDITABLE) ||
      focusedContent->HasIndependentSelection()) {
    return nullptr;
  }
  // If our window is focused, we're focused.
  return OurWindowHasFocus() ? focusedContent.forget() : nullptr;
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
    nsFocusManager::GetFocusedDescendant(ourWindow, false,
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
  nsCOMPtr<nsIDOMNode> focusNode;
  nsresult rv = selection->GetFocusNode(getter_AddRefs(focusNode));
  NS_ENSURE_SUCCESS(rv, nullptr);
  nsCOMPtr<nsIContent> content = do_QueryInterface(focusNode);
  if (!content) {
    return nullptr;
  }

  // If the active content isn't editable, or it has independent selection,
  // we're not active.
  if (!content->HasFlag(NODE_IS_EDITABLE) ||
      content->HasIndependentSelection()) {
    return nullptr;
  }
  return content->GetEditingHost();
}

already_AddRefed<EventTarget>
HTMLEditor::GetDOMEventTarget()
{
  // Don't use getDocument here, because we have no way of knowing
  // whether Init() was ever called.  So we need to get the document
  // ourselves, if it exists.
  MOZ_ASSERT(IsInitialized(), "The HTMLEditor has not been initialized yet");
  nsCOMPtr<mozilla::dom::EventTarget> target = GetDocument();
  return target.forget();
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
  nsCOMPtr<nsIDOMHTMLElement> docBody;
  GetBodyElement(getter_AddRefs(docBody));
  return !SameCOMIdentity(docBody, mRootElement);
}

void
HTMLEditor::ResetRootElementAndEventTarget()
{
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  // Need to remove the event listeners first because BeginningOfDocument
  // could set a new root (and event target is set by InstallEventListeners())
  // and we won't be able to remove them from the old event target then.
  RemoveEventListeners();
  mRootElement = nullptr;
  nsresult rv = InstallEventListeners();
  if (NS_FAILED(rv)) {
    return;
  }

  // We must have mRootElement now.
  nsCOMPtr<nsIDOMElement> root;
  rv = GetRootElement(getter_AddRefs(root));
  if (NS_FAILED(rv) || !mRootElement) {
    return;
  }

  rv = BeginningOfDocument();
  if (NS_FAILED(rv)) {
    return;
  }

  // When this editor has focus, we need to reset the selection limiter to
  // new root.  Otherwise, that is going to be done when this gets focus.
  nsCOMPtr<nsINode> node = GetFocusedNode();
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(node);
  if (target) {
    InitializeSelection(target);
  }

  SyncRealTimeSpell();
}

nsresult
HTMLEditor::GetBodyElement(nsIDOMHTMLElement** aBody)
{
  MOZ_ASSERT(IsInitialized(), "The HTMLEditor hasn't been initialized yet");
  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsCOMPtr<nsIDOMHTMLDocument> domHTMLDocument = do_QueryInterface(document);
  if (!domHTMLDocument) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return domHTMLDocument->GetBody(aBody);
}

already_AddRefed<nsINode>
HTMLEditor::GetFocusedNode()
{
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (!focusedContent) {
    return nullptr;
  }

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ASSERTION(fm, "Focus manager is null");
  nsCOMPtr<nsIDOMElement> focusedElement;
  fm->GetFocusedElement(getter_AddRefs(focusedElement));
  if (focusedElement) {
    nsCOMPtr<nsINode> node = do_QueryInterface(focusedElement);
    return node.forget();
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

  nsCOMPtr<nsIDOMEventTarget> target = aGUIEvent->GetDOMEventTarget();
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

NS_IMETHODIMP
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

bool
HTMLEditor::IsEditable(nsINode* aNode)
{
  if (!TextEditor::IsEditable(aNode)) {
    return false;
  }
  if (aNode->IsElement()) {
    // If we're dealing with an element, then ask it whether it's editable.
    return aNode->IsEditable();
  }
  // We might be dealing with a text node for example, which we always consider
  // to be editable.
  return true;
}

Element*
HTMLEditor::GetEditorRoot()
{
  return GetActiveEditingHost();
}

} // namespace mozilla
