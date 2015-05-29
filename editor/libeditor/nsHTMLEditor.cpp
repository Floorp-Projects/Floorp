/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLEditor.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/EventStates.h"
#include "mozilla/TextEvents.h"

#include "nsCRT.h"

#include "nsUnicharUtils.h"

#include "nsHTMLEditRules.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"

#include "nsHTMLEditorEventListener.h"
#include "TypeInState.h"

#include "nsHTMLURIRefObject.h"

#include "nsIDOMText.h"
#include "nsIDOMMozNamedAttrMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDocumentInlines.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsISelectionController.h"
#include "nsIDOMHTMLDocument.h"
#include "nsILinkHandler.h"
#include "nsIInlineSpellChecker.h"

#include "mozilla/CSSStyleSheet.h"
#include "mozilla/css/Loader.h"
#include "nsIDOMStyleSheet.h"

#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsISupportsArray.h"
#include "nsContentUtils.h"
#include "nsIDocumentEncoder.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "SetDocTitleTxn.h"
#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"

// netwerk
#include "nsIURI.h"
#include "nsNetUtil.h"

// Transactionas
#include "nsStyleSheetTxns.h"

// Misc
#include "TextEditorTest.h"
#include "nsEditorUtils.h"
#include "nsWSRunObject.h"
#include "nsGkAtoms.h"
#include "nsIWidget.h"

#include "nsIFrame.h"
#include "nsIParserService.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "nsTextFragment.h"
#include "nsContentList.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::widget;

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

nsHTMLEditor::nsHTMLEditor()
: nsPlaintextEditor()
, mCRInParagraphCreatesParagraph(false)
, mSelectedCellIndex(0)
, mIsObjectResizingEnabled(true)
, mIsResizing(false)
, mIsAbsolutelyPositioningEnabled(true)
, mResizedObjectIsAbsolutelyPositioned(false)
, mGrabberClicked(false)
, mIsMoving(false)
, mSnapToGridEnabled(false)
, mIsInlineTableEditingEnabled(true)
, mInfoXIncrement(20)
, mInfoYIncrement(20)
, mGridSize(0)
{
}

nsHTMLEditor::~nsHTMLEditor()
{
  // remove the rules as an action listener.  Else we get a bad
  // ownership loop later on.  it's ok if the rules aren't a listener;
  // we ignore the error.
  nsCOMPtr<nsIEditActionListener> mListener = do_QueryInterface(mRules);
  RemoveEditActionListener(mListener);

  //the autopointers will clear themselves up.
  //but we need to also remove the listeners or we have a leak
  nsRefPtr<Selection> selection = GetSelection();
  // if we don't get the selection, just skip this
  if (selection) {
    nsCOMPtr<nsISelectionListener>listener;
    listener = do_QueryInterface(mTypeInState);
    if (listener)
    {
      selection->RemoveSelectionListener(listener);
    }
    listener = do_QueryInterface(mSelectionListenerP);
    if (listener)
    {
      selection->RemoveSelectionListener(listener);
    }
  }

  mTypeInState = nullptr;
  mSelectionListenerP = nullptr;

  // free any default style propItems
  RemoveAllDefaultProperties();

  if (mLinkHandler && mDocWeak)
  {
    nsCOMPtr<nsIPresShell> ps = GetPresShell();

    if (ps && ps->GetPresContext())
    {
      ps->GetPresContext()->SetLinkHandler(mLinkHandler);
    }
  }

  RemoveEventListeners();
}

void
nsHTMLEditor::HideAnonymousEditingUIs()
{
  if (mAbsolutelyPositionedObject)
    HideGrabber();
  if (mInlineEditedCell)
    HideInlineTableEditingUI();
  if (mResizedObject)
    HideResizers();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLEditor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLEditor, nsPlaintextEditor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTypeInState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStyleSheets)

  tmp->HideAnonymousEditingUIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLEditor, nsPlaintextEditor)
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObjectResizeEventListeners)

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

NS_IMPL_ADDREF_INHERITED(nsHTMLEditor, nsEditor)
NS_IMPL_RELEASE_INHERITED(nsHTMLEditor, nsEditor)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsHTMLEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLObjectResizer)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLAbsPosEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLInlineTableEditor)
  NS_INTERFACE_MAP_ENTRY(nsITableEditor)
  NS_INTERFACE_MAP_ENTRY(nsIEditorStyleSheets)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END_INHERITING(nsPlaintextEditor)


NS_IMETHODIMP
nsHTMLEditor::Init(nsIDOMDocument *aDoc,
                   nsIContent *aRoot,
                   nsISelectionController *aSelCon,
                   uint32_t aFlags,
                   const nsAString& aInitialValue)
{
  NS_PRECONDITION(aDoc && !aSelCon, "bad arg");
  NS_ENSURE_TRUE(aDoc, NS_ERROR_NULL_POINTER);
  MOZ_ASSERT(aInitialValue.IsEmpty(), "Non-empty initial values not supported");

  nsresult result = NS_OK, rulesRes = NS_OK;

  if (1)
  {
    // block to scope nsAutoEditInitRulesTrigger
    nsAutoEditInitRulesTrigger rulesTrigger(static_cast<nsPlaintextEditor*>(this), rulesRes);

    // Init the plaintext editor
    result = nsPlaintextEditor::Init(aDoc, aRoot, nullptr, aFlags, aInitialValue);
    if (NS_FAILED(result)) { return result; }

    // Init mutation observer
    nsCOMPtr<nsINode> document = do_QueryInterface(aDoc);
    document->AddMutationObserverUnlessExists(this);

    // disable Composer-only features
    if (IsMailEditor())
    {
      SetAbsolutePositioningEnabled(false);
      SetSnapToGridEnabled(false);
    }

    // Init the HTML-CSS utils
    mHTMLCSSUtils = new nsHTMLCSSUtils(this);

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
    mSelectionListenerP = new ResizerSelectionListener(this);

    if (!IsInteractionAllowed()) {
      // ignore any errors from this in case the file is missing
      AddOverrideStyleSheet(NS_LITERAL_STRING("resource://gre/res/EditorOverride.css"));
    }

    nsRefPtr<Selection> selection = GetSelection();
    if (selection)
    {
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

  NS_ENSURE_SUCCESS(rulesRes, rulesRes);
  return result;
}

NS_IMETHODIMP
nsHTMLEditor::PreDestroy(bool aDestroyingFrames)
{
  if (mDidPreDestroy) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> document = do_QueryReferent(mDocWeak);
  if (document) {
    document->RemoveMutationObserver(this);
  }

  while (mStyleSheetURLs.Length())
  {
    RemoveOverrideStyleSheet(mStyleSheetURLs[0]);
  }

  // Clean up after our anonymous content -- we don't want these nodes to
  // stay around (which they would, since the frames have an owning reference).
  HideAnonymousEditingUIs();

  return nsPlaintextEditor::PreDestroy(aDestroyingFrames);
}

NS_IMETHODIMP
nsHTMLEditor::GetRootElement(nsIDOMElement **aRootElement)
{
  NS_ENSURE_ARG_POINTER(aRootElement);

  if (mRootElement) {
    return nsEditor::GetRootElement(aRootElement);
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
    nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
    NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

    rv = doc->GetDocumentElement(getter_AddRefs(rootElement));
    NS_ENSURE_SUCCESS(rv, rv);
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
nsHTMLEditor::FindSelectionRoot(nsINode *aNode)
{
  NS_PRECONDITION(aNode->IsNodeOfType(nsINode::eDOCUMENT) ||
                  aNode->IsNodeOfType(nsINode::eCONTENT),
                  "aNode must be content or document node");

  nsCOMPtr<nsIDocument> doc = aNode->GetCurrentDoc();
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

/* virtual */
void
nsHTMLEditor::CreateEventListeners()
{
  // Don't create the handler twice
  if (!mEventListener) {
    mEventListener = new nsHTMLEditorEventListener();
  }
}

nsresult
nsHTMLEditor::InstallEventListeners()
{
  NS_ENSURE_TRUE(mDocWeak && mEventListener,
                 NS_ERROR_NOT_INITIALIZED);

  // NOTE: nsHTMLEditor doesn't need to initialize mEventTarget here because
  // the target must be document node and it must be referenced as weak pointer.

  nsHTMLEditorEventListener* listener =
    reinterpret_cast<nsHTMLEditorEventListener*>(mEventListener.get());
  return listener->Connect(this);
}

void
nsHTMLEditor::RemoveEventListeners()
{
  if (!mDocWeak)
  {
    return;
  }

  nsCOMPtr<nsIDOMEventTarget> target = GetDOMEventTarget();

  if (target)
  {
    // Both mMouseMotionListenerP and mResizeEventListenerP can be
    // registerd with other targets than the DOM event receiver that
    // we can reach from here. But nonetheless, unregister the event
    // listeners with the DOM event reveiver (if it's registerd with
    // other targets, it'll get unregisterd once the target goes
    // away).

    if (mMouseMotionListenerP)
    {
      // mMouseMotionListenerP might be registerd either as bubbling or
      // capturing, unregister by both.
      target->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                  mMouseMotionListenerP, false);
      target->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                  mMouseMotionListenerP, true);
    }

    if (mResizeEventListenerP)
    {
      target->RemoveEventListener(NS_LITERAL_STRING("resize"),
                                  mResizeEventListenerP, false);
    }
  }

  mMouseMotionListenerP = nullptr;
  mResizeEventListenerP = nullptr;

  nsPlaintextEditor::RemoveEventListeners();
}

NS_IMETHODIMP
nsHTMLEditor::SetFlags(uint32_t aFlags)
{
  nsresult rv = nsPlaintextEditor::SetFlags(aFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // Sets mCSSAware to correspond to aFlags. This toggles whether CSS is
  // used to style elements in the editor. Note that the editor is only CSS
  // aware by default in Composer and in the mail editor.
  mCSSAware = !NoCSS() && !IsMailEditor();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::InitRules()
{
  if (!mRules) {
    // instantiate the rules for the html editor
    mRules = new nsHTMLEditRules();
  }
  return mRules->Init(static_cast<nsPlaintextEditor*>(this));
}

NS_IMETHODIMP
nsHTMLEditor::BeginningOfDocument()
{
  if (!mDocWeak) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Get the selection
  nsRefPtr<Selection> selection = GetSelection();
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
  int32_t curOffset = 0, selOffset;
  while (!done) {
    nsWSRunObject wsObj(this, curNode, curOffset);
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
      // By definition of nsWSRunObject, a block element terminates a
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
nsHTMLEditor::HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent)
{
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/tests/test_htmleditor_keyevent_handling.html

  if (IsReadonly() || IsDisabled()) {
    // When we're not editable, the events are handled on nsEditor, so, we can
    // bypass nsPlaintextEditor.
    return nsEditor::HandleKeyPressEvent(aKeyEvent);
  }

  WidgetKeyboardEvent* nativeKeyEvent =
    aKeyEvent->GetInternalNSEvent()->AsKeyboardEvent();
  NS_ENSURE_TRUE(nativeKeyEvent, NS_ERROR_UNEXPECTED);
  NS_ASSERTION(nativeKeyEvent->message == NS_KEY_PRESS,
               "HandleKeyPressEvent gets non-keypress event");

  switch (nativeKeyEvent->keyCode) {
    case nsIDOMKeyEvent::DOM_VK_META:
    case nsIDOMKeyEvent::DOM_VK_WIN:
    case nsIDOMKeyEvent::DOM_VK_SHIFT:
    case nsIDOMKeyEvent::DOM_VK_CONTROL:
    case nsIDOMKeyEvent::DOM_VK_ALT:
    case nsIDOMKeyEvent::DOM_VK_BACK_SPACE:
    case nsIDOMKeyEvent::DOM_VK_DELETE:
      // These keys are handled on nsEditor, so, we can bypass
      // nsPlaintextEditor.
      return nsEditor::HandleKeyPressEvent(aKeyEvent);
    case nsIDOMKeyEvent::DOM_VK_TAB: {
      if (IsPlaintextEditor()) {
        // If this works as plain text editor, e.g., mail editor for plain
        // text, should be handled on nsPlaintextEditor.
        return nsPlaintextEditor::HandleKeyPressEvent(aKeyEvent);
      }

      if (IsTabbable()) {
        return NS_OK; // let it be used for focus switching
      }

      if (nativeKeyEvent->IsControl() || nativeKeyEvent->IsAlt() ||
          nativeKeyEvent->IsMeta() || nativeKeyEvent->IsOS()) {
        return NS_OK;
      }

      nsRefPtr<Selection> selection = GetSelection();
      NS_ENSURE_TRUE(selection && selection->RangeCount(), NS_ERROR_FAILURE);

      nsCOMPtr<nsINode> node = selection->GetRangeAt(0)->GetStartParent();
      MOZ_ASSERT(node);

      nsCOMPtr<nsINode> blockParent;
      if (IsBlockNode(node)) {
        blockParent = node;
      } else {
        blockParent = GetBlockNodeParent(node);
      }

      if (!blockParent) {
        break;
      }

      bool handled = false;
      nsresult rv = NS_OK;
      if (nsHTMLEditUtils::IsTableElement(blockParent)) {
        rv = TabInTable(nativeKeyEvent->IsShift(), &handled);
        if (handled) {
          ScrollSelectionIntoView(false);
        }
      } else if (nsHTMLEditUtils::IsListItem(blockParent)) {
        rv = Indent(nativeKeyEvent->IsShift()
                    ? NS_LITERAL_STRING("outdent")
                    : NS_LITERAL_STRING("indent"));
        handled = true;
      }
      NS_ENSURE_SUCCESS(rv, rv);
      if (handled) {
        return aKeyEvent->PreventDefault(); // consumed
      }
      if (nativeKeyEvent->IsShift()) {
        return NS_OK; // don't type text for shift tabs
      }
      aKeyEvent->PreventDefault();
      return TypedText(NS_LITERAL_STRING("\t"), eTypedText);
    }
    case nsIDOMKeyEvent::DOM_VK_RETURN:
      if (nativeKeyEvent->IsControl() || nativeKeyEvent->IsAlt() ||
          nativeKeyEvent->IsMeta() || nativeKeyEvent->IsOS()) {
        return NS_OK;
      }
      aKeyEvent->PreventDefault(); // consumed
      if (nativeKeyEvent->IsShift() && !IsPlaintextEditor()) {
        // only inserts a br node
        return TypedText(EmptyString(), eTypedBR);
      }
      // uses rules to figure out what to insert
      return TypedText(EmptyString(), eTypedBreak);
  }

  // NOTE: On some keyboard layout, some characters are inputted with Control
  // key or Alt key, but at that time, widget sets FALSE to these keys.
  if (nativeKeyEvent->charCode == 0 || nativeKeyEvent->IsControl() ||
      nativeKeyEvent->IsAlt() || nativeKeyEvent->IsMeta() ||
      nativeKeyEvent->IsOS()) {
    // we don't PreventDefault() here or keybindings like control-x won't work
    return NS_OK;
  }
  aKeyEvent->PreventDefault();
  nsAutoString str(nativeKeyEvent->charCode);
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
nsHTMLEditor::NodeIsBlockStatic(const nsINode* aElement)
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
nsHTMLEditor::NodeIsBlockStatic(nsIDOMNode *aNode, bool *aIsBlock)
{
  if (!aNode || !aIsBlock) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<dom::Element> element = do_QueryInterface(aNode);
  *aIsBlock = element && NodeIsBlockStatic(element);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::NodeIsBlock(nsIDOMNode *aNode, bool *aIsBlock)
{
  return NodeIsBlockStatic(aNode, aIsBlock);
}

bool
nsHTMLEditor::IsBlockNode(nsINode *aNode)
{
  return aNode && NodeIsBlockStatic(aNode);
}

// Non-static version for the nsIEditor interface and JavaScript
NS_IMETHODIMP
nsHTMLEditor::SetDocumentTitle(const nsAString &aTitle)
{
  nsRefPtr<SetDocTitleTxn> txn = new SetDocTitleTxn();
  NS_ENSURE_TRUE(txn, NS_ERROR_OUT_OF_MEMORY);

  nsresult result = txn->Init(this, &aTitle);
  NS_ENSURE_SUCCESS(result, result);

  //Don't let Rules System change the selection
  nsAutoTxnsConserveSelection dontChangeSelection(this);
  return nsEditor::DoTransaction(txn);
}

/* ------------ Block methods moved from nsEditor -------------- */
///////////////////////////////////////////////////////////////////////////
// GetBlockNodeParent: returns enclosing block level ancestor, if any
//
already_AddRefed<Element>
nsHTMLEditor::GetBlockNodeParent(nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  nsCOMPtr<nsINode> p = aNode->GetParentNode();

  while (p) {
    if (NodeIsBlockStatic(p)) {
      return p.forget().downcast<Element>();
    }
    p = p->GetParentNode();
  }

  return nullptr;
}

already_AddRefed<nsIDOMNode>
nsHTMLEditor::GetBlockNodeParent(nsIDOMNode *aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);

  if (!node) {
    NS_NOTREACHED("null node passed to GetBlockNodeParent()");
    return nullptr;
  }

  nsCOMPtr<nsIDOMNode> ret =
    dont_AddRef(GetAsDOMNode(GetBlockNodeParent(node).take()));
  return ret.forget();
}

static const char16_t nbsp = 160;

///////////////////////////////////////////////////////////////////////////////
// IsNextCharInNodeWhitespace: checks the adjacent content in the same node to
//                             see if following selection is whitespace or nbsp
void
nsHTMLEditor::IsNextCharInNodeWhitespace(nsIContent* aContent,
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
    *outIsNBSP = (ch == nbsp);
    if (outNode && outOffset) {
      NS_IF_ADDREF(*outNode = aContent);
      // yes, this is _past_ the character
      *outOffset = aOffset + 1;
    }
  }
}


///////////////////////////////////////////////////////////////////////////////
// IsPrevCharInNodeWhitespace: checks the adjacent content in the same node to
//                             see if following selection is whitespace
void
nsHTMLEditor::IsPrevCharInNodeWhitespace(nsIContent* aContent,
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
    *outIsNBSP = (ch == nbsp);
    if (outNode && outOffset) {
      NS_IF_ADDREF(*outNode = aContent);
      *outOffset = aOffset - 1;
    }
  }
}



/* ------------ End Block methods -------------- */


bool
nsHTMLEditor::IsVisBreak(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  if (!nsTextEditUtils::IsBreak(aNode)) {
    return false;
  }
  // Check if there is a later node in block after br
  nsCOMPtr<nsINode> priorNode = GetPriorHTMLNode(aNode, true);
  if (priorNode && nsTextEditUtils::IsBreak(priorNode)) {
    return true;
  }
  nsCOMPtr<nsINode> nextNode = GetNextHTMLNode(aNode, true);
  if (nextNode && nsTextEditUtils::IsBreak(nextNode)) {
    return true;
  }

  // If we are right before block boundary, then br not visible
  if (!nextNode) {
    // This break is trailer in block, it's not visible
    return false;
  }
  if (IsBlockNode(nextNode)) {
    // Break is right before a block, it's not visible
    return false;
  }

  // Sigh.  We have to use expensive whitespace calculation code to
  // determine what is going on
  int32_t selOffset;
  nsCOMPtr<nsINode> selNode = GetNodeLocation(aNode, &selOffset);
  // Let's look after the break
  selOffset++;
  nsWSRunObject wsObj(this, selNode, selOffset);
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

bool
nsHTMLEditor::IsVisBreak(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, false);
  return IsVisBreak(node);
}

NS_IMETHODIMP
nsHTMLEditor::GetIsDocumentEditable(bool *aIsDocumentEditable)
{
  NS_ENSURE_ARG_POINTER(aIsDocumentEditable);

  nsCOMPtr<nsIDOMDocument> doc = GetDOMDocument();
  *aIsDocumentEditable = doc && IsModifiable();

  return NS_OK;
}

bool nsHTMLEditor::IsModifiable()
{
  return !IsReadonly();
}

NS_IMETHODIMP
nsHTMLEditor::UpdateBaseURL()
{
  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // Look for an HTML <base> tag
  nsRefPtr<nsContentList> nodeList =
    doc->GetElementsByTagName(NS_LITERAL_STRING("base"));

  // If no base tag, then set baseURL to the document's URL.  This is very
  // important, else relative URLs for links and images are wrong
  if (!nodeList || !nodeList->Item(0)) {
    return doc->SetBaseURI(doc->GetDocumentURI());
  }
  return NS_OK;
}

/* This routine is needed to provide a bottleneck for typing for logging
   purposes.  Can't use HandleKeyPress() (above) for that since it takes
   a nsIDOMKeyEvent* parameter.  So instead we pass enough info through
   to TypedText() to determine what action to take, but without passing
   an event.
   */
NS_IMETHODIMP
nsHTMLEditor::TypedText(const nsAString& aString, ETypingAction aAction)
{
  nsAutoPlaceHolderBatch batch(this, nsGkAtoms::TypingTxnName);

  if (aAction == eTypedBR) {
    // only inserts a br node
    nsCOMPtr<nsIDOMNode> brNode;
    return InsertBR(address_of(brNode));
  }

  return nsPlaintextEditor::TypedText(aString, aAction);
}

NS_IMETHODIMP
nsHTMLEditor::TabInTable(bool inIsShift, bool* outHandled)
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
  nsresult res = iter->Init(table);
  NS_ENSURE_SUCCESS(res, res);
  // position iter at block
  res = iter->PositionAt(cellElement);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsINode> node;
  do {
    if (inIsShift) {
      iter->Prev();
    } else {
      iter->Next();
    }

    node = iter->GetCurrentNode();

    if (node && nsHTMLEditUtils::IsTableCell(node) &&
        nsCOMPtr<Element>(GetEnclosingTable(node)) == table) {
      CollapseSelectionToDeepestNonTableFirstChild(nullptr, node);
      *outHandled = true;
      return NS_OK;
    }
  } while (!iter->IsDone());

  if (!(*outHandled) && !inIsShift) {
    // If we haven't handled it yet, then we must have run off the end of the
    // table.  Insert a new row.
    res = InsertTableRow(1, true);
    NS_ENSURE_SUCCESS(res, res);
    *outHandled = true;
    // Put selection in right place.  Use table code to get selection and index
    // to new row...
    nsRefPtr<Selection> selection;
    nsCOMPtr<nsIDOMElement> tblElement, cell;
    int32_t row;
    res = GetCellContext(getter_AddRefs(selection),
                         getter_AddRefs(tblElement),
                         getter_AddRefs(cell),
                         nullptr, nullptr,
                         &row, nullptr);
    NS_ENSURE_SUCCESS(res, res);
    // ...so that we can ask for first cell in that row...
    res = GetCellAt(tblElement, row, 0, getter_AddRefs(cell));
    NS_ENSURE_SUCCESS(res, res);
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
nsHTMLEditor::CreateBR(nsINode* aNode, int32_t aOffset, EDirection aSelect)
{
  nsCOMPtr<nsIDOMNode> parent = GetAsDOMNode(aNode);
  int32_t offset = aOffset;
  nsCOMPtr<nsIDOMNode> outBRNode;
  // We assume everything is fine if the br is not null, irrespective of retval
  CreateBRImpl(address_of(parent), &offset, address_of(outBRNode), aSelect);
  nsCOMPtr<Element> ret = do_QueryInterface(outBRNode);
  return ret.forget();
}

NS_IMETHODIMP nsHTMLEditor::CreateBR(nsIDOMNode *aNode, int32_t aOffset, nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect)
{
  nsCOMPtr<nsIDOMNode> parent = aNode;
  int32_t offset = aOffset;
  return CreateBRImpl(address_of(parent), &offset, outBRNode, aSelect);
}

void
nsHTMLEditor::CollapseSelectionToDeepestNonTableFirstChild(
                                         Selection* aSelection, nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  nsRefPtr<Selection> selection = aSelection;
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
    if (nsHTMLEditUtils::IsTable(child) || !IsContainer(child)) {
      break;
    }
    node = child;
  };

  selection->Collapse(node, 0);
}


/**
 * This is mostly like InsertHTMLWithCharsetAndContext, but we can't use that
 * because it is selection-based and the rules code won't let us edit under the
 * <head> node
 */
NS_IMETHODIMP
nsHTMLEditor::ReplaceHeadContentsWithHTML(const nsAString& aSourceToInsert)
{
  // don't do any post processing, rules get confused
  nsAutoRules beginRulesSniffing(this, EditAction::ignore, nsIEditor::eNone);
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  ForceCompositionEnd();

  // Do not use nsAutoRules -- rules code won't let us insert in <head>.  Use
  // the head node as a parent and delete/insert directly.
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

  nsRefPtr<nsContentList> nodeList =
    doc->GetElementsByTagName(NS_LITERAL_STRING("head"));
  NS_ENSURE_TRUE(nodeList, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> headNode = nodeList->Item(0);
  NS_ENSURE_TRUE(headNode, NS_ERROR_NULL_POINTER);

  // First, make sure there are no return chars in the source.  Bad things
  // happen if you insert returns (instead of dom newlines, \n) into an editor
  // document.
  nsAutoString inputString (aSourceToInsert);  // hope this does copy-on-write

  // Windows linebreaks: Map CRLF to LF:
  inputString.ReplaceSubstring(MOZ_UTF16("\r\n"),
                               MOZ_UTF16("\n"));

  // Mac linebreaks: Map any remaining CR to LF:
  inputString.ReplaceSubstring(MOZ_UTF16("\r"),
                               MOZ_UTF16("\n"));

  nsAutoEditBatch beginBatching(this);

  // Get the first range in the selection, for context:
  nsRefPtr<nsRange> range = selection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

  ErrorResult err;
  nsRefPtr<DocumentFragment> docfrag =
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
    nsresult res = DeleteNode(child);
    NS_ENSURE_SUCCESS(res, res);
  }

  // Now insert the new nodes
  int32_t offsetOfNewNode = 0;

  // Loop over the contents of the fragment and move into the document
  while (nsCOMPtr<nsIContent> child = docfrag->GetFirstChild()) {
    nsresult res = InsertNode(*child, *headNode, offsetOfNewNode++);
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::RebuildDocumentFromSource(const nsAString& aSourceString)
{
  ForceCompositionEnd();

  nsRefPtr<Selection> selection = GetSelection();
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
  nsAutoEditBatch beginBatching(this);

  nsReadingIterator<char16_t> endtotal;
  aSourceString.EndReading(endtotal);

  nsresult res;
  if (foundhead) {
    if (foundclosehead) {
      res = ReplaceHeadContentsWithHTML(Substring(beginhead, beginclosehead));
    } else if (foundbody) {
      res = ReplaceHeadContentsWithHTML(Substring(beginhead, beginbody));
    } else {
      // XXX Without recourse to some parser/content sink/docshell hackery we
      // don't really know where the head ends and the body begins so we assume
      // that there is no body
      res = ReplaceHeadContentsWithHTML(Substring(beginhead, endtotal));
    }
  } else {
    nsReadingIterator<char16_t> begintotal;
    aSourceString.BeginReading(begintotal);
    NS_NAMED_LITERAL_STRING(head, "<head>");
    if (foundclosehead) {
      res = ReplaceHeadContentsWithHTML(head + Substring(begintotal,
                                                         beginclosehead));
    } else if (foundbody) {
      res = ReplaceHeadContentsWithHTML(head + Substring(begintotal,
                                                         beginbody));
    } else {
      // XXX Without recourse to some parser/content sink/docshell hackery we
      // don't really know where the head ends and the body begins so we assume
      // that there is no head
      res = ReplaceHeadContentsWithHTML(head);
    }
  }
  NS_ENSURE_SUCCESS(res, res);

  res = SelectAll();
  NS_ENSURE_SUCCESS(res, res);

  if (!foundbody) {
    NS_NAMED_LITERAL_STRING(body, "<body>");
    // XXX Without recourse to some parser/content sink/docshell hackery we
    // don't really know where the head ends and the body begins
    if (foundclosehead) {
      // assume body starts after the head ends
      res = LoadHTML(body + Substring(endclosehead, endtotal));
    } else if (foundhead) {
      // assume there is no body
      res = LoadHTML(body);
    } else {
      // assume there is no head, the entire source is body
      res = LoadHTML(body + aSourceString);
    }
    NS_ENSURE_SUCCESS(res, res);

    nsCOMPtr<Element> divElement =
      CreateElementWithDefaults(NS_LITERAL_STRING("div"));
    NS_ENSURE_TRUE(divElement, NS_ERROR_FAILURE);

    CloneAttributes(bodyElement, divElement);

    return BeginningOfDocument();
  }

  res = LoadHTML(Substring(beginbody, endtotal));
  NS_ENSURE_SUCCESS(res, res);

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

  nsRefPtr<nsRange> range = selection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  ErrorResult rv;
  nsRefPtr<DocumentFragment> docfrag =
    range->CreateContextualFragment(bodyTag, rv);
  NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
  NS_ENSURE_TRUE(docfrag, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> child = docfrag->GetFirstChild();
  NS_ENSURE_TRUE(child && child->IsElement(), NS_ERROR_NULL_POINTER);

  // Copy all attributes from the div child to current body element
  CloneAttributes(bodyElement, child->AsElement());

  // place selection at first editable content
  return BeginningOfDocument();
}

void
nsHTMLEditor::NormalizeEOLInsertPosition(nsIDOMNode *firstNodeToInsert,
                                     nsCOMPtr<nsIDOMNode> *insertParentNode,
                                     int32_t *insertOffset)
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

  if (!IsBlockNode(firstNodeToInsert))
    return;

  nsWSRunObject wsObj(this, *insertParentNode, *insertOffset);
  nsCOMPtr<nsINode> nextVisNode, prevVisNode;
  int32_t nextVisOffset=0;
  WSType nextVisType;
  int32_t prevVisOffset=0;
  WSType prevVisType;

  nsCOMPtr<nsINode> parent(do_QueryInterface(*insertParentNode));
  wsObj.NextVisibleNode(parent, *insertOffset, address_of(nextVisNode), &nextVisOffset, &nextVisType);
  if (!nextVisNode)
    return;

  if (!(nextVisType & WSType::br)) {
    return;
  }

  wsObj.PriorVisibleNode(parent, *insertOffset, address_of(prevVisNode), &prevVisOffset, &prevVisType);
  if (!prevVisNode)
    return;

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
nsHTMLEditor::InsertElementAtSelection(nsIDOMElement* aElement, bool aDeleteSelection)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  nsresult res = NS_ERROR_NOT_INITIALIZED;

  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);

  ForceCompositionEnd();
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::insertElement, nsIEditor::eNext);

  nsRefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  // hand off to the rules system, see if it has anything to say about this
  bool cancel, handled;
  nsTextRulesInfo ruleInfo(EditAction::insertElement);
  ruleInfo.insertElement = aElement;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    if (aDeleteSelection)
    {
      if (!IsBlockNode(aElement)) {
        // E.g., inserting an image.  In this case we don't need to delete any
        // inline wrappers before we do the insertion.  Otherwise we let
        // DeleteSelectionAndPrepareToCreateNode do the deletion for us, which
        // calls DeleteSelection with aStripWrappers = eStrip.
        res = DeleteSelection(nsIEditor::eNone, nsIEditor::eNoStrip);
        NS_ENSURE_SUCCESS(res, res);
      }

      nsresult result = DeleteSelectionAndPrepareToCreateNode();
      NS_ENSURE_SUCCESS(result, result);
    }

    // If deleting, selection will be collapsed.
    // so if not, we collapse it
    if (!aDeleteSelection)
    {
      // Named Anchor is a special case,
      // We collapse to insert element BEFORE the selection
      // For all other tags, we insert AFTER the selection
      if (nsHTMLEditUtils::IsNamedAnchor(node))
      {
        selection->CollapseToStart();
      } else {
        selection->CollapseToEnd();
      }
    }

    nsCOMPtr<nsIDOMNode> parentSelectedNode;
    int32_t offsetForInsert;
    res = selection->GetAnchorNode(getter_AddRefs(parentSelectedNode));
    // XXX: ERROR_HANDLING bad XPCOM usage
    if (NS_SUCCEEDED(res) && NS_SUCCEEDED(selection->GetAnchorOffset(&offsetForInsert)) && parentSelectedNode)
    {
      // Adjust position based on the node we are going to insert.
      NormalizeEOLInsertPosition(node, address_of(parentSelectedNode), &offsetForInsert);

      res = InsertNodeAtPoint(node, address_of(parentSelectedNode), &offsetForInsert, false);
      NS_ENSURE_SUCCESS(res, res);
      // Set caret after element, but check for special case
      //  of inserting table-related elements: set in first cell instead
      if (!SetCaretInTableCell(aElement))
      {
        res = SetCaretAfterElement(aElement);
        NS_ENSURE_SUCCESS(res, res);
      }
      // check for inserting a whole table at the end of a block. If so insert a br after it.
      if (nsHTMLEditUtils::IsTable(node))
      {
        bool isLast;
        res = IsLastEditableChild(node, &isLast);
        NS_ENSURE_SUCCESS(res, res);
        if (isLast)
        {
          nsCOMPtr<nsIDOMNode> brNode;
          res = CreateBR(parentSelectedNode, offsetForInsert+1, address_of(brNode));
          NS_ENSURE_SUCCESS(res, res);
          selection->Collapse(parentSelectedNode, offsetForInsert+1);
        }
      }
    }
  }
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}


/*
  InsertNodeAtPoint: attempts to insert aNode into the document, at a point specified by
      {*ioParent,*ioOffset}.  Checks with strict dtd to see if containment is allowed.  If not
      allowed, will attempt to find a parent in the parent hierarchy of *ioParent that will
      accept aNode as a child.  If such a parent is found, will split the document tree from
      {*ioParent,*ioOffset} up to parent, and then insert aNode.  ioParent & ioOffset are then
      adjusted to point to the actual location that aNode was inserted at.  aNoEmptyNodes
      specifies if the splitting process is allowed to reslt in empty nodes.
              nsIDOMNode            *aNode           node to insert
              nsCOMPtr<nsIDOMNode>  *ioParent        insertion parent
              int32_t               *ioOffset        insertion offset
              bool                  aNoEmptyNodes    splitting can result in empty nodes?
*/
nsresult
nsHTMLEditor::InsertNodeAtPoint(nsIDOMNode *aNode,
                                nsCOMPtr<nsIDOMNode> *ioParent,
                                int32_t *ioOffset,
                                bool aNoEmptyNodes)
{
  nsCOMPtr<nsIContent> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(ioParent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(*ioParent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(ioOffset, NS_ERROR_NULL_POINTER);

  nsresult res = NS_OK;
  nsCOMPtr<nsINode> parent = do_QueryInterface(*ioParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsINode> topChild = parent;
  int32_t offsetOfInsert = *ioOffset;

  // Search up the parent chain to find a suitable container
  while (!CanContain(*parent, *node)) {
    // If the current parent is a root (body or table element)
    // then go no further - we can't insert
    if (parent->IsHTMLElement(nsGkAtoms::body) ||
        nsHTMLEditUtils::IsTableElement(parent)) {
      return NS_ERROR_FAILURE;
    }
    // Get the next parent
    NS_ENSURE_TRUE(parent->GetParentNode(), NS_ERROR_FAILURE);
    if (!IsEditable(parent->GetParentNode())) {
      // There's no suitable place to put the node in this editing host.  Maybe
      // someone is trying to put block content in a span.  So just put it
      // where we were originally asked.
      parent = topChild = do_QueryInterface(*ioParent);
      NS_ENSURE_STATE(parent);
      break;
    }
    topChild = parent;
    parent = parent->GetParentNode();
  }
  if (parent != topChild)
  {
    // we need to split some levels above the original selection parent
    res = SplitNodeDeep(GetAsDOMNode(topChild), *ioParent, *ioOffset,
                        &offsetOfInsert, aNoEmptyNodes);
    NS_ENSURE_SUCCESS(res, res);
    *ioParent = GetAsDOMNode(parent);
    *ioOffset = offsetOfInsert;
  }
  // Now we can insert the new node
  res = InsertNode(*node, *parent, offsetOfInsert);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SelectElement(nsIDOMElement* aElement)
{
  nsresult res = NS_ERROR_NULL_POINTER;

  // Must be sure that element is contained in the document body
  if (IsDescendantOfEditorRoot(aElement)) {
    nsRefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    nsCOMPtr<nsIDOMNode>parent;
    res = aElement->GetParentNode(getter_AddRefs(parent));
    if (NS_SUCCEEDED(res) && parent)
    {
      int32_t offsetInParent = GetChildOffset(aElement, parent);

      // Collapse selection to just before desired element,
      res = selection->Collapse(parent, offsetInParent);
      if (NS_SUCCEEDED(res)) {
        // then extend it to just after
        res = selection->Extend(parent, offsetInParent + 1);
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetCaretAfterElement(nsIDOMElement* aElement)
{
  nsresult res = NS_ERROR_NULL_POINTER;

  // Be sure the element is contained in the document body
  if (aElement && IsDescendantOfEditorRoot(aElement)) {
    nsRefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    nsCOMPtr<nsIDOMNode>parent;
    res = aElement->GetParentNode(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
    int32_t offsetInParent = GetChildOffset(aElement, parent);
    // Collapse selection to just after desired element,
    res = selection->Collapse(parent, offsetInParent + 1);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetParagraphFormat(const nsAString& aParagraphFormat)
{
  nsAutoString tag; tag.Assign(aParagraphFormat);
  ToLowerCase(tag);
  if (tag.EqualsLiteral("dd") || tag.EqualsLiteral("dt"))
    return MakeDefinitionItem(tag);
  else
    return InsertBasicBlock(tag);
}

NS_IMETHODIMP
nsHTMLEditor::GetParagraphState(bool *aMixed, nsAString &outFormat)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  nsRefPtr<nsHTMLEditRules> htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());

  return htmlRules->GetParagraphState(aMixed, outFormat);
}

NS_IMETHODIMP
nsHTMLEditor::GetBackgroundColorState(bool *aMixed, nsAString &aOutColor)
{
  nsresult res;
  if (IsCSSEnabled()) {
    // if we are in CSS mode, we have to check if the containing block defines
    // a background color
    res = GetCSSBackgroundColorState(aMixed, aOutColor, true);
  }
  else {
    // in HTML mode, we look only at page's background
    res = GetHTMLBackgroundColorState(aMixed, aOutColor);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetHighlightColorState(bool *aMixed, nsAString &aOutColor)
{
  nsresult res = NS_OK;
  *aMixed = false;
  aOutColor.AssignLiteral("transparent");
  if (IsCSSEnabled()) {
    // in CSS mode, text background can be added by the Text Highlight button
    // we need to query the background of the selection without looking for
    // the block container of the ranges in the selection
    res = GetCSSBackgroundColorState(aMixed, aOutColor, false);
  }
  return res;
}

nsresult
nsHTMLEditor::GetCSSBackgroundColorState(bool *aMixed, nsAString &aOutColor, bool aBlockLevel)
{
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  // the default background color is transparent
  aOutColor.AssignLiteral("transparent");

  // get selection
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection && selection->GetRangeAt(0));

  // get selection location
  nsCOMPtr<nsINode> parent = selection->GetRangeAt(0)->GetStartParent();
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
    nsCOMPtr<Element> blockParent;
    if (NodeIsBlockStatic(nodeToExamine)) {
      blockParent = nodeToExamine->AsElement();
    } else {
      blockParent = GetBlockNodeParent(nodeToExamine);
    }
    NS_ENSURE_TRUE(blockParent, NS_OK);

    // Make sure to not walk off onto the Document node
    do {
      // retrieve the computed style of background-color for blockParent
      mHTMLCSSUtils->GetComputedProperty(*blockParent,
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
      mHTMLCSSUtils->GetDefaultBackgroundColor(aOutColor);
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
      }
      else {
        // no, it's not; let's retrieve the computed style of background-color for the
        // node to examine
        mHTMLCSSUtils->GetComputedProperty(*nodeToExamine,
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
nsHTMLEditor::GetHTMLBackgroundColorState(bool *aMixed, nsAString &aOutColor)
{
  //TODO: We don't handle "mixed" correctly!
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  aOutColor.Truncate();

  nsCOMPtr<nsIDOMElement> domElement;
  int32_t selectedCount;
  nsAutoString tagName;
  nsresult res = GetSelectedOrParentTableElement(tagName,
                                                 &selectedCount,
                                                 getter_AddRefs(domElement));
  NS_ENSURE_SUCCESS(res, res);

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
nsHTMLEditor::GetListState(bool *aMixed, bool *aOL, bool *aUL, bool *aDL)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  NS_ENSURE_TRUE(aMixed && aOL && aUL && aDL, NS_ERROR_NULL_POINTER);
  nsRefPtr<nsHTMLEditRules> htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());

  return htmlRules->GetListState(aMixed, aOL, aUL, aDL);
}

NS_IMETHODIMP
nsHTMLEditor::GetListItemState(bool *aMixed, bool *aLI, bool *aDT, bool *aDD)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  NS_ENSURE_TRUE(aMixed && aLI && aDT && aDD, NS_ERROR_NULL_POINTER);

  nsRefPtr<nsHTMLEditRules> htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());

  return htmlRules->GetListItemState(aMixed, aLI, aDT, aDD);
}

NS_IMETHODIMP
nsHTMLEditor::GetAlignment(bool *aMixed, nsIHTMLEditor::EAlignment *aAlign)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  NS_ENSURE_TRUE(aMixed && aAlign, NS_ERROR_NULL_POINTER);
  nsRefPtr<nsHTMLEditRules> htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());

  return htmlRules->GetAlignment(aMixed, aAlign);
}


NS_IMETHODIMP
nsHTMLEditor::GetIndentState(bool *aCanIndent, bool *aCanOutdent)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  NS_ENSURE_TRUE(aCanIndent && aCanOutdent, NS_ERROR_NULL_POINTER);

  nsRefPtr<nsHTMLEditRules> htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());

  return htmlRules->GetIndentState(aCanIndent, aCanOutdent);
}

NS_IMETHODIMP
nsHTMLEditor::MakeOrChangeList(const nsAString& aListType, bool entireList, const nsAString& aBulletType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  bool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::makeList, nsIEditor::eNext);

  // pre-process
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsTextRulesInfo ruleInfo(EditAction::makeList);
  ruleInfo.blockType = &aListType;
  ruleInfo.entireList = entireList;
  ruleInfo.bulletType = &aBulletType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // Find out if the selection is collapsed:
    bool isCollapsed = selection->Collapsed();

    nsCOMPtr<nsINode> node;
    int32_t offset;
    res = GetStartNodeAndOffset(selection, getter_AddRefs(node), &offset);
    if (!node) res = NS_ERROR_FAILURE;
    NS_ENSURE_SUCCESS(res, res);

    if (isCollapsed)
    {
      // have to find a place to put the list
      nsCOMPtr<nsINode> parent = node;
      nsCOMPtr<nsINode> topChild = node;

      nsCOMPtr<nsIAtom> listAtom = do_GetAtom(aListType);
      while (!CanContainTag(*parent, *listAtom)) {
        topChild = parent;
        parent = parent->GetParentNode();
      }

      if (parent != node)
      {
        // we need to split up to the child of parent
        res = SplitNodeDeep(GetAsDOMNode(topChild), GetAsDOMNode(node), offset,
                            &offset);
        NS_ENSURE_SUCCESS(res, res);
      }

      // make a list
      nsCOMPtr<Element> newList = CreateNode(listAtom, parent, offset);
      NS_ENSURE_STATE(newList);
      // make a list item
      nsCOMPtr<Element> newItem = CreateNode(nsGkAtoms::li, newList, 0);
      NS_ENSURE_STATE(newItem);
      res = selection->Collapse(newItem,0);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}


NS_IMETHODIMP
nsHTMLEditor::RemoveList(const nsAString& aListType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  bool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::removeList, nsIEditor::eNext);

  // pre-process
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsTextRulesInfo ruleInfo(EditAction::removeList);
  if (aListType.LowerCaseEqualsLiteral("ol"))
    ruleInfo.bOrdered = true;
  else  ruleInfo.bOrdered = false;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  // no default behavior for this yet.  what would it mean?

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

nsresult
nsHTMLEditor::MakeDefinitionItem(const nsAString& aItemType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  bool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::makeDefListItem, nsIEditor::eNext);

  // pre-process
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsTextRulesInfo ruleInfo(EditAction::makeDefListItem);
  ruleInfo.blockType = &aItemType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // todo: no default for now.  we count on rules to handle it.
  }

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

nsresult
nsHTMLEditor::InsertBasicBlock(const nsAString& aBlockType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  bool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::makeBasicBlock, nsIEditor::eNext);

  // pre-process
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsTextRulesInfo ruleInfo(EditAction::makeBasicBlock);
  ruleInfo.blockType = &aBlockType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // Find out if the selection is collapsed:
    bool isCollapsed = selection->Collapsed();

    nsCOMPtr<nsINode> node;
    int32_t offset;
    res = GetStartNodeAndOffset(selection, getter_AddRefs(node), &offset);
    if (!node) res = NS_ERROR_FAILURE;
    NS_ENSURE_SUCCESS(res, res);

    if (isCollapsed)
    {
      // have to find a place to put the block
      nsCOMPtr<nsINode> parent = node;
      nsCOMPtr<nsINode> topChild = node;

      nsCOMPtr<nsIAtom> blockAtom = do_GetAtom(aBlockType);
      while (!CanContainTag(*parent, *blockAtom)) {
        NS_ENSURE_TRUE(parent->GetParentNode(), NS_ERROR_FAILURE);
        topChild = parent;
        parent = parent->GetParentNode();
      }

      if (parent != node)
      {
        // we need to split up to the child of parent
        res = SplitNodeDeep(GetAsDOMNode(topChild), GetAsDOMNode(node), offset,
                            &offset);
        NS_ENSURE_SUCCESS(res, res);
      }

      // make a block
      nsCOMPtr<Element> newBlock = CreateNode(blockAtom, parent, offset);
      NS_ENSURE_STATE(newBlock);

      // reposition selection to inside the block
      res = selection->Collapse(newBlock,0);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::Indent(const nsAString& aIndent)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  bool cancel, handled;
  EditAction opID = EditAction::indent;
  if (aIndent.LowerCaseEqualsLiteral("outdent"))
  {
    opID = EditAction::outdent;
  }
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, opID, nsIEditor::eNext);

  // pre-process
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsTextRulesInfo ruleInfo(opID);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // Do default - insert a blockquote node if selection collapsed
    nsCOMPtr<nsINode> node;
    int32_t offset;
    bool isCollapsed = selection->Collapsed();

    res = GetStartNodeAndOffset(selection, getter_AddRefs(node), &offset);
    if (!node) res = NS_ERROR_FAILURE;
    NS_ENSURE_SUCCESS(res, res);

    if (aIndent.EqualsLiteral("indent"))
    {
      if (isCollapsed)
      {
        // have to find a place to put the blockquote
        nsCOMPtr<nsINode> parent = node;
        nsCOMPtr<nsINode> topChild = node;
        while (!CanContainTag(*parent, *nsGkAtoms::blockquote)) {
          NS_ENSURE_TRUE(parent->GetParentNode(), NS_ERROR_FAILURE);
          topChild = parent;
          parent = parent->GetParentNode();
        }

        if (parent != node)
        {
          // we need to split up to the child of parent
          res = SplitNodeDeep(GetAsDOMNode(topChild), GetAsDOMNode(node),
                              offset, &offset);
          NS_ENSURE_SUCCESS(res, res);
        }

        // make a blockquote
        nsCOMPtr<Element> newBQ = CreateNode(nsGkAtoms::blockquote, parent, offset);
        NS_ENSURE_STATE(newBQ);
        // put a space in it so layout will draw the list item
        res = selection->Collapse(newBQ,0);
        NS_ENSURE_SUCCESS(res, res);
        res = InsertText(NS_LITERAL_STRING(" "));
        NS_ENSURE_SUCCESS(res, res);
        // reposition selection to before the space character
        res = GetStartNodeAndOffset(selection, getter_AddRefs(node), &offset);
        NS_ENSURE_SUCCESS(res, res);
        res = selection->Collapse(node,0);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

//TODO: IMPLEMENT ALIGNMENT!

NS_IMETHODIMP
nsHTMLEditor::Align(const nsAString& aAlignType)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::align, nsIEditor::eNext);

  nsCOMPtr<nsIDOMNode> node;
  bool cancel, handled;

  // Find out if the selection is collapsed:
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsTextRulesInfo ruleInfo(EditAction::align);
  ruleInfo.alignType = &aAlignType;
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(res))
    return res;

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

already_AddRefed<Element>
nsHTMLEditor::GetElementOrParentByTagName(const nsAString& aTagName,
                                          nsINode* aNode)
{
  MOZ_ASSERT(!aTagName.IsEmpty());

  nsCOMPtr<nsINode> node = aNode;
  if (!node) {
    // If no node supplied, get it from anchor node of current selection
    nsRefPtr<Selection> selection = GetSelection();
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
    if ((getLink && nsHTMLEditUtils::IsLink(current)) ||
        (getNamedAnchor && nsHTMLEditUtils::IsNamedAnchor(current))) {
      return current.forget();
    }
    if (findList) {
      // Match "ol", "ul", or "dl" for lists
      if (nsHTMLEditUtils::IsList(current)) {
        return current.forget();
      }
    } else if (findTableCell) {
      // Table cells are another special case: match either "td" or "th"
      if (nsHTMLEditUtils::IsTableCell(current)) {
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
nsHTMLEditor::GetElementOrParentByTagName(const nsAString& aTagName,
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
    return NS_EDITOR_ELEMENT_NOT_FOUND;
  }

  ret.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetSelectedElement(const nsAString& aTagName, nsIDOMElement** aReturn)
{
  NS_ENSURE_TRUE(aReturn , NS_ERROR_NULL_POINTER);

  // default is null - no element found
  *aReturn = nullptr;

  // First look for a single element in selection
  nsRefPtr<Selection> selection = GetSelection();
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
  nsRefPtr<nsRange> range = selection->GetRangeAt(0);
  NS_ENSURE_STATE(range);

  nsCOMPtr<nsIDOMNode> startParent;
  int32_t startOffset, endOffset;
  nsresult res = range->GetStartContainer(getter_AddRefs(startParent));
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMNode> endParent;
  res = range->GetEndContainer(getter_AddRefs(endParent));
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(res, res);

  // Optimization for a single selected element
  if (startParent && startParent == endParent && (endOffset-startOffset) == 1)
  {
    nsCOMPtr<nsIDOMNode> selectedNode = GetChildAt(startParent, startOffset);
    NS_ENSURE_SUCCESS(res, NS_OK);
    if (selectedNode)
    {
      selectedNode->GetNodeName(domTagName);
      ToLowerCase(domTagName);

      // Test for appropriate node type requested
      if (anyTag || (TagName == domTagName) ||
          (isLinkTag && nsHTMLEditUtils::IsLink(selectedNode)) ||
          (isNamedAnchorTag && nsHTMLEditUtils::IsNamedAnchor(selectedNode)))
      {
        bNodeFound = true;
        selectedElement = do_QueryInterface(selectedNode);
      }
    }
  }

  if (!bNodeFound)
  {
    if (isLinkTag)
    {
      // Link tag is a special case - we return the anchor node
      //  found for any selection that is totally within a link,
      //  included a collapsed selection (just a caret in a link)
      nsCOMPtr<nsIDOMNode> anchorNode;
      res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
      NS_ENSURE_SUCCESS(res, res);
      int32_t anchorOffset = -1;
      if (anchorNode)
        selection->GetAnchorOffset(&anchorOffset);

      nsCOMPtr<nsIDOMNode> focusNode;
      res = selection->GetFocusNode(getter_AddRefs(focusNode));
      NS_ENSURE_SUCCESS(res, res);
      int32_t focusOffset = -1;
      if (focusNode)
        selection->GetFocusOffset(&focusOffset);

      // Link node must be the same for both ends of selection
      if (NS_SUCCEEDED(res) && anchorNode)
      {
        nsCOMPtr<nsIDOMElement> parentLinkOfAnchor;
        res = GetElementOrParentByTagName(NS_LITERAL_STRING("href"), anchorNode, getter_AddRefs(parentLinkOfAnchor));
        // XXX: ERROR_HANDLING  can parentLinkOfAnchor be null?
        if (NS_SUCCEEDED(res) && parentLinkOfAnchor)
        {
          if (isCollapsed)
          {
            // We have just a caret in the link
            bNodeFound = true;
          } else if(focusNode)
          {  // Link node must be the same for both ends of selection
            nsCOMPtr<nsIDOMElement> parentLinkOfFocus;
            res = GetElementOrParentByTagName(NS_LITERAL_STRING("href"), focusNode, getter_AddRefs(parentLinkOfFocus));
            if (NS_SUCCEEDED(res) && parentLinkOfFocus == parentLinkOfAnchor)
              bNodeFound = true;
          }

          // We found a link node parent
          if (bNodeFound) {
            // GetElementOrParentByTagName addref'd this, so we don't need to do it here
            *aReturn = parentLinkOfAnchor;
            NS_IF_ADDREF(*aReturn);
            return NS_OK;
          }
        }
        else if (anchorOffset >= 0)  // Check if link node is the only thing selected
        {
          nsCOMPtr<nsIDOMNode> anchorChild;
          anchorChild = GetChildAt(anchorNode,anchorOffset);
          if (anchorChild && nsHTMLEditUtils::IsLink(anchorChild) &&
              (anchorNode == focusNode) && focusOffset == (anchorOffset+1))
          {
            selectedElement = do_QueryInterface(anchorChild);
            bNodeFound = true;
          }
        }
      }
    }

    if (!isCollapsed)   // Don't bother to examine selection if it is collapsed
    {
      nsRefPtr<nsRange> currange = selection->GetRangeAt(0);
      if (currange) {
        nsCOMPtr<nsIContentIterator> iter =
          do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
        NS_ENSURE_SUCCESS(res, res);

        iter->Init(currange);
        // loop through the content iterator for each content node
        while (!iter->IsDone())
        {
          // Query interface to cast nsIContent to nsIDOMNode
          //  then get tagType to compare to  aTagName
          // Clone node of each desired type and append it to the aDomFrag
          selectedElement = do_QueryInterface(iter->GetCurrentNode());
          if (selectedElement)
          {
            // If we already found a node, then we have another element,
            //  thus there's not just one element selected
            if (bNodeFound)
            {
              bNodeFound = false;
              break;
            }

            selectedElement->GetNodeName(domTagName);
            ToLowerCase(domTagName);

            if (anyTag)
            {
              // Get name of first selected element
              selectedElement->GetTagName(TagName);
              ToLowerCase(TagName);
              anyTag = false;
            }

            // The "A" tag is a pain,
            //  used for both link(href is set) and "Named Anchor"
            nsCOMPtr<nsIDOMNode> selectedNode = do_QueryInterface(selectedElement);
            if ( (isLinkTag && nsHTMLEditUtils::IsLink(selectedNode)) ||
                (isNamedAnchorTag && nsHTMLEditUtils::IsNamedAnchor(selectedNode)) )
            {
              bNodeFound = true;
            } else if (TagName == domTagName) { // All other tag names are handled here
              bNodeFound = true;
            }
            if (!bNodeFound)
            {
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
  if (bNodeFound)
  {

    *aReturn = selectedElement;
    if (selectedElement)
    {
      // Getters must addref
      NS_ADDREF(*aReturn);
    }
  }
  else res = NS_EDITOR_ELEMENT_NOT_FOUND;

  return res;
}

already_AddRefed<Element>
nsHTMLEditor::CreateElementWithDefaults(const nsAString& aTagName)
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
  nsCOMPtr<Element> newElement =
    CreateHTMLContent(nsCOMPtr<nsIAtom>(do_GetAtom(realTagName)));
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
    nsresult res = SetAttributeOrEquivalent(
        static_cast<nsIDOMElement*>(newElement->AsDOMNode()),
        NS_LITERAL_STRING("valign"), NS_LITERAL_STRING("top"), true);
    NS_ENSURE_SUCCESS(res, nullptr);
  }
  // ADD OTHER TAGS HERE

  return newElement.forget();
}

NS_IMETHODIMP
nsHTMLEditor::CreateElementWithDefaults(const nsAString& aTagName, nsIDOMElement** aReturn)
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
nsHTMLEditor::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
  NS_ENSURE_TRUE(aAnchorElement, NS_ERROR_NULL_POINTER);

  // We must have a real selection
  nsRefPtr<Selection> selection = GetSelection();
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
  nsresult res = anchor->GetHref(href);
  NS_ENSURE_SUCCESS(res, res);
  if (href.IsEmpty()) {
    return NS_OK;
  }

  nsAutoEditBatch beginBatching(this);

  // Set all attributes found on the supplied anchor element
  nsCOMPtr<nsIDOMMozNamedAttrMap> attrMap;
  aAnchorElement->GetAttributes(getter_AddRefs(attrMap));
  NS_ENSURE_TRUE(attrMap, NS_ERROR_FAILURE);

  uint32_t count;
  attrMap->GetLength(&count);
  nsAutoString name, value;

  for (uint32_t i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMAttr> attribute;
    res = attrMap->Item(i, getter_AddRefs(attribute));
    NS_ENSURE_SUCCESS(res, res);

    if (attribute) {
      // We must clear the string buffers
      //   because GetName, GetValue appends to previous string!
      name.Truncate();
      value.Truncate();

      res = attribute->GetName(name);
      NS_ENSURE_SUCCESS(res, res);

      res = attribute->GetValue(value);
      NS_ENSURE_SUCCESS(res, res);

      res = SetInlineProperty(nsGkAtoms::a, name, value);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}

nsresult
nsHTMLEditor::SetHTMLBackgroundColor(const nsAString& aColor)
{
  NS_PRECONDITION(mDocWeak, "Missing Editor DOM Document");

  // Find a selected or enclosing table element to set background on
  nsCOMPtr<nsIDOMElement> element;
  int32_t selectedCount;
  nsAutoString tagName;
  nsresult res = GetSelectedOrParentTableElement(tagName, &selectedCount,
                                                 getter_AddRefs(element));
  NS_ENSURE_SUCCESS(res, res);

  bool setColor = !aColor.IsEmpty();

  NS_NAMED_LITERAL_STRING(bgcolor, "bgcolor");
  if (element)
  {
    if (selectedCount > 0)
    {
      // Traverse all selected cells
      nsCOMPtr<nsIDOMElement> cell;
      res = GetFirstSelectedCell(nullptr, getter_AddRefs(cell));
      if (NS_SUCCEEDED(res) && cell)
      {
        while(cell)
        {
          if (setColor)
            res = SetAttribute(cell, bgcolor, aColor);
          else
            res = RemoveAttribute(cell, bgcolor);
          if (NS_FAILED(res)) break;

          GetNextSelectedCell(nullptr, getter_AddRefs(cell));
        };
        return res;
      }
    }
    // If we failed to find a cell, fall through to use originally-found element
  } else {
    // No table element -- set the background color on the body tag
    element = do_QueryInterface(GetRoot());
    NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);
  }
  // Use the editor method that goes through the transaction system
  if (setColor)
    res = SetAttribute(element, bgcolor, aColor);
  else
    res = RemoveAttribute(element, bgcolor);

  return res;
}

NS_IMETHODIMP nsHTMLEditor::SetBodyAttribute(const nsAString& aAttribute, const nsAString& aValue)
{
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level

  NS_ASSERTION(mDocWeak, "Missing Editor DOM Document");

  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement = do_QueryInterface(GetRoot());
  NS_ENSURE_TRUE(bodyElement, NS_ERROR_NULL_POINTER);

  // Use the editor method that goes through the transaction system
  return SetAttribute(bodyElement, aAttribute, aValue);
}

NS_IMETHODIMP
nsHTMLEditor::GetLinkedObjects(nsISupportsArray** aNodeList)
{
  NS_ENSURE_TRUE(aNodeList, NS_ERROR_NULL_POINTER);

  nsresult res;

  res = NS_NewISupportsArray(aNodeList);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(*aNodeList, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContentIterator> iter =
       do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
  NS_ENSURE_TRUE(iter, NS_ERROR_NULL_POINTER);
  if ((NS_SUCCEEDED(res)))
  {
    nsCOMPtr<nsIDocument> doc = GetDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    iter->Init(doc->GetRootElement());

    // loop through the content iterator for each content node
    while (!iter->IsDone())
    {
      nsCOMPtr<nsIDOMNode> node (do_QueryInterface(iter->GetCurrentNode()));
      if (node)
      {
        // Let nsURIRefObject make the hard decisions:
        nsCOMPtr<nsIURIRefObject> refObject;
        res = NS_NewHTMLURIRefObject(getter_AddRefs(refObject), node);
        if (NS_SUCCEEDED(res))
        {
          nsCOMPtr<nsISupports> isupp (do_QueryInterface(refObject));

          (*aNodeList)->AppendElement(isupp);
        }
      }
      iter->Next();
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditor::AddStyleSheet(const nsAString &aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL))
    return NS_OK;

  // Lose the previously-loaded sheet so there's nothing to replace
  // This pattern is different from Override methods because
  //  we must wait to remove mLastStyleSheetURL and add new sheet
  //  at the same time (in StyleSheetLoaded callback) so they are undoable together
  mLastStyleSheetURL.Truncate();
  return ReplaceStyleSheet(aURL);
}

NS_IMETHODIMP
nsHTMLEditor::ReplaceStyleSheet(const nsAString& aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL))
  {
    // Disable last sheet if not the same as new one
    if (!mLastStyleSheetURL.IsEmpty() && !mLastStyleSheetURL.Equals(aURL))
      return EnableStyleSheet(mLastStyleSheetURL, false);

    return NS_OK;
  }

  // Make sure the pres shell doesn't disappear during the load.
  NS_ENSURE_TRUE(mDocWeak, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIURI> uaURI;
  nsresult rv = NS_NewURI(getter_AddRefs(uaURI), aURL);
  NS_ENSURE_SUCCESS(rv, rv);

  return ps->GetDocument()->CSSLoader()->
    LoadSheet(uaURI, nullptr, EmptyCString(), this);
}

NS_IMETHODIMP
nsHTMLEditor::RemoveStyleSheet(const nsAString &aURL)
{
  nsRefPtr<CSSStyleSheet> sheet;
  nsresult rv = GetStyleSheetForURL(aURL, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(sheet, NS_ERROR_UNEXPECTED);

  nsRefPtr<RemoveStyleSheetTxn> txn;
  rv = CreateTxnForRemoveStyleSheet(sheet, getter_AddRefs(txn));
  if (!txn) rv = NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(rv))
  {
    rv = DoTransaction(txn);
    if (NS_SUCCEEDED(rv))
      mLastStyleSheetURL.Truncate();        // forget it

    // Remove it from our internal list
    rv = RemoveStyleSheetFromList(aURL);
  }

  return rv;
}


NS_IMETHODIMP
nsHTMLEditor::AddOverrideStyleSheet(const nsAString& aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL))
    return NS_OK;

  // Make sure the pres shell doesn't disappear during the load.
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIURI> uaURI;
  nsresult rv = NS_NewURI(getter_AddRefs(uaURI), aURL);
  NS_ENSURE_SUCCESS(rv, rv);

  // We MUST ONLY load synchronous local files (no @import)
  // XXXbz Except this will actually try to load remote files
  // synchronously, of course..
  nsRefPtr<CSSStyleSheet> sheet;
  // Editor override style sheets may want to style Gecko anonymous boxes
  rv = ps->GetDocument()->CSSLoader()->
    LoadSheetSync(uaURI, true, true, getter_AddRefs(sheet));

  // Synchronous loads should ALWAYS return completed
  NS_ENSURE_TRUE(sheet, NS_ERROR_NULL_POINTER);

  // Add the override style sheet
  // (This checks if already exists)
  ps->AddOverrideStyleSheet(sheet);

  ps->ReconstructStyleData();

  // Save as the last-loaded sheet
  mLastOverrideStyleSheetURL = aURL;

  //Add URL and style sheet to our lists
  return AddNewStyleSheetToList(aURL, sheet);
}

NS_IMETHODIMP
nsHTMLEditor::ReplaceOverrideStyleSheet(const nsAString& aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL))
  {
    // Disable last sheet if not the same as new one
    if (!mLastOverrideStyleSheetURL.IsEmpty() && !mLastOverrideStyleSheetURL.Equals(aURL))
      return EnableStyleSheet(mLastOverrideStyleSheetURL, false);

    return NS_OK;
  }
  // Remove the previous sheet
  if (!mLastOverrideStyleSheetURL.IsEmpty())
    RemoveOverrideStyleSheet(mLastOverrideStyleSheetURL);

  return AddOverrideStyleSheet(aURL);
}

// Do NOT use transaction system for override style sheets
NS_IMETHODIMP
nsHTMLEditor::RemoveOverrideStyleSheet(const nsAString &aURL)
{
  nsRefPtr<CSSStyleSheet> sheet;
  GetStyleSheetForURL(aURL, getter_AddRefs(sheet));

  // Make sure we remove the stylesheet from our internal list in all
  // cases.
  nsresult rv = RemoveStyleSheetFromList(aURL);

  NS_ENSURE_TRUE(sheet, NS_OK); /// Don't fail if sheet not found

  NS_ENSURE_TRUE(mDocWeak, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  ps->RemoveOverrideStyleSheet(sheet);
  ps->ReconstructStyleData();

  // Remove it from our internal list
  return rv;
}

NS_IMETHODIMP
nsHTMLEditor::EnableStyleSheet(const nsAString &aURL, bool aEnable)
{
  nsRefPtr<CSSStyleSheet> sheet;
  nsresult rv = GetStyleSheetForURL(aURL, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(sheet, NS_OK); // Don't fail if sheet not found

  // Ensure the style sheet is owned by our document.
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  sheet->SetOwningDocument(doc);

  return sheet->SetDisabled(!aEnable);
}

bool
nsHTMLEditor::EnableExistingStyleSheet(const nsAString &aURL)
{
  nsRefPtr<CSSStyleSheet> sheet;
  nsresult rv = GetStyleSheetForURL(aURL, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, false);

  // Enable sheet if already loaded.
  if (sheet)
  {
    // Ensure the style sheet is owned by our document.
    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
    sheet->SetOwningDocument(doc);

    sheet->SetDisabled(false);
    return true;
  }
  return false;
}

nsresult
nsHTMLEditor::AddNewStyleSheetToList(const nsAString &aURL,
                                     CSSStyleSheet* aStyleSheet)
{
  uint32_t countSS = mStyleSheets.Length();
  uint32_t countU = mStyleSheetURLs.Length();

  if (countSS != countU)
    return NS_ERROR_UNEXPECTED;

  if (!mStyleSheetURLs.AppendElement(aURL))
    return NS_ERROR_UNEXPECTED;

  return mStyleSheets.AppendElement(aStyleSheet) ? NS_OK : NS_ERROR_UNEXPECTED;
}

nsresult
nsHTMLEditor::RemoveStyleSheetFromList(const nsAString &aURL)
{
  // is it already in the list?
  size_t foundIndex;
  foundIndex = mStyleSheetURLs.IndexOf(aURL);
  if (foundIndex == mStyleSheetURLs.NoIndex)
    return NS_ERROR_FAILURE;

  // Attempt both removals; if one fails there's not much we can do.
  mStyleSheets.RemoveElementAt(foundIndex);
  mStyleSheetURLs.RemoveElementAt(foundIndex);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetStyleSheetForURL(const nsAString &aURL,
                                  CSSStyleSheet** aStyleSheet)
{
  NS_ENSURE_ARG_POINTER(aStyleSheet);
  *aStyleSheet = 0;

  // is it already in the list?
  size_t foundIndex;
  foundIndex = mStyleSheetURLs.IndexOf(aURL);
  if (foundIndex == mStyleSheetURLs.NoIndex)
    return NS_OK; //No sheet -- don't fail!

  *aStyleSheet = mStyleSheets[foundIndex];
  NS_ENSURE_TRUE(*aStyleSheet, NS_ERROR_FAILURE);

  NS_ADDREF(*aStyleSheet);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetURLForStyleSheet(CSSStyleSheet* aStyleSheet,
                                  nsAString &aURL)
{
  // is it already in the list?
  int32_t foundIndex = mStyleSheets.IndexOf(aStyleSheet);

  // Don't fail if we don't find it in our list
  if (foundIndex == -1)
    return NS_OK;

  // Found it in the list!
  aURL = mStyleSheetURLs[foundIndex];
  return NS_OK;
}

/*
 * nsIEditorMailSupport methods
 */

NS_IMETHODIMP
nsHTMLEditor::GetEmbeddedObjects(nsISupportsArray** aNodeList)
{
  NS_ENSURE_TRUE(aNodeList, NS_ERROR_NULL_POINTER);

  nsresult rv = NS_NewISupportsArray(aNodeList);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(*aNodeList, NS_ERROR_NULL_POINTER);

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
        (*aNodeList)->AppendElement(domNode);
      }
    }
    iter->Next();
  }

  return rv;
}


NS_IMETHODIMP
nsHTMLEditor::DeleteSelectionImpl(EDirection aAction,
                                  EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  nsresult res = nsEditor::DeleteSelectionImpl(aAction, aStripWrappers);
  NS_ENSURE_SUCCESS(res, res);

  // If we weren't asked to strip any wrappers, we're done.
  if (aStripWrappers == eNoStrip) {
    return NS_OK;
  }

  nsRefPtr<Selection> selection = GetSelection();
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
  res = IsEmptyNode(blockParent, &emptyBlockParent);
  NS_ENSURE_SUCCESS(res, res);
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
    res = DeleteNode(content);
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}


nsresult
nsHTMLEditor::DeleteNode(nsINode* aNode)
{
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode);
  return DeleteNode(node);
}

NS_IMETHODIMP
nsHTMLEditor::DeleteNode(nsIDOMNode* aNode)
{
  // do nothing if the node is read-only
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (!IsModifiableNode(aNode) && !IsMozEditorBogusNode(content)) {
    return NS_ERROR_FAILURE;
  }

  return nsEditor::DeleteNode(aNode);
}

nsresult
nsHTMLEditor::DeleteText(nsGenericDOMDataNode& aCharData, uint32_t aOffset,
                         uint32_t aLength)
{
  // Do nothing if the node is read-only
  if (!IsModifiableNode(&aCharData)) {
    return NS_ERROR_FAILURE;
  }

  return nsEditor::DeleteText(aCharData, aOffset, aLength);
}

nsresult
nsHTMLEditor::InsertTextImpl(const nsAString& aStringToInsert,
                             nsCOMPtr<nsINode>* aInOutNode,
                             int32_t* aInOutOffset, nsIDocument* aDoc)
{
  // Do nothing if the node is read-only
  if (!IsModifiableNode(*aInOutNode)) {
    return NS_ERROR_FAILURE;
  }

  return nsEditor::InsertTextImpl(aStringToInsert, aInOutNode, aInOutOffset,
                                  aDoc);
}

void
nsHTMLEditor::ContentAppended(nsIDocument *aDocument, nsIContent* aContainer,
                              nsIContent* aFirstNewContent,
                              int32_t aIndexInContainer)
{
  DoContentInserted(aDocument, aContainer, aFirstNewContent, aIndexInContainer,
                    eAppended);
}

void
nsHTMLEditor::ContentInserted(nsIDocument *aDocument, nsIContent* aContainer,
                              nsIContent* aChild, int32_t aIndexInContainer)
{
  DoContentInserted(aDocument, aContainer, aChild, aIndexInContainer,
                    eInserted);
}

void
nsHTMLEditor::DoContentInserted(nsIDocument* aDocument, nsIContent* aContainer,
                                nsIContent* aChild, int32_t aIndexInContainer,
                                InsertedOrAppended aInsertedOrAppended)
{
  if (!aChild) {
    return;
  }

  nsCOMPtr<nsIHTMLEditor> kungFuDeathGrip(this);

  if (ShouldReplaceRootElement()) {
    nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(
      this, &nsHTMLEditor::ResetRootElementAndEventTarget));
  }
  // We don't need to handle our own modifications
  else if (!mAction && (aContainer ? aContainer->IsEditable() : aDocument->IsEditable())) {
    if (IsMozEditorBogusNode(aChild)) {
      // Ignore insertion of the bogus node
      return;
    }
    // Protect the edit rules object from dying
    nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
    mRules->DocumentModified();

    // Update spellcheck for only the newly-inserted node (bug 743819)
    if (mInlineSpellChecker) {
      nsRefPtr<nsRange> range = new nsRange(aChild);
      int32_t endIndex = aIndexInContainer + 1;
      if (aInsertedOrAppended == eAppended) {
        // Count all the appended nodes
        nsIContent* sibling = aChild->GetNextSibling();
        while (sibling) {
          endIndex++;
          sibling = sibling->GetNextSibling();
        }
      }
      nsresult res = range->Set(aContainer, aIndexInContainer,
                                aContainer, endIndex);
      if (NS_SUCCEEDED(res)) {
        mInlineSpellChecker->SpellCheckRange(range);
      }
    }
  }
}

void
nsHTMLEditor::ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer,
                             nsIContent* aChild, int32_t aIndexInContainer,
                             nsIContent* aPreviousSibling)
{
  nsCOMPtr<nsIHTMLEditor> kungFuDeathGrip(this);

  if (SameCOMIdentity(aChild, mRootElement)) {
    nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(
      this, &nsHTMLEditor::ResetRootElementAndEventTarget));
  }
  // We don't need to handle our own modifications
  else if (!mAction && (aContainer ? aContainer->IsEditable() : aDocument->IsEditable())) {
    if (aChild && IsMozEditorBogusNode(aChild)) {
      // Ignore removal of the bogus node
      return;
    }
    // Protect the edit rules object from dying
    nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
    mRules->DocumentModified();
  }
}

NS_IMETHODIMP_(bool)
nsHTMLEditor::IsModifiableNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return IsModifiableNode(node);
}

bool
nsHTMLEditor::IsModifiableNode(nsINode *aNode)
{
  return !aNode || aNode->IsEditable();
}

NS_IMETHODIMP
nsHTMLEditor::GetIsSelectionEditable(bool* aIsSelectionEditable)
{
  MOZ_ASSERT(aIsSelectionEditable);

  nsRefPtr<Selection> selection = GetSelection();
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
                               nsIWeakReference* aDocWeak)
{
  // Set selection around <head> node
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(aDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

  dom::Element* headNode = doc->GetHeadElement();
  NS_ENSURE_STATE(headNode);

  // Collapse selection to before first child of the head,
  nsresult rv = aSelection->CollapseNative(headNode, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  // Then extend it to just after.
  uint32_t childCount = headNode->GetChildCount();
  return aSelection->ExtendNative(headNode, childCount + 1);
}

NS_IMETHODIMP
nsHTMLEditor::GetHeadContentsAsHTML(nsAString& aOutputString)
{
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // Save current selection
  nsAutoSelectionReset selectionResetter(selection, this);

  nsresult res = SetSelectionAroundHeadChildren(selection, mDocWeak);
  NS_ENSURE_SUCCESS(res, res);

  res = OutputToString(NS_LITERAL_STRING("text/html"),
                       nsIDocumentEncoder::OutputSelectionOnly,
                       aOutputString);
  if (NS_SUCCEEDED(res))
  {
    // Selection always includes <body></body>,
    //  so terminate there
    nsReadingIterator<char16_t> findIter,endFindIter;
    aOutputString.BeginReading(findIter);
    aOutputString.EndReading(endFindIter);
    //counting on our parser to always lower case!!!
    if (CaseInsensitiveFindInReadable(NS_LITERAL_STRING("<body"),
                                      findIter, endFindIter))
    {
      nsReadingIterator<char16_t> beginIter;
      aOutputString.BeginReading(beginIter);
      int32_t offset = Distance(beginIter, findIter);//get the distance

      nsWritingIterator<char16_t> writeIter;
      aOutputString.BeginWriting(writeIter);
      // Ensure the string ends in a newline
      char16_t newline ('\n');
      findIter.advance(-1);
      if (offset ==0 || (offset >0 &&  (*findIter) != newline)) //check for 0
      {
        writeIter.advance(offset);
        *writeIter = newline;
        aOutputString.Truncate(offset+1);
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DebugUnitTests(int32_t *outNumTests, int32_t *outNumTestsFailed)
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
nsHTMLEditor::StyleSheetLoaded(CSSStyleSheet* aSheet, bool aWasAlternate,
                               nsresult aStatus)
{
  nsresult rv = NS_OK;
  nsAutoEditBatch batchIt(this);

  if (!mLastStyleSheetURL.IsEmpty())
    RemoveStyleSheet(mLastStyleSheetURL);

  nsRefPtr<AddStyleSheetTxn> txn;
  rv = CreateTxnForAddStyleSheet(aSheet, getter_AddRefs(txn));
  if (!txn) rv = NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(rv))
  {
    rv = DoTransaction(txn);
    if (NS_SUCCEEDED(rv))
    {
      // Get the URI, then url spec from the sheet
      nsAutoCString spec;
      rv = aSheet->GetSheetURI()->GetSpec(spec);

      if (NS_SUCCEEDED(rv))
      {
        // Save it so we can remove before applying the next one
        mLastStyleSheetURL.AssignWithConversion(spec.get());

        // Also save in our arrays of urls and sheets
        AddNewStyleSheetToList(mLastStyleSheetURL, aSheet);
      }
    }
  }

  return NS_OK;
}


/** All editor operations which alter the doc should be prefaced
 *  with a call to StartOperation, naming the action and direction */
NS_IMETHODIMP
nsHTMLEditor::StartOperation(EditAction opID,
                             nsIEditor::EDirection aDirection)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  nsEditor::StartOperation(opID, aDirection);  // will set mAction, mDirection
  if (mRules) return mRules->BeforeEdit(mAction, mDirection);
  return NS_OK;
}


/** All editor operations which alter the doc should be followed
 *  with a call to EndOperation */
NS_IMETHODIMP
nsHTMLEditor::EndOperation()
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  // post processing
  nsresult res = NS_OK;
  if (mRules) res = mRules->AfterEdit(mAction, mDirection);
  nsEditor::EndOperation();  // will clear mAction, mDirection
  return res;
}

bool
nsHTMLEditor::TagCanContainTag(nsIAtom& aParentTag, nsIAtom& aChildTag)
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
  return nsHTMLEditUtils::CanContain(parentTagEnum, childTagEnum);
}

bool
nsHTMLEditor::IsContainer(nsINode* aNode) {
  MOZ_ASSERT(aNode);

  int32_t tagEnum;
  // XXX Should this handle #cdata-section too?
  if (aNode->IsNodeOfType(nsINode::eTEXT)) {
    tagEnum = eHTMLTag_text;
  } else {
    tagEnum =
      nsContentUtils::GetParserService()->HTMLStringTagToId(aNode->NodeName());
  }

  return nsHTMLEditUtils::IsContainer(tagEnum);
}

bool
nsHTMLEditor::IsContainer(nsIDOMNode *aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  if (!node) {
    return false;
  }
  return IsContainer(node);
}


nsresult
nsHTMLEditor::SelectEntireDocument(Selection* aSelection)
{
  if (!aSelection || !mRules) { return NS_ERROR_NULL_POINTER; }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  // get editor root node
  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(GetRoot());

  // is doc empty?
  bool bDocIsEmpty;
  nsresult res = mRules->DocumentIsEmpty(&bDocIsEmpty);
  NS_ENSURE_SUCCESS(res, res);

  if (bDocIsEmpty)
  {
    // if its empty dont select entire doc - that would select the bogus node
    return aSelection->Collapse(rootElement, 0);
  }

  return nsEditor::SelectEntireDocument(aSelection);
}

NS_IMETHODIMP
nsHTMLEditor::SelectAll()
{
  ForceCompositionEnd();

  nsRefPtr<Selection> selection = GetSelection();
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

  Maybe<mozilla::dom::Selection::AutoApplyUserSelectStyle> userSelection;
  if (!rootContent->IsEditable()) {
    userSelection.emplace(selection);
  }
  return selection->SelectAllChildren(rootElement);
}


// this will NOT find aAttribute unless aAttribute has a non-null value
// so singleton attributes like <Table border> will not be matched!
bool nsHTMLEditor::IsTextPropertySetByContent(nsINode*         aNode,
                                              nsIAtom*         aProperty,
                                              const nsAString* aAttribute,
                                              const nsAString* aValue,
                                              nsAString*       outValue)
{
  MOZ_ASSERT(aNode && aProperty);
  bool isSet;
  IsTextPropertySetByContent(aNode->AsDOMNode(), aProperty, aAttribute, aValue,
                             isSet, outValue);
  return isSet;
}

void nsHTMLEditor::IsTextPropertySetByContent(nsIDOMNode        *aNode,
                                              nsIAtom           *aProperty,
                                              const nsAString   *aAttribute,
                                              const nsAString   *aValue,
                                              bool              &aIsSet,
                                              nsAString *outValue)
{
  nsresult result;
  aIsSet = false;  // must be initialized to false for code below to work
  nsAutoString propName;
  aProperty->ToString(propName);
  nsCOMPtr<nsIDOMNode>node = aNode;

  while (node)
  {
    nsCOMPtr<nsIDOMElement>element;
    element = do_QueryInterface(node);
    if (element)
    {
      nsAutoString tag, value;
      element->GetTagName(tag);
      if (propName.Equals(tag, nsCaseInsensitiveStringComparator()))
      {
        bool found = false;
        if (aAttribute && 0!=aAttribute->Length())
        {
          element->GetAttribute(*aAttribute, value);
          if (outValue) *outValue = value;
          if (!value.IsEmpty())
          {
            if (!aValue) {
              found = true;
            }
            else
            {
              nsString tString(*aValue);
              if (tString.Equals(value, nsCaseInsensitiveStringComparator())) {
                found = true;
              }
              else {  // we found the prop with the attribute, but the value doesn't match
                break;
              }
            }
          }
        }
        else {
          found = true;
        }
        if (found)
        {
          aIsSet = true;
          break;
        }
      }
    }
    nsCOMPtr<nsIDOMNode>temp;
    result = node->GetParentNode(getter_AddRefs(temp));
    if (NS_SUCCEEDED(result) && temp) {
      node = temp;
    }
    else {
      node = nullptr;
    }
  }
}


//================================================================
// HTML Editor methods
//
// Note: Table Editing methods are implemented in nsTableEditor.cpp
//


bool
nsHTMLEditor::SetCaretInTableCell(nsIDOMElement* aElement)
{
  nsCOMPtr<dom::Element> element = do_QueryInterface(aElement);
  if (!element || !element->IsHTMLElement() ||
      !nsHTMLEditUtils::IsTableElement(element) ||
      !IsDescendantOfEditorRoot(element)) {
    return false;
  }

  nsIContent* node = element;
  while (node->HasChildren()) {
    node = node->GetFirstChild();
  }

  // Set selection at beginning of the found node
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, false);

  return NS_SUCCEEDED(selection->CollapseNative(node, 0));
}

///////////////////////////////////////////////////////////////////////////
// GetEnclosingTable: find ancestor who is a table, if any
//
already_AddRefed<Element>
nsHTMLEditor::GetEnclosingTable(nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  for (nsCOMPtr<Element> block = GetBlockNodeParent(aNode);
       block;
       block = GetBlockNodeParent(block)) {
    if (nsHTMLEditUtils::IsTable(block)) {
      return block.forget();
    }
  }
  return nullptr;
}

nsCOMPtr<nsIDOMNode>
nsHTMLEditor::GetEnclosingTable(nsIDOMNode *aNode)
{
  NS_PRECONDITION(aNode, "null node passed to nsHTMLEditor::GetEnclosingTable");
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, nullptr);
  nsCOMPtr<Element> table = GetEnclosingTable(node);
  nsCOMPtr<nsIDOMNode> ret = do_QueryInterface(table);
  return ret;
}


/* this method scans the selection for adjacent text nodes
 * and collapses them into a single text node.
 * "adjacent" means literally adjacent siblings of the same parent.
 * Uses nsEditor::JoinNodes so action is undoable.
 * Should be called within the context of a batch transaction.
 */
nsresult
nsHTMLEditor::CollapseAdjacentTextNodes(nsRange* aInRange)
{
  NS_ENSURE_TRUE(aInRange, NS_ERROR_NULL_POINTER);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  nsTArray<nsCOMPtr<nsIDOMNode> > textNodes;
  // we can't actually do anything during iteration, so store the text nodes in an array
  // don't bother ref counting them because we know we can hold them for the
  // lifetime of this method


  // build a list of editable text nodes
  nsresult result;
  nsCOMPtr<nsIContentIterator> iter =
    do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &result);
  NS_ENSURE_SUCCESS(result, result);

  iter->Init(aInRange);

  while (!iter->IsDone())
  {
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
  while (textNodes.Length() > 1)
  {
    // we assume a textNodes entry can't be nullptr
    nsIDOMNode *leftTextNode = textNodes[0];
    nsIDOMNode *rightTextNode = textNodes[1];
    NS_ASSERTION(leftTextNode && rightTextNode,"left or rightTextNode null in CollapseAdjacentTextNodes");

    // get the prev sibling of the right node, and see if its leftTextNode
    nsCOMPtr<nsIDOMNode> prevSibOfRightNode;
    result =
      rightTextNode->GetPreviousSibling(getter_AddRefs(prevSibOfRightNode));
    NS_ENSURE_SUCCESS(result, result);
    if (prevSibOfRightNode && (prevSibOfRightNode == leftTextNode))
    {
      nsCOMPtr<nsIDOMNode> parent;
      result = rightTextNode->GetParentNode(getter_AddRefs(parent));
      NS_ENSURE_SUCCESS(result, result);
      NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
      result = JoinNodes(leftTextNode, rightTextNode, parent);
      NS_ENSURE_SUCCESS(result, result);
    }

    textNodes.RemoveElementAt(0); // remove the leftmost text node from the list
  }

  return result;
}

nsresult
nsHTMLEditor::SetSelectionAtDocumentStart(Selection* aSelection)
{
  dom::Element* rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER);

  return aSelection->CollapseNative(rootElement, 0);
}


///////////////////////////////////////////////////////////////////////////
// RemoveBlockContainer: remove inNode, reparenting its children into their
//                  the parent of inNode.  In addition, INSERT ANY BR's NEEDED
//                  TO PRESERVE IDENTITY OF REMOVED BLOCK.
//
nsresult
nsHTMLEditor::RemoveBlockContainer(nsIDOMNode *inNode)
{
  nsCOMPtr<nsIContent> node = do_QueryInterface(inNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> sibling, child, unused;

  // Two possibilities: the container cold be empty of editable content.
  // If that is the case, we need to compare what is before and after inNode
  // to determine if we need a br.
  // Or it could not be empty, in which case we have to compare previous
  // sibling and first child to determine if we need a leading br,
  // and compare following sibling and last child to determine if we need a
  // trailing br.

  child = GetAsDOMNode(GetFirstEditableChild(*node));

  if (child)  // the case of inNode not being empty
  {
    // we need a br at start unless:
    // 1) previous sibling of inNode is a block, OR
    // 2) previous sibling of inNode is a br, OR
    // 3) first child of inNode is a block OR
    // 4) either is null

    res = GetPriorHTMLSibling(inNode, address_of(sibling));
    NS_ENSURE_SUCCESS(res, res);
    if (sibling && !IsBlockNode(sibling) && !nsTextEditUtils::IsBreak(sibling))
    {
      if (!IsBlockNode(child)) {
        // insert br node
        res = CreateBR(inNode, 0, address_of(unused));
        NS_ENSURE_SUCCESS(res, res);
      }
    }

    // we need a br at end unless:
    // 1) following sibling of inNode is a block, OR
    // 2) last child of inNode is a block, OR
    // 3) last child of inNode is a block OR
    // 4) either is null

    res = GetNextHTMLSibling(inNode, address_of(sibling));
    NS_ENSURE_SUCCESS(res, res);
    if (sibling && !IsBlockNode(sibling))
    {
      child = GetAsDOMNode(GetLastEditableChild(*node));
      if (child && !IsBlockNode(child) && !nsTextEditUtils::IsBreak(child))
      {
        // insert br node
        uint32_t len;
        res = GetLengthOfDOMNode(inNode, len);
        NS_ENSURE_SUCCESS(res, res);
        res = CreateBR(inNode, (int32_t)len, address_of(unused));
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  else  // the case of inNode being empty
  {
    // we need a br at start unless:
    // 1) previous sibling of inNode is a block, OR
    // 2) previous sibling of inNode is a br, OR
    // 3) following sibling of inNode is a block, OR
    // 4) following sibling of inNode is a br OR
    // 5) either is null
    res = GetPriorHTMLSibling(inNode, address_of(sibling));
    NS_ENSURE_SUCCESS(res, res);
    if (sibling && !IsBlockNode(sibling) && !nsTextEditUtils::IsBreak(sibling))
    {
      res = GetNextHTMLSibling(inNode, address_of(sibling));
      NS_ENSURE_SUCCESS(res, res);
      if (sibling && !IsBlockNode(sibling) && !nsTextEditUtils::IsBreak(sibling))
      {
        // insert br node
        res = CreateBR(inNode, 0, address_of(unused));
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }

  // now remove container
  return RemoveContainer(node);
}


///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLSibling: returns the previous editable sibling, if there is
//                   one within the parent
//
nsIContent*
nsHTMLEditor::GetPriorHTMLSibling(nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  nsIContent* node = aNode->GetPreviousSibling();
  while (node && !IsEditable(node)) {
    node = node->GetPreviousSibling();
  }

  return node;
}

nsresult
nsHTMLEditor::GetPriorHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  *outNode = nullptr;

  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  *outNode = do_QueryInterface(GetPriorHTMLSibling(node));
  return NS_OK;
}



///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLSibling: returns the previous editable sibling, if there is
//                   one within the parent.  just like above routine but
//                   takes a parent/offset instead of a node.
//
nsIContent*
nsHTMLEditor::GetPriorHTMLSibling(nsINode* aParent, int32_t aOffset)
{
  MOZ_ASSERT(aParent);

  nsIContent* node = aParent->GetChildAt(aOffset - 1);
  if (!node || IsEditable(node)) {
    return node;
  }

  return GetPriorHTMLSibling(node);
}

nsresult
nsHTMLEditor::GetPriorHTMLSibling(nsIDOMNode *inParent, int32_t inOffset, nsCOMPtr<nsIDOMNode> *outNode)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  *outNode = nullptr;

  nsCOMPtr<nsINode> parent = do_QueryInterface(inParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  *outNode = do_QueryInterface(GetPriorHTMLSibling(parent, inOffset));
  return NS_OK;
}



///////////////////////////////////////////////////////////////////////////
// GetNextHTMLSibling: returns the next editable sibling, if there is
//                   one within the parent
//
nsIContent*
nsHTMLEditor::GetNextHTMLSibling(nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  nsIContent* node = aNode->GetNextSibling();
  while (node && !IsEditable(node)) {
    node = node->GetNextSibling();
  }

  return node;
}

nsresult
nsHTMLEditor::GetNextHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  *outNode = nullptr;

  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  *outNode = do_QueryInterface(GetNextHTMLSibling(node));
  return NS_OK;
}



///////////////////////////////////////////////////////////////////////////
// GetNextHTMLSibling: returns the next editable sibling, if there is
//                   one within the parent.  just like above routine but
//                   takes a parent/offset instead of a node.
nsIContent*
nsHTMLEditor::GetNextHTMLSibling(nsINode* aParent, int32_t aOffset)
{
  MOZ_ASSERT(aParent);

  nsIContent* node = aParent->GetChildAt(aOffset + 1);
  if (!node || IsEditable(node)) {
    return node;
  }

  return GetNextHTMLSibling(node);
}

nsresult
nsHTMLEditor::GetNextHTMLSibling(nsIDOMNode *inParent, int32_t inOffset, nsCOMPtr<nsIDOMNode> *outNode)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  *outNode = nullptr;

  nsCOMPtr<nsINode> parent = do_QueryInterface(inParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  *outNode = do_QueryInterface(GetNextHTMLSibling(parent, inOffset));
  return NS_OK;
}



///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLNode: returns the previous editable leaf node, if there is
//                   one within the <body>
//
nsIContent*
nsHTMLEditor::GetPriorHTMLNode(nsINode* aNode, bool aNoBlockCrossing)
{
  MOZ_ASSERT(aNode);

  if (!GetActiveEditingHost()) {
    return nullptr;
  }

  return GetPriorNode(aNode, true, aNoBlockCrossing);
}

nsresult
nsHTMLEditor::GetPriorHTMLNode(nsIDOMNode* aNode,
                               nsCOMPtr<nsIDOMNode>* aResultNode,
                               bool aNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetPriorHTMLNode(node, aNoBlockCrossing));
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLNode: same as above but takes {parent,offset} instead of node
//
nsIContent*
nsHTMLEditor::GetPriorHTMLNode(nsINode* aParent, int32_t aOffset,
                               bool aNoBlockCrossing)
{
  MOZ_ASSERT(aParent);

  if (!GetActiveEditingHost()) {
    return nullptr;
  }

  return GetPriorNode(aParent, aOffset, true, aNoBlockCrossing);
}

nsresult
nsHTMLEditor::GetPriorHTMLNode(nsIDOMNode* aNode, int32_t aOffset,
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


///////////////////////////////////////////////////////////////////////////
// GetNextHTMLNode: returns the next editable leaf node, if there is
//                   one within the <body>
//
nsIContent*
nsHTMLEditor::GetNextHTMLNode(nsINode* aNode, bool aNoBlockCrossing)
{
  MOZ_ASSERT(aNode);

  nsIContent* result = GetNextNode(aNode, true, aNoBlockCrossing);

  if (result && !IsDescendantOfEditorRoot(result)) {
    return nullptr;
  }

  return result;
}

nsresult
nsHTMLEditor::GetNextHTMLNode(nsIDOMNode* aNode,
                              nsCOMPtr<nsIDOMNode>* aResultNode,
                              bool aNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetNextHTMLNode(node, aNoBlockCrossing));
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetNextHTMLNode: same as above but takes {parent,offset} instead of node
//
nsIContent*
nsHTMLEditor::GetNextHTMLNode(nsINode* aParent, int32_t aOffset,
                              bool aNoBlockCrossing)
{
  nsIContent* content = GetNextNode(aParent, aOffset, true, aNoBlockCrossing);
  if (content && !IsDescendantOfEditorRoot(content)) {
    return nullptr;
  }
  return content;
}

nsresult
nsHTMLEditor::GetNextHTMLNode(nsIDOMNode* aNode, int32_t aOffset,
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


nsresult
nsHTMLEditor::IsFirstEditableChild( nsIDOMNode *aNode, bool *aOutIsFirst)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(aOutIsFirst && node, NS_ERROR_NULL_POINTER);

  // init out parms
  *aOutIsFirst = false;

  // find first editable child and compare it to aNode
  nsCOMPtr<nsINode> parent = node->GetParentNode();
  NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);

  *aOutIsFirst = (GetFirstEditableChild(*parent) == node);
  return NS_OK;
}


nsresult
nsHTMLEditor::IsLastEditableChild( nsIDOMNode *aNode, bool *aOutIsLast)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(aOutIsLast && node, NS_ERROR_NULL_POINTER);

  // init out parms
  *aOutIsLast = false;

  // find last editable child and compare it to aNode
  nsCOMPtr<nsINode> parent = node->GetParentNode();
  NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);

  *aOutIsLast = (GetLastEditableChild(*parent) == node);
  return NS_OK;
}


nsIContent*
nsHTMLEditor::GetFirstEditableChild(nsINode& aNode)
{
  nsCOMPtr<nsIContent> child = aNode.GetFirstChild();

  while (child && !IsEditable(child)) {
    child = child->GetNextSibling();
  }

  return child;
}


nsIContent*
nsHTMLEditor::GetLastEditableChild(nsINode& aNode)
{
  nsCOMPtr<nsIContent> child = aNode.GetLastChild();

  while (child && !IsEditable(child)) {
    child = child->GetPreviousSibling();
  }

  return child;
}

nsIContent*
nsHTMLEditor::GetFirstEditableLeaf(nsINode& aNode)
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
nsHTMLEditor::GetLastEditableLeaf(nsINode& aNode)
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


///////////////////////////////////////////////////////////////////////////
// IsVisTextNode: figure out if textnode aTextNode has any visible content.
//
nsresult
nsHTMLEditor::IsVisTextNode(nsIContent* aNode,
                            bool* outIsEmptyNode,
                            bool aSafeToAskFrames)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(aNode->NodeType() == nsIDOMNode::TEXT_NODE);
  MOZ_ASSERT(outIsEmptyNode);

  *outIsEmptyNode = true;

  uint32_t length = aNode->TextLength();
  if (aSafeToAskFrames)
  {
    nsCOMPtr<nsISelectionController> selCon;
    nsresult res = GetSelectionController(getter_AddRefs(selCon));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);
    bool isVisible = false;
    // ask the selection controller for information about whether any
    // of the data in the node is really rendered.  This is really
    // something that frames know about, but we aren't supposed to talk to frames.
    // So we put a call in the selection controller interface, since it's already
    // in bed with frames anyway.  (this is a fix for bug 22227, and a
    // partial fix for bug 46209)
    res = selCon->CheckVisibilityContent(aNode, 0, length, &isVisible);
    NS_ENSURE_SUCCESS(res, res);
    if (isVisible)
    {
      *outIsEmptyNode = false;
    }
  }
  else if (length)
  {
    if (aNode->TextIsOnlyWhitespace())
    {
      nsWSRunObject wsRunObj(this, aNode, 0);
      nsCOMPtr<nsINode> visNode;
      int32_t outVisOffset=0;
      WSType visType;
      wsRunObj.NextVisibleNode(aNode, 0, address_of(visNode),
                               &outVisOffset, &visType);
      if (visType == WSType::normalWS || visType == WSType::text) {
        *outIsEmptyNode = (aNode != visNode);
      }
    }
    else
    {
      *outIsEmptyNode = false;
    }
  }
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsEmptyNode: figure out if aNode is an empty node.
//               A block can have children and still be considered empty,
//               if the children are empty or non-editable.
//
nsresult
nsHTMLEditor::IsEmptyNode( nsIDOMNode *aNode,
                           bool *outIsEmptyNode,
                           bool aSingleBRDoesntCount,
                           bool aListOrCellNotEmpty,
                           bool aSafeToAskFrames)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return IsEmptyNode(node, outIsEmptyNode, aSingleBRDoesntCount,
                     aListOrCellNotEmpty, aSafeToAskFrames);
}

nsresult
nsHTMLEditor::IsEmptyNode(nsINode* aNode,
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

///////////////////////////////////////////////////////////////////////////
// IsEmptyNodeImpl: workhorse for IsEmptyNode.
//
nsresult
nsHTMLEditor::IsEmptyNodeImpl(nsINode* aNode,
                              bool *outIsEmptyNode,
                              bool aSingleBRDoesntCount,
                              bool aListOrCellNotEmpty,
                              bool aSafeToAskFrames,
                              bool *aSeenBR)
{
  NS_ENSURE_TRUE(aNode && outIsEmptyNode && aSeenBR, NS_ERROR_NULL_POINTER);

  if (aNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    return IsVisTextNode(static_cast<nsIContent*>(aNode), outIsEmptyNode, aSafeToAskFrames);
  }

  // if it's not a text node (handled above) and it's not a container,
  // then we don't call it empty (it's an <hr>, or <br>, etc).
  // Also, if it's an anchor then don't treat it as empty - even though
  // anchors are containers, named anchors are "empty" but we don't
  // want to treat them as such.  Also, don't call ListItems or table
  // cells empty if caller desires.  Form Widgets not empty.
  if (!IsContainer(aNode->AsDOMNode())                      ||
      (nsHTMLEditUtils::IsNamedAnchor(aNode) ||
       nsHTMLEditUtils::IsFormWidget(aNode) ||
       (aListOrCellNotEmpty &&
        (nsHTMLEditUtils::IsListItem(aNode) ||
         nsHTMLEditUtils::IsTableCell(aNode))))) {
    *outIsEmptyNode = false;
    return NS_OK;
  }

  // need this for later
  bool isListItemOrCell = nsHTMLEditUtils::IsListItem(aNode) ||
                          nsHTMLEditUtils::IsTableCell(aNode);

  // loop over children of node. if no children, or all children are either
  // empty text nodes or non-editable, then node qualifies as empty
  for (nsCOMPtr<nsIContent> child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    // Is the child editable and non-empty?  if so, return false
    if (nsEditor::IsEditable(child)) {
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
              if (nsHTMLEditUtils::IsList(child) ||
                  child->IsHTMLElement(nsGkAtoms::table)) {
                // break out if we find we aren't empty
                *outIsEmptyNode = false;
                return NS_OK;
              }
            } else if (nsHTMLEditUtils::IsFormWidget(child)) {
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
nsHTMLEditor::SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                       const nsAString & aAttribute,
                                       const nsAString & aValue,
                                       bool aSuppressTransaction)
{
  nsAutoScriptBlocker scriptBlocker;

  nsresult res = NS_OK;
  if (IsCSSEnabled() && mHTMLCSSUtils) {
    int32_t count;
    res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(aElement, nullptr, &aAttribute, &aValue, &count,
                                                     aSuppressTransaction);
    NS_ENSURE_SUCCESS(res, res);
    if (count) {
      // we found an equivalence ; let's remove the HTML attribute itself if it is set
      nsAutoString existingValue;
      bool wasSet = false;
      res = GetAttributeValue(aElement, aAttribute, existingValue, &wasSet);
      NS_ENSURE_SUCCESS(res, res);
      if (wasSet) {
        if (aSuppressTransaction)
          res = aElement->RemoveAttribute(aAttribute);
        else
          res = RemoveAttribute(aElement, aAttribute);
      }
    }
    else {
      // count is an integer that represents the number of CSS declarations applied to the
      // element. If it is zero, we found no equivalence in this implementation for the
      // attribute
      if (aAttribute.EqualsLiteral("style")) {
        // if it is the style attribute, just add the new value to the existing style
        // attribute's value
        nsAutoString existingValue;
        bool wasSet = false;
        res = GetAttributeValue(aElement, NS_LITERAL_STRING("style"), existingValue, &wasSet);
        NS_ENSURE_SUCCESS(res, res);
        existingValue.Append(' ');
        existingValue.Append(aValue);
        if (aSuppressTransaction)
          res = aElement->SetAttribute(aAttribute, existingValue);
        else
          res = SetAttribute(aElement, aAttribute, existingValue);
      }
      else {
        // we have no CSS equivalence for this attribute and it is not the style
        // attribute; let's set it the good'n'old HTML way
        if (aSuppressTransaction)
          res = aElement->SetAttribute(aAttribute, aValue);
        else
          res = SetAttribute(aElement, aAttribute, aValue);
      }
    }
  }
  else {
    // we are not in an HTML+CSS editor; let's set the attribute the HTML way
    if (aSuppressTransaction)
      res = aElement->SetAttribute(aAttribute, aValue);
    else
      res = SetAttribute(aElement, aAttribute, aValue);
  }
  return res;
}

nsresult
nsHTMLEditor::RemoveAttributeOrEquivalent(nsIDOMElement* aElement,
                                          const nsAString& aAttribute,
                                          bool aSuppressTransaction)
{
  nsCOMPtr<dom::Element> element = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(element, NS_OK);

  nsCOMPtr<nsIAtom> attribute = do_GetAtom(aAttribute);
  MOZ_ASSERT(attribute);

  nsresult res = NS_OK;
  if (IsCSSEnabled() && mHTMLCSSUtils) {
    res = mHTMLCSSUtils->RemoveCSSEquivalentToHTMLStyle(
        element, nullptr, &aAttribute, nullptr, aSuppressTransaction);
    NS_ENSURE_SUCCESS(res, res);
  }

  if (element->HasAttr(kNameSpaceID_None, attribute)) {
    if (aSuppressTransaction) {
      res = element->UnsetAttr(kNameSpaceID_None, attribute,
                               /* aNotify = */ true);
    } else {
      res = RemoveAttribute(aElement, aAttribute);
    }
  }
  return res;
}

nsresult
nsHTMLEditor::SetIsCSSEnabled(bool aIsCSSPrefChecked)
{
  if (!mHTMLCSSUtils) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mHTMLCSSUtils->SetCSSEnabled(aIsCSSPrefChecked);

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
nsHTMLEditor::SetCSSBackgroundColor(const nsAString& aColor)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);
  ForceCompositionEnd();

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  bool isCollapsed = selection->Collapsed();

  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, EditAction::insertElement,
                                 nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);

  bool cancel, handled;
  nsTextRulesInfo ruleInfo(EditAction::setTextProperty);
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled) {
    // Loop through the ranges in the selection
    NS_NAMED_LITERAL_STRING(bgcolor, "bgcolor");
    for (uint32_t i = 0; i < selection->RangeCount(); i++) {
      nsRefPtr<nsRange> range = selection->GetRangeAt(i);
      NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

      nsCOMPtr<Element> cachedBlockParent;

      // Check for easy case: both range endpoints in same text node
      nsCOMPtr<nsINode> startNode = range->GetStartParent();
      int32_t startOffset = range->StartOffset();
      nsCOMPtr<nsINode> endNode = range->GetEndParent();
      int32_t endOffset = range->EndOffset();
      if (startNode == endNode && IsTextNode(startNode)) {
        // Let's find the block container of the text node
        nsCOMPtr<Element> blockParent = GetBlockNodeParent(startNode);
        // And apply the background color to that block container
        if (blockParent && cachedBlockParent != blockParent) {
          cachedBlockParent = blockParent;
          mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(blockParent, nullptr,
                                                     &bgcolor, &aColor, false);
        }
      } else if (startNode == endNode &&
                 startNode->IsHTMLElement(nsGkAtoms::body) && isCollapsed) {
        // No block in the document, let's apply the background to the body
        mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(startNode->AsElement(),
                                                   nullptr, &bgcolor, &aColor,
                                                   false);
      } else if (startNode == endNode && (endOffset - startOffset == 1 ||
                                          (!startOffset && !endOffset))) {
        // A unique node is selected, let's also apply the background color to
        // the containing block, possibly the node itself
        nsCOMPtr<nsIContent> selectedNode = startNode->GetChildAt(startOffset);
        nsCOMPtr<Element> blockParent;
        if (NodeIsBlockStatic(selectedNode)) {
          blockParent = selectedNode->AsElement();
        } else {
          blockParent = GetBlockNodeParent(selectedNode);
        }
        if (blockParent && cachedBlockParent != blockParent) {
          cachedBlockParent = blockParent;
          mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(blockParent, nullptr,
                                                     &bgcolor, &aColor, false);
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
        res = iter->Init(range);
        // Init returns an error if no nodes in range.  This can easily happen
        // with the subtree iterator if the selection doesn't contain any
        // *whole* nodes.
        if (NS_SUCCEEDED(res)) {
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
            mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(blockParent, nullptr,
                                                       &bgcolor, &aColor,
                                                       false);
          }
        }

        // Then loop through the list, set the property on each node
        for (auto& node : arrayOfNodes) {
          nsCOMPtr<Element> blockParent;
          if (NodeIsBlockStatic(node)) {
            blockParent = node->AsElement();
          } else {
            blockParent = GetBlockNodeParent(node);
          }
          if (blockParent && cachedBlockParent != blockParent) {
            cachedBlockParent = blockParent;
            mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(blockParent, nullptr,
                                                       &bgcolor, &aColor,
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
            mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(blockParent, nullptr,
                                                       &bgcolor, &aColor,
                                                       false);
          }
        }
      }
    }
  }
  if (!cancel) {
    // Post-process
    res = mRules->DidDoAction(selection, &ruleInfo, res);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetBackgroundColor(const nsAString& aColor)
{
  nsresult res;
  if (IsCSSEnabled()) {
    // if we are in CSS mode, we have to apply the background color to the
    // containing block (or the body if we have no block-level element in
    // the document)
    res = SetCSSBackgroundColor(aColor);
  }
  else {
    // but in HTML mode, we can only set the document's background color
    res = SetHTMLBackgroundColor(aColor);
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// NodesSameType: do these nodes have the same tag?
//
/* virtual */
bool
nsHTMLEditor::AreNodesSameType(nsIContent* aNode1, nsIContent* aNode2)
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
  return mHTMLCSSUtils->ElementsSameStyle(aNode1->AsDOMNode(),
                                          aNode2->AsDOMNode());
}

nsresult
nsHTMLEditor::CopyLastEditableChildStyles(nsIDOMNode * aPreviousBlock, nsIDOMNode * aNewBlock,
                                          nsIDOMNode **aOutBrNode)
{
  nsCOMPtr<nsINode> newBlock = do_QueryInterface(aNewBlock);
  NS_ENSURE_STATE(newBlock || !aNewBlock);
  *aOutBrNode = nullptr;
  nsCOMPtr<nsIDOMNode> child, tmp;
  nsresult res;
  // first, clear out aNewBlock.  Contract is that we want only the styles from previousBlock.
  res = aNewBlock->GetFirstChild(getter_AddRefs(child));
  while (NS_SUCCEEDED(res) && child)
  {
    res = DeleteNode(child);
    NS_ENSURE_SUCCESS(res, res);
    res = aNewBlock->GetFirstChild(getter_AddRefs(child));
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
  while (child && nsTextEditUtils::IsBreak(child)) {
    nsCOMPtr<nsIDOMNode> priorNode;
    res = GetPriorHTMLNode(child, address_of(priorNode));
    NS_ENSURE_SUCCESS(res, res);
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
    if (nsHTMLEditUtils::IsInlineStyle(childElement) ||
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
    *aOutBrNode = GetAsDOMNode(CreateBR(deepestStyle, 0).take());
    NS_ENSURE_STATE(*aOutBrNode);
  }
  return NS_OK;
}

nsresult
nsHTMLEditor::GetElementOrigin(nsIDOMElement * aElement, int32_t & aX, int32_t & aY)
{
  aX = 0;
  aY = 0;

  NS_ENSURE_TRUE(mDocWeak, NS_ERROR_NOT_INITIALIZED);
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
nsHTMLEditor::EndUpdateViewBatch()
{
  nsresult res = nsEditor::EndUpdateViewBatch();
  NS_ENSURE_SUCCESS(res, res);

  // We may need to show resizing handles or update existing ones after
  // all transactions are done. This way of doing is preferred to DOM
  // mutation events listeners because all the changes the user can apply
  // to a document may result in multiple events, some of them quite hard
  // to listen too (in particular when an ancestor of the selection is
  // changed but the selection itself is not changed).
  if (mUpdateCount == 0) {
    nsRefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);
    res = CheckSelectionStateForAnonymousButtons(selection);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetSelectionContainer(nsIDOMElement ** aReturn)
{
  nsRefPtr<Selection> selection = GetSelection();
  // if we don't get the selection, just skip this
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> focusNode;

  nsresult res;
  if (selection->Collapsed()) {
    res = selection->GetFocusNode(getter_AddRefs(focusNode));
    NS_ENSURE_SUCCESS(res, res);
  } else {

    int32_t rangeCount;
    res = selection->GetRangeCount(&rangeCount);
    NS_ENSURE_SUCCESS(res, res);

    if (rangeCount == 1) {

      nsRefPtr<nsRange> range = selection->GetRangeAt(0);
      NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

      nsCOMPtr<nsIDOMNode> startContainer, endContainer;
      res = range->GetStartContainer(getter_AddRefs(startContainer));
      NS_ENSURE_SUCCESS(res, res);
      res = range->GetEndContainer(getter_AddRefs(endContainer));
      NS_ENSURE_SUCCESS(res, res);
      int32_t startOffset, endOffset;
      res = range->GetStartOffset(&startOffset);
      NS_ENSURE_SUCCESS(res, res);
      res = range->GetEndOffset(&endOffset);
      NS_ENSURE_SUCCESS(res, res);

      nsCOMPtr<nsIDOMElement> focusElement;
      if (startContainer == endContainer && startOffset + 1 == endOffset) {
        res = GetSelectedElement(EmptyString(), getter_AddRefs(focusElement));
        NS_ENSURE_SUCCESS(res, res);
        if (focusElement)
          focusNode = do_QueryInterface(focusElement);
      }
      if (!focusNode) {
        res = range->GetCommonAncestorContainer(getter_AddRefs(focusNode));
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    else {
      int32_t i;
      nsRefPtr<nsRange> range;
      for (i = 0; i < rangeCount; i++)
      {
        range = selection->GetRangeAt(i);
        NS_ENSURE_STATE(range);
        nsCOMPtr<nsIDOMNode> startContainer;
        res = range->GetStartContainer(getter_AddRefs(startContainer));
        if (NS_FAILED(res)) continue;
        if (!focusNode)
          focusNode = startContainer;
        else if (focusNode != startContainer) {
          res = startContainer->GetParentNode(getter_AddRefs(focusNode));
          NS_ENSURE_SUCCESS(res, res);
          break;
        }
      }
    }
  }

  if (focusNode) {
    uint16_t nodeType;
    focusNode->GetNodeType(&nodeType);
    if (nsIDOMNode::TEXT_NODE == nodeType) {
      nsCOMPtr<nsIDOMNode> parent;
      res = focusNode->GetParentNode(getter_AddRefs(parent));
      NS_ENSURE_SUCCESS(res, res);
      focusNode = parent;
    }
  }

  nsCOMPtr<nsIDOMElement> focusElement = do_QueryInterface(focusNode);
  focusElement.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::IsAnonymousElement(nsIDOMElement * aElement, bool * aReturn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  *aReturn = content->IsRootOfNativeAnonymousSubtree();
  return NS_OK;
}

nsresult
nsHTMLEditor::SetReturnInParagraphCreatesNewParagraph(bool aCreatesNewParagraph)
{
  mCRInParagraphCreatesParagraph = aCreatesNewParagraph;
  return NS_OK;
}

bool
nsHTMLEditor::GetReturnInParagraphCreatesNewParagraph()
{
  return mCRInParagraphCreatesParagraph;
}

nsresult
nsHTMLEditor::GetReturnInParagraphCreatesNewParagraph(bool *aCreatesNewParagraph)
{
  *aCreatesNewParagraph = mCRInParagraphCreatesParagraph;
  return NS_OK;
}

already_AddRefed<nsIContent>
nsHTMLEditor::GetFocusedContent()
{
  NS_ENSURE_TRUE(mDocWeak, nullptr);

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nullptr);

  nsCOMPtr<nsIContent> focusedContent = fm->GetFocusedContent();

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  bool inDesignMode = doc->HasFlag(NODE_IS_EDITABLE);
  if (!focusedContent) {
    // in designMode, nobody gets focus in most cases.
    if (inDesignMode && OurWindowHasFocus()) {
      nsCOMPtr<nsIContent> docRoot = doc->GetRootElement();
      return docRoot.forget();
    }
    return nullptr;
  }

  if (inDesignMode) {
    return OurWindowHasFocus() &&
      nsContentUtils::ContentIsDescendantOf(focusedContent, doc) ?
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
nsHTMLEditor::GetFocusedContentForIME()
{
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (!focusedContent) {
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, nullptr);
  return doc->HasFlag(NODE_IS_EDITABLE) ? nullptr : focusedContent.forget();
}

bool
nsHTMLEditor::IsActiveInDOMWindow()
{
  NS_ENSURE_TRUE(mDocWeak, false);

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  bool inDesignMode = doc->HasFlag(NODE_IS_EDITABLE);

  // If we're in designMode, we're always active in the DOM window.
  if (inDesignMode) {
    return true;
  }

  nsPIDOMWindow* ourWindow = doc->GetWindow();
  nsCOMPtr<nsPIDOMWindow> win;
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

dom::Element*
nsHTMLEditor::GetActiveEditingHost()
{
  NS_ENSURE_TRUE(mDocWeak, nullptr);

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, nullptr);
  if (doc->HasFlag(NODE_IS_EDITABLE)) {
    return doc->GetBodyElement();
  }

  // We're HTML editor for contenteditable
  nsRefPtr<Selection> selection = GetSelection();
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

already_AddRefed<mozilla::dom::EventTarget>
nsHTMLEditor::GetDOMEventTarget()
{
  // Don't use getDocument here, because we have no way of knowing
  // whether Init() was ever called.  So we need to get the document
  // ourselves, if it exists.
  NS_PRECONDITION(mDocWeak, "This editor has not been initialized yet");
  nsCOMPtr<mozilla::dom::EventTarget> target = do_QueryReferent(mDocWeak);
  return target.forget();
}

bool
nsHTMLEditor::ShouldReplaceRootElement()
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
nsHTMLEditor::ResetRootElementAndEventTarget()
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
nsHTMLEditor::GetBodyElement(nsIDOMHTMLElement** aBody)
{
  NS_PRECONDITION(mDocWeak, "bad state, null mDocWeak");
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryReferent(mDocWeak);
  if (!htmlDoc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return htmlDoc->GetBody(aBody);
}

already_AddRefed<nsINode>
nsHTMLEditor::GetFocusedNode()
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

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  return doc.forget();
}

bool
nsHTMLEditor::OurWindowHasFocus()
{
  NS_ENSURE_TRUE(mDocWeak, false);
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);
  nsCOMPtr<nsIDOMWindow> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow) {
    return false;
  }
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  nsCOMPtr<nsIDOMWindow> ourWindow = do_QueryInterface(doc->GetWindow());
  return ourWindow == focusedWindow;
}

bool
nsHTMLEditor::IsAcceptableInputEvent(nsIDOMEvent* aEvent)
{
  if (!nsEditor::IsAcceptableInputEvent(aEvent)) {
    return false;
  }

  // While there is composition, all composition events in its top level window
  // are always fired on the composing editor.  Therefore, if this editor has
  // composition, the composition events should be handled in this editor.
  if (mComposition && aEvent->GetInternalNSEvent()->AsCompositionEvent()) {
    return true;
  }

  NS_ENSURE_TRUE(mDocWeak, false);

  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));
  NS_ENSURE_TRUE(target, false);

  nsCOMPtr<nsIDocument> document = do_QueryReferent(mDocWeak);
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
    return document == targetContent->GetCurrentDoc();
  }

  // This HTML editor is for contenteditable.  We need to check the validity of
  // the target.
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);
  NS_ENSURE_TRUE(targetContent, false);

  // If the event is a mouse event, we need to check if the target content is
  // the focused editing host or its descendant.
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aEvent);
  if (mouseEvent) {
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
nsHTMLEditor::GetPreferredIMEState(IMEState *aState)
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
nsHTMLEditor::GetInputEventTargetContent()
{
  nsCOMPtr<nsIContent> target = GetActiveEditingHost();
  return target.forget();
}

bool
nsHTMLEditor::IsEditable(nsINode* aNode) {
  if (!nsPlaintextEditor::IsEditable(aNode)) {
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

// virtual override
dom::Element*
nsHTMLEditor::GetEditorRoot()
{
  return GetActiveEditingHost();
}
