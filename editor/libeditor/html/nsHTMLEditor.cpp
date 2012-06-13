/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCRT.h"

#include "nsUnicharUtils.h"

#include "nsHTMLEditor.h"
#include "nsHTMLEditRules.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"

#include "nsHTMLEditorEventListener.h"
#include "TypeInState.h"

#include "nsHTMLURIRefObject.h"

#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDocument.h"
#include "nsIDOMEventTarget.h" 
#include "nsIDOMKeyEvent.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsISelectionController.h"
#include "nsIDOMHTMLDocument.h"
#include "nsILinkHandler.h"

#include "mozilla/css/Loader.h"
#include "nsCSSStyleSheet.h"
#include "nsIDOMStyleSheet.h"

#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMRange.h"
#include "nsISupportsArray.h"
#include "nsContentUtils.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMDocumentFragment.h"
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
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::widget;

// Some utilities to handle annoying overloading of "A" tag for link and named anchor
static char hrefText[] = "href";
static char anchorTxt[] = "anchor";
static char namedanchorText[] = "namedanchor";

#define IsLinkTag(s) (s.EqualsIgnoreCase(hrefText))
#define IsNamedAnchorTag(s) (s.EqualsIgnoreCase(anchorTxt) || s.EqualsIgnoreCase(namedanchorText))

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
  nsCOMPtr<nsISelection>selection;
  nsresult result = GetSelection(getter_AddRefs(selection));
  // if we don't get the selection, just skip this
  if (NS_SUCCEEDED(result) && selection) 
  {
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
    nsCOMPtr<nsISelectionListener>listener;
    listener = do_QueryInterface(mTypeInState);
    if (listener)
    {
      selPriv->RemoveSelectionListener(listener); 
    }
    listener = do_QueryInterface(mSelectionListenerP);
    if (listener)
    {
      selPriv->RemoveSelectionListener(listener); 
    }
  }

  mTypeInState = nsnull;
  mSelectionListenerP = nsnull;

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
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTypeInState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTextServices)

  tmp->HideAnonymousEditingUIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLEditor, nsPlaintextEditor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTypeInState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTextServices)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTopLeftHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTopHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTopRightHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLeftHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRightHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mBottomLeftHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mBottomHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mBottomRightHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mActivatedHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mResizingShadow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mResizingInfo)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mResizedObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mMouseMotionListenerP)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSelectionListenerP)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mResizeEventListenerP)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(objectResizeEventListeners)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAbsolutelyPositionedObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mGrabber)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPositioningShadow)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mInlineEditedCell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAddColumnBeforeButton)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRemoveColumnButton)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAddColumnAfterButton)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAddRowBeforeButton)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRemoveRowButton)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAddRowAfterButton)
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
                   PRUint32 aFlags)
{
  NS_PRECONDITION(aDoc && !aSelCon, "bad arg");
  NS_ENSURE_TRUE(aDoc, NS_ERROR_NULL_POINTER);

  nsresult result = NS_OK, rulesRes = NS_OK;
   
  if (1)
  {
    // block to scope nsAutoEditInitRulesTrigger
    nsAutoEditInitRulesTrigger rulesTrigger(static_cast<nsPlaintextEditor*>(this), rulesRes);

    // Init the plaintext editor
    result = nsPlaintextEditor::Init(aDoc, aRoot, nsnull, aFlags);
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

      context->SetLinkHandler(nsnull);
    }

    // init the type-in state
    mTypeInState = new TypeInState();

    // init the selection listener for image resizing
    mSelectionListenerP = new ResizerSelectionListener(this);

    if (!IsInteractionAllowed()) {
      // ignore any errors from this in case the file is missing
      AddOverrideStyleSheet(NS_LITERAL_STRING("resource://gre/res/EditorOverride.css"));
    }

    nsCOMPtr<nsISelection>selection;
    result = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) { return result; }
    if (selection) 
    {
      nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
      nsCOMPtr<nsISelectionListener>listener;
      listener = do_QueryInterface(mTypeInState);
      if (listener) {
        selPriv->AddSelectionListener(listener); 
      }
      listener = do_QueryInterface(mSelectionListenerP);
      if (listener) {
        selPriv->AddSelectionListener(listener); 
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

  *aRootElement = nsnull;

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
    return nsnull;
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
    return nsnull;
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

  mMouseMotionListenerP = nsnull;
  mResizeEventListenerP = nsnull;

  nsPlaintextEditor::RemoveEventListeners();
}

NS_IMETHODIMP 
nsHTMLEditor::SetFlags(PRUint32 aFlags)
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
  // instantiate the rules for the html editor
  mRules = new nsHTMLEditRules();
  return mRules->Init(static_cast<nsPlaintextEditor*>(this));
}

NS_IMETHODIMP
nsHTMLEditor::BeginningOfDocument()
{
  if (!mDocWeak) { return NS_ERROR_NOT_INITIALIZED; }

  // get the selection
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);

  // Get the root element.
  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(GetRoot());
  if (!rootElement) {
    NS_WARNING("GetRoot() returned a null pointer (mRootElement is null)");
    return NS_OK;
  }

  // find first editable thingy
  bool done = false;
  nsCOMPtr<nsIDOMNode> curNode(rootElement), selNode;
  PRInt32 curOffset = 0, selOffset;
  while (!done)
  {
    nsWSRunObject wsObj(this, curNode, curOffset);
    nsCOMPtr<nsIDOMNode> visNode;
    PRInt32 visOffset=0;
    PRInt16 visType=0;
    wsObj.NextVisibleNode(curNode, curOffset, address_of(visNode), &visOffset, &visType);
    if ((visType==nsWSRunObject::eNormalWS) || 
        (visType==nsWSRunObject::eText))
    {
      selNode = visNode;
      selOffset = visOffset;
      done = true;
    }
    else if ((visType==nsWSRunObject::eBreak)    ||
             (visType==nsWSRunObject::eSpecial))
    {
      res = GetNodeLocation(visNode, address_of(selNode), &selOffset);
      NS_ENSURE_SUCCESS(res, res); 
      done = true;
    }
    else if (visType==nsWSRunObject::eOtherBlock)
    {
      // By definition of nsWSRunObject, a block element terminates 
      // a whitespace run. That is, although we are calling a method 
      // that is named "NextVisibleNode", the node returned
      // might not be visible/editable!
      // If the given block does not contain any visible/editable items,
      // we want to skip it and continue our search.

      if (!IsContainer(visNode))
      {
        // However, we were given a block that is not a container.
        // Since the block can not contain anything that's visible,
        // such a block only makes sense if it is visible by itself,
        // like a <hr>
        // We want to place the caret in front of that block.

        res = GetNodeLocation(visNode, address_of(selNode), &selOffset);
        NS_ENSURE_SUCCESS(res, res); 
        done = true;
      }
      else
      {
        bool isEmptyBlock;
        if (NS_SUCCEEDED(IsEmptyNode(visNode, &isEmptyBlock)) &&
            isEmptyBlock)
        {
          // skip the empty block
          res = GetNodeLocation(visNode, address_of(curNode), &curOffset);
          NS_ENSURE_SUCCESS(res, res); 
          ++curOffset;
        }
        else
        {
          curNode = visNode;
          curOffset = 0;
        }
        // keep looping
      }
    }
    else
    {
      // else we found nothing useful
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
  //   * editor/libeditor/html/tests/test_htmleditor_keyevent_handling.html

  if (IsReadonly() || IsDisabled()) {
    // When we're not editable, the events are handled on nsEditor, so, we can
    // bypass nsPlaintextEditor.
    return nsEditor::HandleKeyPressEvent(aKeyEvent);
  }

  nsKeyEvent* nativeKeyEvent = GetNativeKeyEvent(aKeyEvent);
  NS_ENSURE_TRUE(nativeKeyEvent, NS_ERROR_UNEXPECTED);
  NS_ASSERTION(nativeKeyEvent->message == NS_KEY_PRESS,
               "HandleKeyPressEvent gets non-keypress event");

  switch (nativeKeyEvent->keyCode) {
    case nsIDOMKeyEvent::DOM_VK_META:
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
          nativeKeyEvent->IsMeta()) {
        return NS_OK;
      }

      nsCOMPtr<nsISelection> selection;
      nsresult rv = GetSelection(getter_AddRefs(selection));
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt32 offset;
      nsCOMPtr<nsIDOMNode> node, blockParent;
      rv = GetStartNodeAndOffset(selection, getter_AddRefs(node), &offset);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

      bool isBlock = false;
      NodeIsBlock(node, &isBlock);
      if (isBlock) {
        blockParent = node;
      } else {
        blockParent = GetBlockNodeParent(node);
      }

      if (!blockParent) {
        break;
      }

      bool handled = false;
      if (nsHTMLEditUtils::IsTableElement(blockParent)) {
        rv = TabInTable(nativeKeyEvent->IsShift(), &handled);
        if (handled) {
          ScrollSelectionIntoView(false);
        }
      } else if (nsHTMLEditUtils::IsListItem(blockParent)) {
        rv = Indent(nativeKeyEvent->IsShift() ?
                      NS_LITERAL_STRING("outdent") :
                      NS_LITERAL_STRING("indent"));
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
    case nsIDOMKeyEvent::DOM_VK_ENTER:
      if (nativeKeyEvent->IsControl() || nativeKeyEvent->IsAlt() ||
          nativeKeyEvent->IsMeta()) {
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
      nativeKeyEvent->IsAlt() || nativeKeyEvent->IsMeta()) {
    // we don't PreventDefault() here or keybindings like control-x won't work
    return NS_OK;
  }
  aKeyEvent->PreventDefault();
  nsAutoString str(nativeKeyEvent->charCode);
  return TypedText(str, eTypedText);
}

/**
 * Returns true if the id represents an element of block type.
 * Can be used to determine if a new paragraph should be started.
 */
nsresult
nsHTMLEditor::NodeIsBlockStatic(nsIDOMNode *aNode, bool *aIsBlock)
{
  if (!aNode || !aIsBlock) { return NS_ERROR_NULL_POINTER; }

  *aIsBlock = false;

#define USE_PARSER_FOR_BLOCKNESS 1
#ifdef USE_PARSER_FOR_BLOCKNESS
  nsresult rv;

  nsCOMPtr<nsIDOMElement>element = do_QueryInterface(aNode);
  if (!element)
  {
    // We don't have an element -- probably a text node
    return NS_OK;
  }

  nsIAtom *tagAtom = GetTag(aNode);
  NS_ENSURE_TRUE(tagAtom, NS_ERROR_NULL_POINTER);

  // Nodes we know we want to treat as block
  // even though the parser says they're not:
  if (tagAtom==nsEditProperty::body       ||
      tagAtom==nsEditProperty::head       ||
      tagAtom==nsEditProperty::tbody      ||
      tagAtom==nsEditProperty::thead      ||
      tagAtom==nsEditProperty::tfoot      ||
      tagAtom==nsEditProperty::tr         ||
      tagAtom==nsEditProperty::th         ||
      tagAtom==nsEditProperty::td         ||
      tagAtom==nsEditProperty::li         ||
      tagAtom==nsEditProperty::dt         ||
      tagAtom==nsEditProperty::dd         ||
      tagAtom==nsEditProperty::pre)
  {
    *aIsBlock = true;
    return NS_OK;
  }

  rv = nsContentUtils::GetParserService()->
    IsBlock(nsContentUtils::GetParserService()->HTMLAtomTagToId(tagAtom),
            *aIsBlock);

#ifdef DEBUG
  // Check this against what we would have said with the old code:
  if (tagAtom==nsEditProperty::p          ||
      tagAtom==nsEditProperty::div        ||
      tagAtom==nsEditProperty::blockquote ||
      tagAtom==nsEditProperty::h1         ||
      tagAtom==nsEditProperty::h2         ||
      tagAtom==nsEditProperty::h3         ||
      tagAtom==nsEditProperty::h4         ||
      tagAtom==nsEditProperty::h5         ||
      tagAtom==nsEditProperty::h6         ||
      tagAtom==nsEditProperty::ul         ||
      tagAtom==nsEditProperty::ol         ||
      tagAtom==nsEditProperty::dl         ||
      tagAtom==nsEditProperty::noscript   ||
      tagAtom==nsEditProperty::form       ||
      tagAtom==nsEditProperty::hr         ||
      tagAtom==nsEditProperty::table      ||
      tagAtom==nsEditProperty::fieldset   ||
      tagAtom==nsEditProperty::address    ||
      tagAtom==nsEditProperty::caption    ||
      tagAtom==nsEditProperty::col        ||
      tagAtom==nsEditProperty::colgroup   ||
      tagAtom==nsEditProperty::li         ||
      tagAtom==nsEditProperty::dt         ||
      tagAtom==nsEditProperty::dd         ||
      tagAtom==nsEditProperty::legend     )
  {
    if (!(*aIsBlock))
    {
      nsAutoString assertmsg (NS_LITERAL_STRING("Parser and editor disagree on blockness: "));

      nsAutoString tagName;
      rv = element->GetTagName(tagName);
      NS_ENSURE_SUCCESS(rv, rv);

      assertmsg.Append(tagName);
      char* assertstr = ToNewCString(assertmsg);
      NS_ASSERTION(*aIsBlock, assertstr);
      NS_Free(assertstr);
    }
  }
#endif /* DEBUG */

  return rv;
#else /* USE_PARSER_FOR_BLOCKNESS */
  nsresult result = NS_ERROR_FAILURE;
  *aIsBlock = false;
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString tagName;
    result = element->GetTagName(tagName);
    if (NS_SUCCEEDED(result))
    {
      ToLowerCase(tagName);
      nsCOMPtr<nsIAtom> tagAtom = do_GetAtom(tagName);
      if (!tagAtom) { return NS_ERROR_NULL_POINTER; }

      if (tagAtom==nsEditProperty::p          ||
          tagAtom==nsEditProperty::div        ||
          tagAtom==nsEditProperty::blockquote ||
          tagAtom==nsEditProperty::h1         ||
          tagAtom==nsEditProperty::h2         ||
          tagAtom==nsEditProperty::h3         ||
          tagAtom==nsEditProperty::h4         ||
          tagAtom==nsEditProperty::h5         ||
          tagAtom==nsEditProperty::h6         ||
          tagAtom==nsEditProperty::ul         ||
          tagAtom==nsEditProperty::ol         ||
          tagAtom==nsEditProperty::dl         ||
          tagAtom==nsEditProperty::pre        ||
          tagAtom==nsEditProperty::noscript   ||
          tagAtom==nsEditProperty::form       ||
          tagAtom==nsEditProperty::hr         ||
          tagAtom==nsEditProperty::fieldset   ||
          tagAtom==nsEditProperty::address    ||
          tagAtom==nsEditProperty::body       ||
          tagAtom==nsEditProperty::caption    ||
          tagAtom==nsEditProperty::table      ||
          tagAtom==nsEditProperty::tbody      ||
          tagAtom==nsEditProperty::thead      ||
          tagAtom==nsEditProperty::tfoot      ||
          tagAtom==nsEditProperty::tr         ||
          tagAtom==nsEditProperty::td         ||
          tagAtom==nsEditProperty::th         ||
          tagAtom==nsEditProperty::col        ||
          tagAtom==nsEditProperty::colgroup   ||
          tagAtom==nsEditProperty::li         ||
          tagAtom==nsEditProperty::dt         ||
          tagAtom==nsEditProperty::dd         ||
          tagAtom==nsEditProperty::legend     )
      {
        *aIsBlock = true;
      }
      result = NS_OK;
    }
  } else {
    // We don't have an element -- probably a text node
    nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(aNode);
    if (nodeAsText)
    {
      *aIsBlock = false;
      result = NS_OK;
    }
  }
  return result;

#endif /* USE_PARSER_FOR_BLOCKNESS */
}

NS_IMETHODIMP
nsHTMLEditor::NodeIsBlock(nsIDOMNode *aNode, bool *aIsBlock)
{
  return NodeIsBlockStatic(aNode, aIsBlock);
}

bool
nsHTMLEditor::IsBlockNode(nsIDOMNode *aNode)
{
  bool isBlock;
  NodeIsBlockStatic(aNode, &isBlock);
  return isBlock;
}

bool
nsHTMLEditor::IsBlockNode(nsINode *aNode)
{
  bool isBlock;
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode);
  NodeIsBlockStatic(node, &isBlock);
  return isBlock;
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
already_AddRefed<nsIDOMNode>
nsHTMLEditor::GetBlockNodeParent(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to GetBlockNodeParent()");
    return nsnull;
  }

  nsCOMPtr<nsIDOMNode> p;
  if (NS_FAILED(aNode->GetParentNode(getter_AddRefs(p))))  // no parent, ran off top of tree
    return nsnull;

  nsCOMPtr<nsIDOMNode> tmp;
  while (p)
  {
    bool isBlock;
    if (NS_FAILED(NodeIsBlockStatic(p, &isBlock)) || isBlock)
      break;
    if (NS_FAILED(p->GetParentNode(getter_AddRefs(tmp))) || !tmp) // no parent, ran off top of tree
      break;

    p = tmp;
  }
  return p.forget();
}

///////////////////////////////////////////////////////////////////////////
// NextNodeInBlock: gets the next/prev node in the block, if any.  Next node
//                  must be an element or text node, others are ignored
already_AddRefed<nsIDOMNode>
nsHTMLEditor::NextNodeInBlock(nsIDOMNode *aNode, IterDirection aDir)
{
  NS_ENSURE_TRUE(aNode, nsnull);

  nsresult rv;
  nsCOMPtr<nsIContentIterator> iter =
       do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &rv);
  NS_ENSURE_SUCCESS(rv, nsnull);

  // much gnashing of teeth as we twit back and forth between content and domnode types
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  nsCOMPtr<nsIDOMNode> blockParent;
  bool isBlock;
  if (NS_SUCCEEDED(NodeIsBlockStatic(aNode, &isBlock)) && isBlock) {
    blockParent = aNode;
  } else {
    blockParent = GetBlockNodeParent(aNode);
  }
  NS_ENSURE_TRUE(blockParent, nsnull);
  nsCOMPtr<nsIContent> blockContent = do_QueryInterface(blockParent);
  NS_ENSURE_TRUE(blockContent, nsnull);
  
  if (NS_FAILED(iter->Init(blockContent))) {
    return nsnull;
  }
  if (NS_FAILED(iter->PositionAt(content))) {
    return nsnull;
  }
  
  while (!iter->IsDone()) {
    // ignore nodes that aren't elements or text, or that are the
    // block parent
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(iter->GetCurrentNode());
    if (node && IsTextOrElementNode(node) && node != blockParent &&
        node != aNode)
      return node.forget();

    if (aDir == kIterForward)
      iter->Next();
    else
      iter->Prev();
  }
  
  return nsnull;
}

static const PRUnichar nbsp = 160;

///////////////////////////////////////////////////////////////////////////
// IsNextCharWhitespace: checks the adjacent content in the same block
//                       to see if following selection is whitespace or nbsp
void
nsHTMLEditor::IsNextCharWhitespace(nsIDOMNode *aParentNode, 
                                   PRInt32 aOffset,
                                   bool *outIsSpace,
                                   bool *outIsNBSP,
                                   nsCOMPtr<nsIDOMNode> *outNode,
                                   PRInt32 *outOffset)
{
  MOZ_ASSERT(outIsSpace && outIsNBSP);
  *outIsSpace = false;
  *outIsNBSP = false;
  if (outNode) *outNode = nsnull;
  if (outOffset) *outOffset = -1;
  
  nsAutoString tempString;
  PRUint32 strLength;
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aParentNode);
  if (textNode)
  {
    textNode->GetLength(&strLength);
    if ((PRUint32)aOffset < strLength)
    {
      // easy case: next char is in same node
      textNode->SubstringData(aOffset,aOffset+1,tempString);
      *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
      *outIsNBSP = (tempString.First() == nbsp);
      if (outNode) *outNode = do_QueryInterface(aParentNode);
      if (outOffset) *outOffset = aOffset+1;  // yes, this is _past_ the character; 
      return;
    }
  }
  
  // harder case: next char in next node.
  nsCOMPtr<nsIDOMNode> node = NextNodeInBlock(aParentNode, kIterForward);
  nsCOMPtr<nsIDOMNode> tmp;
  while (node) 
  {
    bool isBlock (false);
    NodeIsBlock(node, &isBlock);
    if (isBlock)  // skip over bold, italic, link, ect nodes
    {
      if (IsTextNode(node) && IsEditable(node))
      {
        textNode = do_QueryInterface(node);
        textNode->GetLength(&strLength);
        if (strLength)
        {
          textNode->SubstringData(0,1,tempString);
          *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
          *outIsNBSP = (tempString.First() == nbsp);
          if (outNode) *outNode = do_QueryInterface(node);
          if (outOffset) *outOffset = 1;  // yes, this is _past_ the character; 
          return;
        }
        // else it's an empty text node, or not editable; skip it.
      }
      else  // node is an image or some other thingy that doesn't count as whitespace
      {
        break;
      }
    }
    tmp = node;
    node = NextNodeInBlock(tmp, kIterForward);
  }
}


///////////////////////////////////////////////////////////////////////////
// IsPrevCharWhitespace: checks the adjacent content in the same block
//                       to see if following selection is whitespace
void
nsHTMLEditor::IsPrevCharWhitespace(nsIDOMNode *aParentNode, 
                                   PRInt32 aOffset,
                                   bool *outIsSpace,
                                   bool *outIsNBSP,
                                   nsCOMPtr<nsIDOMNode> *outNode,
                                   PRInt32 *outOffset)
{
  MOZ_ASSERT(outIsSpace && outIsNBSP);
  *outIsSpace = false;
  *outIsNBSP = false;
  if (outNode) *outNode = nsnull;
  if (outOffset) *outOffset = -1;
  
  nsAutoString tempString;
  PRUint32 strLength;
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aParentNode);
  if (textNode)
  {
    if (aOffset > 0)
    {
      // easy case: prev char is in same node
      textNode->SubstringData(aOffset-1,aOffset,tempString);
      *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
      *outIsNBSP = (tempString.First() == nbsp);
      if (outNode) *outNode = do_QueryInterface(aParentNode);
      if (outOffset) *outOffset = aOffset-1;  
      return;
    }
  }
  
  // harder case: prev char in next node
  nsCOMPtr<nsIDOMNode> node = NextNodeInBlock(aParentNode, kIterBackward);
  nsCOMPtr<nsIDOMNode> tmp;
  while (node) 
  {
    bool isBlock (false);
    NodeIsBlock(node, &isBlock);
    if (isBlock)  // skip over bold, italic, link, ect nodes
    {
      if (IsTextNode(node) && IsEditable(node))
      {
        textNode = do_QueryInterface(node);
        textNode->GetLength(&strLength);
        if (strLength)
        {
          // you could use nsIContent::TextIsOnlyWhitespace here
          textNode->SubstringData(strLength-1,strLength,tempString);
          *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
          *outIsNBSP = (tempString.First() == nbsp);
          if (outNode) *outNode = do_QueryInterface(aParentNode);
          if (outOffset) *outOffset = strLength-1;  
          return;
        }
        // else it's an empty text node, or not editable; skip it.
      }
      else  // node is an image or some other thingy that doesn't count as whitespace
      {
        break;
      }
    }
    // otherwise we found a node we want to skip, keep going
    tmp = node;
    node = NextNodeInBlock(tmp, kIterBackward);
  }
}



/* ------------ End Block methods -------------- */


bool nsHTMLEditor::IsVisBreak(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, false);
  if (!nsTextEditUtils::IsBreak(aNode)) 
    return false;
  // check if there is a later node in block after br
  nsCOMPtr<nsIDOMNode> priorNode, nextNode;
  GetPriorHTMLNode(aNode, address_of(priorNode), true); 
  GetNextHTMLNode(aNode, address_of(nextNode), true); 
  // if we are next to another break, we are visible
  if (priorNode && nsTextEditUtils::IsBreak(priorNode))
    return true;
  if (nextNode && nsTextEditUtils::IsBreak(nextNode))
    return true;
  
  // if we are right before block boundary, then br not visible
  NS_ENSURE_TRUE(nextNode, false);  // this break is trailer in block, it's not visible
  if (IsBlockNode(nextNode))
    return false; // break is right before a block, it's not visible
    
  // sigh.  We have to use expensive whitespace calculation code to 
  // determine what is going on
  nsCOMPtr<nsIDOMNode> selNode, tmp;
  PRInt32 selOffset;
  GetNodeLocation(aNode, address_of(selNode), &selOffset);
  selOffset++; // lets look after the break
  nsWSRunObject wsObj(this, selNode, selOffset);
  nsCOMPtr<nsIDOMNode> visNode;
  PRInt32 visOffset=0;
  PRInt16 visType=0;
  wsObj.NextVisibleNode(selNode, selOffset, address_of(visNode), &visOffset, &visType);
  if (visType & nsWSRunObject::eBlock)
    return false;
  
  return true;
}

NS_IMETHODIMP
nsHTMLEditor::BreakIsVisible(nsIDOMNode *aNode, bool *aIsVisible)
{
  NS_ENSURE_ARG_POINTER(aNode && aIsVisible);

  *aIsVisible = IsVisBreak(aNode);

  return NS_OK;
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
  nsCOMPtr<nsIDOMDocument> domDoc = GetDOMDocument();
  NS_ENSURE_TRUE(domDoc, NS_ERROR_FAILURE);

  // Look for an HTML <base> tag
  nsCOMPtr<nsIDOMNodeList> nodeList;
  nsresult rv = domDoc->GetElementsByTagName(NS_LITERAL_STRING("base"), getter_AddRefs(nodeList));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> baseNode;
  if (nodeList)
  {
    PRUint32 count;
    nodeList->GetLength(&count);
    if (count >= 1)
    {
      rv = nodeList->Item(0, getter_AddRefs(baseNode));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  // If no base tag, then set baseURL to the document's URL
  // This is very important, else relative URLs for links and images are wrong
  if (!baseNode)
  {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

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

NS_IMETHODIMP nsHTMLEditor::TabInTable(bool inIsShift, bool *outHandled)
{
  NS_ENSURE_TRUE(outHandled, NS_ERROR_NULL_POINTER);
  *outHandled = false;

  // Find enclosing table cell from the selection (cell may be the selected element)
  nsCOMPtr<nsIDOMElement> cellElement;
    // can't use |NS_LITERAL_STRING| here until |GetElementOrParentByTagName| is fixed to accept readables
  nsresult res = GetElementOrParentByTagName(NS_LITERAL_STRING("td"), nsnull, getter_AddRefs(cellElement));
  NS_ENSURE_SUCCESS(res, res);
  // Do nothing -- we didn't find a table cell
  NS_ENSURE_TRUE(cellElement, NS_OK);

  // find enclosing table
  nsCOMPtr<nsIDOMNode> tbl = GetEnclosingTable(cellElement);
  NS_ENSURE_TRUE(tbl, res);

  // advance to next cell
  // first create an iterator over the table
  nsCOMPtr<nsIContentIterator> iter =
      do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(iter, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIContent> cTbl = do_QueryInterface(tbl);
  nsCOMPtr<nsIContent> cBlock = do_QueryInterface(cellElement);
  res = iter->Init(cTbl);
  NS_ENSURE_SUCCESS(res, res);
  // position iter at block
  res = iter->PositionAt(cBlock);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMNode> node;
  do
  {
    if (inIsShift)
      iter->Prev();
    else
      iter->Next();

    node = do_QueryInterface(iter->GetCurrentNode());

    if (node && nsHTMLEditUtils::IsTableCell(node) &&
        GetEnclosingTable(node) == tbl)
    {
      res = CollapseSelectionToDeepestNonTableFirstChild(nsnull, node);
      NS_ENSURE_SUCCESS(res, res);
      *outHandled = true;
      return NS_OK;
    }
  } while (!iter->IsDone());
  
  if (!(*outHandled) && !inIsShift)
  {
    // if we havent handled it yet then we must have run off the end of
    // the table.  Insert a new row.
    res = InsertTableRow(1, true);
    NS_ENSURE_SUCCESS(res, res);
    *outHandled = true;
    // put selection in right place
    // Use table code to get selection and index to new row...
    nsCOMPtr<nsISelection>selection;
    nsCOMPtr<nsIDOMElement> tblElement;
    nsCOMPtr<nsIDOMElement> cell;
    PRInt32 row;
    res = GetCellContext(getter_AddRefs(selection), 
                         getter_AddRefs(tblElement),
                         getter_AddRefs(cell), 
                         nsnull, nsnull,
                         &row, nsnull);
    NS_ENSURE_SUCCESS(res, res);
    // ...so that we can ask for first cell in that row...
    res = GetCellAt(tblElement, row, 0, getter_AddRefs(cell));
    NS_ENSURE_SUCCESS(res, res);
    // ...and then set selection there.
    // (Note that normally you should use CollapseSelectionToDeepestNonTableFirstChild(),
    //  but we know cell is an empty new cell, so this works fine)
    node = do_QueryInterface(cell);
    if (node) selection->Collapse(node,0);
    return NS_OK;
  }
  
  return res;
}

NS_IMETHODIMP nsHTMLEditor::CreateBR(nsIDOMNode *aNode, PRInt32 aOffset, nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect)
{
  nsCOMPtr<nsIDOMNode> parent = aNode;
  PRInt32 offset = aOffset;
  return CreateBRImpl(address_of(parent), &offset, outBRNode, aSelect);
}

nsresult 
nsHTMLEditor::CollapseSelectionToDeepestNonTableFirstChild(nsISelection *aSelection, nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  nsresult res;

  nsCOMPtr<nsISelection> selection;
  if (aSelection)
  {
    selection = aSelection;
  } else {
    res = GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  }
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMNode> child;
  
  do {
    node->GetFirstChild(getter_AddRefs(child));
    
    if (child)
    {
      // Stop if we find a table
      // don't want to go into nested tables
      if (nsHTMLEditUtils::IsTable(child)) break;
      // hey, it'g gotta be a container too!
      if (!IsContainer(child)) break;
      node = child;
    }
  }
  while (child);

  selection->Collapse(node,0);
  return NS_OK;
}


// This is mostly like InsertHTMLWithCharsetAndContext, 
//  but we can't use that because it is selection-based and 
//  the rules code won't let us edit under the <head> node
NS_IMETHODIMP
nsHTMLEditor::ReplaceHeadContentsWithHTML(const nsAString& aSourceToInsert)
{
  nsAutoRules beginRulesSniffing(this, kOpIgnore, nsIEditor::eNone); // don't do any post processing, rules get confused
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  ForceCompositionEnd();

  // Do not use nsAutoRules -- rules code won't let us insert in <head>
  // Use the head node as a parent and delete/insert directly
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMNodeList>nodeList; 
  res = doc->GetElementsByTagName(NS_LITERAL_STRING("head"), getter_AddRefs(nodeList));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(nodeList, NS_ERROR_NULL_POINTER);

  PRUint32 count; 
  nodeList->GetLength(&count);
  if (count < 1) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> headNode;
  res = nodeList->Item(0, getter_AddRefs(headNode)); 
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(headNode, NS_ERROR_NULL_POINTER);

  // First, make sure there are no return chars in the source.
  // Bad things happen if you insert returns (instead of dom newlines, \n)
  // into an editor document.
  nsAutoString inputString (aSourceToInsert);  // hope this does copy-on-write
 
  // Windows linebreaks: Map CRLF to LF:
  inputString.ReplaceSubstring(NS_LITERAL_STRING("\r\n").get(),
                               NS_LITERAL_STRING("\n").get());
 
  // Mac linebreaks: Map any remaining CR to LF:
  inputString.ReplaceSubstring(NS_LITERAL_STRING("\r").get(),
                               NS_LITERAL_STRING("\n").get());

  nsAutoEditBatch beginBatching(this);

  res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // Get the first range in the selection, for context:
  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMDocumentFragment> docfrag;
  res = range->CreateContextualFragment(inputString,
                                        getter_AddRefs(docfrag));

  //XXXX BUG 50965: This is not returning the text between <title> ... </title>
  // Special code is needed in JS to handle title anyway, so it really doesn't matter!

  if (NS_FAILED(res))
  {
#ifdef DEBUG
    printf("Couldn't create contextual fragment: error was %d\n", res);
#endif
    return res;
  }
  NS_ENSURE_TRUE(docfrag, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> child;

  // First delete all children in head
  do {
    res = headNode->GetFirstChild(getter_AddRefs(child));
    NS_ENSURE_SUCCESS(res, res);
    if (child)
    {
      res = DeleteNode(child);
      NS_ENSURE_SUCCESS(res, res);
    }
  } while (child);

  // Now insert the new nodes
  PRInt32 offsetOfNewNode = 0;
  nsCOMPtr<nsIDOMNode> fragmentAsNode (do_QueryInterface(docfrag));

  // Loop over the contents of the fragment and move into the document
  do {
    res = fragmentAsNode->GetFirstChild(getter_AddRefs(child));
    NS_ENSURE_SUCCESS(res, res);
    if (child)
    {
      res = InsertNode(child, headNode, offsetOfNewNode++);
      NS_ENSURE_SUCCESS(res, res);
    }
  } while (child);

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::RebuildDocumentFromSource(const nsAString& aSourceString)
{
  ForceCompositionEnd();

  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMElement> bodyElement = do_QueryInterface(GetRoot());
  NS_ENSURE_TRUE(bodyElement, NS_ERROR_NULL_POINTER);

  // Find where the <body> tag starts.
  nsReadingIterator<PRUnichar> beginbody;
  nsReadingIterator<PRUnichar> endbody;
  aSourceString.BeginReading(beginbody);
  aSourceString.EndReading(endbody);
  bool foundbody = CaseInsensitiveFindInReadable(NS_LITERAL_STRING("<body"),
                                                   beginbody, endbody);

  nsReadingIterator<PRUnichar> beginhead;
  nsReadingIterator<PRUnichar> endhead;
  aSourceString.BeginReading(beginhead);
  aSourceString.EndReading(endhead);
  bool foundhead = CaseInsensitiveFindInReadable(NS_LITERAL_STRING("<head"),
                                                   beginhead, endhead);

  nsReadingIterator<PRUnichar> beginclosehead;
  nsReadingIterator<PRUnichar> endclosehead;
  aSourceString.BeginReading(beginclosehead);
  aSourceString.EndReading(endclosehead);

  // Find the index after "<head>"
  bool foundclosehead = CaseInsensitiveFindInReadable(
           NS_LITERAL_STRING("</head>"), beginclosehead, endclosehead);
  
  // Time to change the document
  nsAutoEditBatch beginBatching(this);

  nsReadingIterator<PRUnichar> endtotal;
  aSourceString.EndReading(endtotal);

  if (foundhead) {
    if (foundclosehead)
      res = ReplaceHeadContentsWithHTML(Substring(beginhead, beginclosehead));
    else if (foundbody)
      res = ReplaceHeadContentsWithHTML(Substring(beginhead, beginbody));
    else
      // XXX Without recourse to some parser/content sink/docshell hackery
      // we don't really know where the head ends and the body begins
      // so we assume that there is no body
      res = ReplaceHeadContentsWithHTML(Substring(beginhead, endtotal));
  } else {
    nsReadingIterator<PRUnichar> begintotal;
    aSourceString.BeginReading(begintotal);
    NS_NAMED_LITERAL_STRING(head, "<head>");
    if (foundclosehead)
      res = ReplaceHeadContentsWithHTML(head + Substring(begintotal, beginclosehead));
    else if (foundbody)
      res = ReplaceHeadContentsWithHTML(head + Substring(begintotal, beginbody));
    else
      // XXX Without recourse to some parser/content sink/docshell hackery
      // we don't really know where the head ends and the body begins
      // so we assume that there is no head
      res = ReplaceHeadContentsWithHTML(head);
  }
  NS_ENSURE_SUCCESS(res, res);

  res = SelectAll();
  NS_ENSURE_SUCCESS(res, res);

  if (!foundbody) {
    NS_NAMED_LITERAL_STRING(body, "<body>");
    // XXX Without recourse to some parser/content sink/docshell hackery
    // we don't really know where the head ends and the body begins
    if (foundclosehead) // assume body starts after the head ends
      res = LoadHTML(body + Substring(endclosehead, endtotal));
    else if (foundhead) // assume there is no body
      res = LoadHTML(body);
    else // assume there is no head, the entire source is body
      res = LoadHTML(body + aSourceString);
    NS_ENSURE_SUCCESS(res, res);

    nsCOMPtr<nsIDOMElement> divElement;
    res = CreateElementWithDefaults(NS_LITERAL_STRING("div"), getter_AddRefs(divElement));
    NS_ENSURE_SUCCESS(res, res);

    res = CloneAttributes(bodyElement, divElement);
    NS_ENSURE_SUCCESS(res, res);

    return BeginningOfDocument();
  }

  res = LoadHTML(Substring(beginbody, endtotal));
  NS_ENSURE_SUCCESS(res, res);

  // Now we must copy attributes user might have edited on the <body> tag
  //  because InsertHTML (actually, CreateContextualFragment()) 
  //  will never return a body node in the DOM fragment
  
  // We already know where "<body" begins
  nsReadingIterator<PRUnichar> beginclosebody = beginbody;
  nsReadingIterator<PRUnichar> endclosebody;
  aSourceString.EndReading(endclosebody);
  if (!FindInReadable(NS_LITERAL_STRING(">"),beginclosebody,endclosebody))
    return NS_ERROR_FAILURE;

  // Truncate at the end of the body tag
  // Kludge of the year: fool the parser by replacing "body" with "div" so we get a node
  nsAutoString bodyTag;
  bodyTag.AssignLiteral("<div ");
  bodyTag.Append(Substring(endbody, endclosebody));

  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMDocumentFragment> docfrag;
  res = range->CreateContextualFragment(bodyTag, getter_AddRefs(docfrag));
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMNode> fragmentAsNode (do_QueryInterface(docfrag));
  NS_ENSURE_TRUE(fragmentAsNode, NS_ERROR_NULL_POINTER);
  
  nsCOMPtr<nsIDOMNode> child;
  res = fragmentAsNode->GetFirstChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(child, NS_ERROR_NULL_POINTER);
  
  // Copy all attributes from the div child to current body element
  res = CloneAttributes(bodyElement, child);
  NS_ENSURE_SUCCESS(res, res);
  
  // place selection at first editable content
  return BeginningOfDocument();
}

void
nsHTMLEditor::NormalizeEOLInsertPosition(nsIDOMNode *firstNodeToInsert,
                                     nsCOMPtr<nsIDOMNode> *insertParentNode,
                                     PRInt32 *insertOffset)
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
  nsCOMPtr<nsIDOMNode> nextVisNode;
  nsCOMPtr<nsIDOMNode> prevVisNode;
  PRInt32 nextVisOffset=0;
  PRInt16 nextVisType=0;
  PRInt32 prevVisOffset=0;
  PRInt16 prevVisType=0;

  wsObj.NextVisibleNode(*insertParentNode, *insertOffset, address_of(nextVisNode), &nextVisOffset, &nextVisType);
  if (!nextVisNode)
    return;

  if (! (nextVisType & nsWSRunObject::eBreak))
    return;

  wsObj.PriorVisibleNode(*insertParentNode, *insertOffset, address_of(prevVisNode), &prevVisOffset, &prevVisType);
  if (!prevVisNode)
    return;

  if (prevVisType & nsWSRunObject::eBreak)
    return;

  if (prevVisType & nsWSRunObject::eThisBlock)
    return;

  nsCOMPtr<nsIDOMNode> brNode;
  PRInt32 brOffset=0;

  GetNodeLocation(nextVisNode, address_of(brNode), &brOffset);

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
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);

  nsRefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  // hand off to the rules system, see if it has anything to say about this
  bool cancel, handled;
  nsTextRulesInfo ruleInfo(kOpInsertElement);
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

      nsCOMPtr<nsIDOMNode> tempNode;
      PRInt32 tempOffset;
      nsresult result = DeleteSelectionAndPrepareToCreateNode(tempNode,tempOffset);
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
    PRInt32 offsetForInsert;
    res = selection->GetAnchorNode(getter_AddRefs(parentSelectedNode));
    // XXX: ERROR_HANDLING bad XPCOM usage
    if (NS_SUCCEEDED(res) && NS_SUCCEEDED(selection->GetAnchorOffset(&offsetForInsert)) && parentSelectedNode)
    {
#ifdef DEBUG_cmanske
      {
      nsAutoString name;
      parentSelectedNode->GetNodeName(name);
      printf("InsertElement: Anchor node of selection: ");
      wprintf(name.get());
      printf(" Offset: %d\n", offsetForInsert);
      }
#endif

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
              PRInt32               *ioOffset        insertion offset
              bool                  aNoEmptyNodes    splitting can result in empty nodes?
*/
nsresult
nsHTMLEditor::InsertNodeAtPoint(nsIDOMNode *aNode, 
                                nsCOMPtr<nsIDOMNode> *ioParent, 
                                PRInt32 *ioOffset, 
                                bool aNoEmptyNodes)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(ioParent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(*ioParent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(ioOffset, NS_ERROR_NULL_POINTER);
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> parent = *ioParent;
  nsCOMPtr<nsIDOMNode> topChild = *ioParent;
  nsCOMPtr<nsIDOMNode> tmp;
  PRInt32 offsetOfInsert = *ioOffset;
   
  // Search up the parent chain to find a suitable container      
  while (!CanContain(parent, aNode)) {
    // If the current parent is a root (body or table element)
    // then go no further - we can't insert
    if (nsTextEditUtils::IsBody(parent) || nsHTMLEditUtils::IsTableElement(parent))
      return NS_ERROR_FAILURE;
    // Get the next parent
    parent->GetParentNode(getter_AddRefs(tmp));
    NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
    topChild = parent;
    parent = tmp;
  }
  if (parent != topChild)
  {
    // we need to split some levels above the original selection parent
    res = SplitNodeDeep(topChild, *ioParent, *ioOffset, &offsetOfInsert, aNoEmptyNodes);
    NS_ENSURE_SUCCESS(res, res);
    *ioParent = parent;
    *ioOffset = offsetOfInsert;
  }
  // Now we can insert the new node
  res = InsertNode(aNode, parent, offsetOfInsert);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SelectElement(nsIDOMElement* aElement)
{
  nsresult res = NS_ERROR_NULL_POINTER;

  // Must be sure that element is contained in the document body
  if (IsDescendantOfEditorRoot(aElement)) {
    nsCOMPtr<nsISelection> selection;
    res = GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    nsCOMPtr<nsIDOMNode>parent;
    res = aElement->GetParentNode(getter_AddRefs(parent));
    if (NS_SUCCEEDED(res) && parent)
    {
      PRInt32 offsetInParent;
      res = GetChildOffset(aElement, parent, offsetInParent);

      if (NS_SUCCEEDED(res))
      {
        // Collapse selection to just before desired element,
        res = selection->Collapse(parent, offsetInParent);
        if (NS_SUCCEEDED(res)) {
          //  then extend it to just after
          res = selection->Extend(parent, offsetInParent+1);
        }
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
    nsCOMPtr<nsISelection> selection;
    res = GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    nsCOMPtr<nsIDOMNode>parent;
    res = aElement->GetParentNode(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
    PRInt32 offsetInParent;
    res = GetChildOffset(aElement, parent, offsetInParent);
    if (NS_SUCCEEDED(res))
    {
      // Collapse selection to just after desired element,
      res = selection->Collapse(parent, offsetInParent+1);
#if 0 //def DEBUG_cmanske
      {
      nsAutoString name;
      parent->GetNodeName(name);
      printf("SetCaretAfterElement: Parent node: ");
      wprintf(name.get());
      printf(" Offset: %d\n\nHTML:\n", offsetInParent+1);
      nsAutoString Format("text/html");
      nsAutoString ContentsAs;
      OutputToString(Format, 2, ContentsAs);
      wprintf(ContentsAs.get());
      }
#endif
    }
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
  nsHTMLEditRules* htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());
  NS_ENSURE_TRUE(htmlRules, NS_ERROR_FAILURE);
  
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
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);

  // get selection location
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = GetStartNodeAndOffset(selection, getter_AddRefs(parent), &offset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  // is the selection collapsed?
  nsCOMPtr<nsIDOMNode> nodeToExamine;
  if (selection->Collapsed() || IsTextNode(parent)) {
    // we want to look at the parent and ancestors
    nodeToExamine = parent;
  } else {
    // otherwise we want to look at the first editable node after
    // {parent,offset} and its ancestors for divs with alignment on them
    nodeToExamine = GetChildAt(parent, offset);
    //GetNextNode(parent, offset, true, address_of(nodeToExamine));
  }
  
  NS_ENSURE_TRUE(nodeToExamine, NS_ERROR_NULL_POINTER);

  // is the node to examine a block ?
  bool isBlock;
  res = NodeIsBlockStatic(nodeToExamine, &isBlock);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMNode> tmp;

  if (aBlockLevel) {
    // we are querying the block background (and not the text background), let's
    // climb to the block container
    nsCOMPtr<nsIDOMNode> blockParent = nodeToExamine;
    if (!isBlock) {
      blockParent = GetBlockNodeParent(nodeToExamine);
      NS_ENSURE_TRUE(blockParent, NS_OK);
    }

    // Make sure to not walk off onto the Document node
    nsCOMPtr<nsIDOMElement> element;
    do {
      // retrieve the computed style of background-color for blockParent
      mHTMLCSSUtils->GetComputedProperty(blockParent,
                                         nsEditProperty::cssBackgroundColor,
                                         aOutColor);
      tmp.swap(blockParent);
      res = tmp->GetParentNode(getter_AddRefs(blockParent));
      element = do_QueryInterface(blockParent);
      // look at parent if the queried color is transparent and if the node to
      // examine is not the root of the document
    } while (aOutColor.EqualsLiteral("transparent") && element);
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
      res = nodeToExamine->GetParentNode(getter_AddRefs(parent));
      NS_ENSURE_SUCCESS(res, res);
      nodeToExamine = parent;
    }
    do {
      // is the node to examine a block ?
      res = NodeIsBlockStatic(nodeToExamine, &isBlock);
      NS_ENSURE_SUCCESS(res, res);
      if (isBlock) {
        // yes it is a block; in that case, the text background color is transparent
        aOutColor.AssignLiteral("transparent");
        break;
      }
      else {
        // no, it's not; let's retrieve the computed style of background-color for the
        // node to examine
        mHTMLCSSUtils->GetComputedProperty(nodeToExamine, nsEditProperty::cssBackgroundColor,
                            aOutColor);
        if (!aOutColor.EqualsLiteral("transparent")) {
          break;
        }
      }
      tmp.swap(nodeToExamine);
      res = tmp->GetParentNode(getter_AddRefs(nodeToExamine));
      NS_ENSURE_SUCCESS(res, res);
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
  PRInt32 selectedCount;
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
    if (element->IsHTML(nsGkAtoms::body)) {
      return NS_OK;
    }

    // No color is set, but we need to report visible color inherited 
    // from nested cells/tables, so search up parent chain
    element = element->GetElementParent();
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
  nsHTMLEditRules* htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());
  NS_ENSURE_TRUE(htmlRules, NS_ERROR_FAILURE);
  
  return htmlRules->GetListState(aMixed, aOL, aUL, aDL);
}

NS_IMETHODIMP 
nsHTMLEditor::GetListItemState(bool *aMixed, bool *aLI, bool *aDT, bool *aDD)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  NS_ENSURE_TRUE(aMixed && aLI && aDT && aDD, NS_ERROR_NULL_POINTER);

  nsHTMLEditRules* htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());
  NS_ENSURE_TRUE(htmlRules, NS_ERROR_FAILURE);
  
  return htmlRules->GetListItemState(aMixed, aLI, aDT, aDD);
}

NS_IMETHODIMP
nsHTMLEditor::GetAlignment(bool *aMixed, nsIHTMLEditor::EAlignment *aAlign)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  NS_ENSURE_TRUE(aMixed && aAlign, NS_ERROR_NULL_POINTER);
  nsHTMLEditRules* htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());
  NS_ENSURE_TRUE(htmlRules, NS_ERROR_FAILURE);
  
  return htmlRules->GetAlignment(aMixed, aAlign);
}


NS_IMETHODIMP 
nsHTMLEditor::GetIndentState(bool *aCanIndent, bool *aCanOutdent)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  NS_ENSURE_TRUE(aCanIndent && aCanOutdent, NS_ERROR_NULL_POINTER);

  nsHTMLEditRules* htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());
  NS_ENSURE_TRUE(htmlRules, NS_ERROR_FAILURE);
  
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
  nsAutoRules beginRulesSniffing(this, kOpMakeList, nsIEditor::eNext);
  
  // pre-process
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsTextRulesInfo ruleInfo(kOpMakeList);
  ruleInfo.blockType = &aListType;
  ruleInfo.entireList = entireList;
  ruleInfo.bulletType = &aBulletType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // Find out if the selection is collapsed:
    bool isCollapsed = selection->Collapsed();

    nsCOMPtr<nsIDOMNode> node;
    PRInt32 offset;
    res = GetStartNodeAndOffset(selection, getter_AddRefs(node), &offset);
    if (!node) res = NS_ERROR_FAILURE;
    NS_ENSURE_SUCCESS(res, res);
  
    if (isCollapsed)
    {
      // have to find a place to put the list
      nsCOMPtr<nsIDOMNode> parent = node;
      nsCOMPtr<nsIDOMNode> topChild = node;
      nsCOMPtr<nsIDOMNode> tmp;
    
      nsCOMPtr<nsIAtom> listAtom = do_GetAtom(aListType);
      while (!CanContainTag(parent, listAtom)) {
        parent->GetParentNode(getter_AddRefs(tmp));
        NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
        topChild = parent;
        parent = tmp;
      }
    
      if (parent != node)
      {
        // we need to split up to the child of parent
        res = SplitNodeDeep(topChild, node, offset, &offset);
        NS_ENSURE_SUCCESS(res, res);
      }

      // make a list
      nsCOMPtr<nsIDOMNode> newList;
      res = CreateNode(aListType, parent, offset, getter_AddRefs(newList));
      NS_ENSURE_SUCCESS(res, res);
      // make a list item
      nsCOMPtr<nsIDOMNode> newItem;
      res = CreateNode(NS_LITERAL_STRING("li"), newList, 0, getter_AddRefs(newItem));
      NS_ENSURE_SUCCESS(res, res);
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
  nsAutoRules beginRulesSniffing(this, kOpRemoveList, nsIEditor::eNext);
  
  // pre-process
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsTextRulesInfo ruleInfo(kOpRemoveList);
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
  nsAutoRules beginRulesSniffing(this, kOpMakeDefListItem, nsIEditor::eNext);
  
  // pre-process
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsTextRulesInfo ruleInfo(kOpMakeDefListItem);
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
  nsAutoRules beginRulesSniffing(this, kOpMakeBasicBlock, nsIEditor::eNext);
  
  // pre-process
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsTextRulesInfo ruleInfo(kOpMakeBasicBlock);
  ruleInfo.blockType = &aBlockType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // Find out if the selection is collapsed:
    bool isCollapsed = selection->Collapsed();

    nsCOMPtr<nsIDOMNode> node;
    PRInt32 offset;
    res = GetStartNodeAndOffset(selection, getter_AddRefs(node), &offset);
    if (!node) res = NS_ERROR_FAILURE;
    NS_ENSURE_SUCCESS(res, res);
  
    if (isCollapsed)
    {
      // have to find a place to put the block
      nsCOMPtr<nsIDOMNode> parent = node;
      nsCOMPtr<nsIDOMNode> topChild = node;
      nsCOMPtr<nsIDOMNode> tmp;
    
      nsCOMPtr<nsIAtom> blockAtom = do_GetAtom(aBlockType);
      while (!CanContainTag(parent, blockAtom)) {
        parent->GetParentNode(getter_AddRefs(tmp));
        NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
        topChild = parent;
        parent = tmp;
      }
    
      if (parent != node)
      {
        // we need to split up to the child of parent
        res = SplitNodeDeep(topChild, node, offset, &offset);
        NS_ENSURE_SUCCESS(res, res);
      }

      // make a block
      nsCOMPtr<nsIDOMNode> newBlock;
      res = CreateNode(aBlockType, parent, offset, getter_AddRefs(newBlock));
      NS_ENSURE_SUCCESS(res, res);
    
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
  OperationID opID = kOpIndent;
  if (aIndent.LowerCaseEqualsLiteral("outdent"))
  {
    opID = kOpOutdent;
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
    nsCOMPtr<nsIDOMNode> node;
    PRInt32 offset;
    bool isCollapsed = selection->Collapsed();

    res = GetStartNodeAndOffset(selection, getter_AddRefs(node), &offset);
    if (!node) res = NS_ERROR_FAILURE;
    NS_ENSURE_SUCCESS(res, res);
  
    if (aIndent.EqualsLiteral("indent"))
    {
      if (isCollapsed)
      {
        // have to find a place to put the blockquote
        nsCOMPtr<nsIDOMNode> parent = node;
        nsCOMPtr<nsIDOMNode> topChild = node;
        nsCOMPtr<nsIDOMNode> tmp;
        while (!CanContainTag(parent, nsGkAtoms::blockquote)) {
          parent->GetParentNode(getter_AddRefs(tmp));
          NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
          topChild = parent;
          parent = tmp;
        }
    
        if (parent != node)
        {
          // we need to split up to the child of parent
          res = SplitNodeDeep(topChild, node, offset, &offset);
          NS_ENSURE_SUCCESS(res, res);
        }

        // make a blockquote
        nsCOMPtr<nsIDOMNode> newBQ;
        res = CreateNode(NS_LITERAL_STRING("blockquote"), parent, offset,
                         getter_AddRefs(newBQ));
        NS_ENSURE_SUCCESS(res, res);
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
  nsAutoRules beginRulesSniffing(this, kOpAlign, nsIEditor::eNext);

  nsCOMPtr<nsIDOMNode> node;
  bool cancel, handled;
  
  // Find out if the selection is collapsed:
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsTextRulesInfo ruleInfo(kOpAlign);
  ruleInfo.alignType = &aAlignType;
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(res))
    return res;
  
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetElementOrParentByTagName(const nsAString& aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn)
{
  if (aTagName.IsEmpty() || !aReturn )
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> currentNode;

  if (aNode)
    currentNode = aNode;
  else
  {
    // If no node supplied, get it from anchor node of current selection
    nsCOMPtr<nsISelection>selection;
    res = GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

    nsCOMPtr<nsIDOMNode> anchorNode;
    res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
    if(NS_FAILED(res)) return res;
    NS_ENSURE_TRUE(anchorNode, NS_ERROR_FAILURE);

    // Try to get the actual selected node
    bool hasChildren = false;
    anchorNode->HasChildNodes(&hasChildren);
    if (hasChildren)
    {
      PRInt32 offset;
      res = selection->GetAnchorOffset(&offset);
      if(NS_FAILED(res)) return res;
      currentNode = nsEditor::GetChildAt(anchorNode, offset);
    }
    // anchor node is probably a text node - just use that
    if (!currentNode)
      currentNode = anchorNode;
  }
   
  nsAutoString TagName(aTagName);
  ToLowerCase(TagName);
  bool getLink = IsLinkTag(TagName);
  bool getNamedAnchor = IsNamedAnchorTag(TagName);
  if ( getLink || getNamedAnchor)
  {
    TagName.AssignLiteral("a");  
  }
  bool findTableCell = TagName.EqualsLiteral("td");
  bool findList = TagName.EqualsLiteral("list");

  // default is null - no element found
  *aReturn = nsnull;
  
  nsCOMPtr<nsIDOMNode> parent;
  bool bNodeFound = false;

  while (true)
  {
    nsAutoString currentTagName; 
    // Test if we have a link (an anchor with href set)
    if ( (getLink && nsHTMLEditUtils::IsLink(currentNode)) ||
         (getNamedAnchor && nsHTMLEditUtils::IsNamedAnchor(currentNode)) )
    {
      bNodeFound = true;
      break;
    } else {
      if (findList)
      {
        // Match "ol", "ul", or "dl" for lists
        if (nsHTMLEditUtils::IsList(currentNode))
          goto NODE_FOUND;

      } else if (findTableCell)
      {
        // Table cells are another special case:
        // Match either "td" or "th" for them
        if (nsHTMLEditUtils::IsTableCell(currentNode))
          goto NODE_FOUND;

      } else {
        currentNode->GetNodeName(currentTagName);
        if (currentTagName.Equals(TagName, nsCaseInsensitiveStringComparator()))
        {
NODE_FOUND:
          bNodeFound = true;
          break;
        } 
      }
    }
    // Search up the parent chain
    // We should never fail because of root test below, but lets be safe
    // XXX: ERROR_HANDLING error return code lost
    if (NS_FAILED(currentNode->GetParentNode(getter_AddRefs(parent))) || !parent)
      break;

    // Stop searching if parent is a body tag
    nsAutoString parentTagName;
    parent->GetNodeName(parentTagName);
    // Note: Originally used IsRoot to stop at table cells,
    //  but that's too messy when you are trying to find the parent table
    if(parentTagName.LowerCaseEqualsLiteral("body"))
      break;

    currentNode = parent;
  }
  if (bNodeFound)
  {
    nsCOMPtr<nsIDOMElement> currentElement = do_QueryInterface(currentNode);
    if (currentElement)
    {
      *aReturn = currentElement;
      // Getters must addref
      NS_ADDREF(*aReturn);
    }
  }
  else res = NS_EDITOR_ELEMENT_NOT_FOUND;

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetSelectedElement(const nsAString& aTagName, nsIDOMElement** aReturn)
{
  NS_ENSURE_TRUE(aReturn , NS_ERROR_NULL_POINTER);
  
  // default is null - no element found
  *aReturn = nsnull;
  
  // First look for a single element in selection
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

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
  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMNode> startParent;
  PRInt32 startOffset, endOffset;
  res = range->GetStartContainer(getter_AddRefs(startParent));
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
      PRInt32 anchorOffset = -1;
      if (anchorNode)
        selection->GetAnchorOffset(&anchorOffset);
    
      nsCOMPtr<nsIDOMNode> focusNode;
      res = selection->GetFocusNode(getter_AddRefs(focusNode));
      NS_ENSURE_SUCCESS(res, res);
      PRInt32 focusOffset = -1;
      if (focusNode)
        selection->GetFocusOffset(&focusOffset);

      // Link node must be the same for both ends of selection
      if (NS_SUCCEEDED(res) && anchorNode)
      {
  #ifdef DEBUG_cmanske
        {
        nsAutoString name;
        anchorNode->GetNodeName(name);
        printf("GetSelectedElement: Anchor node of selection: ");
        wprintf(name.get());
        printf(" Offset: %d\n", anchorOffset);
        focusNode->GetNodeName(name);
        printf("Focus node of selection: ");
        wprintf(name.get());
        printf(" Offset: %d\n", focusOffset);
        }
  #endif
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
      nsCOMPtr<nsIEnumerator> enumerator;
      res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
      if (NS_SUCCEEDED(res))
      {
        if(!enumerator)
          return NS_ERROR_NULL_POINTER;

        enumerator->First(); 
        nsCOMPtr<nsISupports> currentItem;
        res = enumerator->CurrentItem(getter_AddRefs(currentItem));
        if ((NS_SUCCEEDED(res)) && currentItem)
        {
          nsCOMPtr<nsIDOMRange> currange( do_QueryInterface(currentItem) );
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
          printf("isCollapsed was FALSE, but no elements found in selection\n");
        }
      } else {
        printf("Could not create enumerator for GetSelectionProperties\n");
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

NS_IMETHODIMP
nsHTMLEditor::CreateElementWithDefaults(const nsAString& aTagName, nsIDOMElement** aReturn)
{
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  if (aReturn)
    *aReturn = nsnull;

//  NS_ENSURE_TRUE(aTagName && aReturn, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(!aTagName.IsEmpty() && aReturn, NS_ERROR_NULL_POINTER);
    
  nsAutoString TagName(aTagName);
  ToLowerCase(TagName);
  nsAutoString realTagName;

  if (IsLinkTag(TagName) || IsNamedAnchorTag(TagName))
  {
    realTagName.AssignLiteral("a");
  } else {
    realTagName = TagName;
  }
  //We don't use editor's CreateElement because we don't want to 
  //  go through the transaction system

  nsCOMPtr<nsIDOMElement>newElement;
  nsCOMPtr<dom::Element> newContent;
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

  //new call to use instead to get proper HTML element, bug# 39919
  res = CreateHTMLContent(realTagName, getter_AddRefs(newContent));
  newElement = do_QueryInterface(newContent);
  if (NS_FAILED(res) || !newElement)
    return NS_ERROR_FAILURE;

  // Mark the new element dirty, so it will be formatted
  newElement->SetAttribute(NS_LITERAL_STRING("_moz_dirty"), EmptyString());

  // Set default values for new elements
  if (TagName.EqualsLiteral("table")) {
    res = newElement->SetAttribute(NS_LITERAL_STRING("cellpadding"),NS_LITERAL_STRING("2"));
    NS_ENSURE_SUCCESS(res, res);
    res = newElement->SetAttribute(NS_LITERAL_STRING("cellspacing"),NS_LITERAL_STRING("2"));
    NS_ENSURE_SUCCESS(res, res);
    res = newElement->SetAttribute(NS_LITERAL_STRING("border"),NS_LITERAL_STRING("1"));
  } else if (TagName.EqualsLiteral("td"))
  {
    res = SetAttributeOrEquivalent(newElement, NS_LITERAL_STRING("valign"),
                                   NS_LITERAL_STRING("top"), true);
  }
  // ADD OTHER TAGS HERE

  if (NS_SUCCEEDED(res))
  {
    *aReturn = newElement;
    // Getters must addref
    NS_ADDREF(*aReturn);
  }

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
  NS_ENSURE_TRUE(aAnchorElement, NS_ERROR_NULL_POINTER);

  // We must have a real selection
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (!selection)
  {
    res = NS_ERROR_NULL_POINTER;
  }
  NS_ENSURE_SUCCESS(res, res);
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
  res = anchor->GetHref(href);
  NS_ENSURE_SUCCESS(res, res);
  if (href.IsEmpty()) {
    return NS_OK;
  }

  nsAutoEditBatch beginBatching(this);

  // Set all attributes found on the supplied anchor element
  nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
  aAnchorElement->GetAttributes(getter_AddRefs(attrMap));
  NS_ENSURE_TRUE(attrMap, NS_ERROR_FAILURE);

  PRUint32 count;
  attrMap->GetLength(&count);
  nsAutoString name, value;

  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMNode> attrNode;
    res = attrMap->Item(i, getter_AddRefs(attrNode));
    NS_ENSURE_SUCCESS(res, res);

    nsCOMPtr<nsIDOMAttr> attribute = do_QueryInterface(attrNode);
    if (attribute) {
      // We must clear the string buffers
      //   because GetName, GetValue appends to previous string!
      name.Truncate();
      value.Truncate();

      res = attribute->GetName(name);
      NS_ENSURE_SUCCESS(res, res);

      res = attribute->GetValue(value);
      NS_ENSURE_SUCCESS(res, res);

      res = SetInlineProperty(nsEditProperty::a, name, value);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetHTMLBackgroundColor(const nsAString& aColor)
{
  NS_PRECONDITION(mDocWeak, "Missing Editor DOM Document");
  
  // Find a selected or enclosing table element to set background on
  nsCOMPtr<nsIDOMElement> element;
  PRInt32 selectedCount;
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
      res = GetFirstSelectedCell(nsnull, getter_AddRefs(cell));
      if (NS_SUCCEEDED(res) && cell)
      {
        while(cell)
        {
          if (setColor)
            res = SetAttribute(cell, bgcolor, aColor);
          else
            res = RemoveAttribute(cell, bgcolor);
          if (NS_FAILED(res)) break;

          GetNextSelectedCell(nsnull, getter_AddRefs(cell));
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
    LoadSheet(uaURI, nsnull, EmptyCString(), this);
}

NS_IMETHODIMP
nsHTMLEditor::RemoveStyleSheet(const nsAString &aURL)
{
  nsRefPtr<nsCSSStyleSheet> sheet;
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
  nsRefPtr<nsCSSStyleSheet> sheet;
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
  nsRefPtr<nsCSSStyleSheet> sheet;
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
  nsRefPtr<nsCSSStyleSheet> sheet;
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
  nsRefPtr<nsCSSStyleSheet> sheet;
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
                                     nsCSSStyleSheet *aStyleSheet)
{
  PRUint32 countSS = mStyleSheets.Length();
  PRUint32 countU = mStyleSheetURLs.Length();

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
  PRUint32 foundIndex;
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
                                  nsCSSStyleSheet **aStyleSheet)
{
  NS_ENSURE_ARG_POINTER(aStyleSheet);
  *aStyleSheet = 0;

  // is it already in the list?
  PRUint32 foundIndex;
  foundIndex = mStyleSheetURLs.IndexOf(aURL);
  if (foundIndex == mStyleSheetURLs.NoIndex)
    return NS_OK; //No sheet -- don't fail!

  *aStyleSheet = mStyleSheets[foundIndex];
  NS_ENSURE_TRUE(*aStyleSheet, NS_ERROR_FAILURE);

  NS_ADDREF(*aStyleSheet);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetURLForStyleSheet(nsCSSStyleSheet *aStyleSheet,
                                  nsAString &aURL)
{
  // is it already in the list?
  PRInt32 foundIndex = mStyleSheets.IndexOf(aStyleSheet);

  // Don't fail if we don't find it in our list
  // Note: mStyleSheets is nsCOMArray, so its IndexOf() method
  // returns -1 on failure.
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
      if (element->IsHTML(nsGkAtoms::img) ||
          element->IsHTML(nsGkAtoms::embed) ||
          element->IsHTML(nsGkAtoms::a) ||
          (element->IsHTML(nsGkAtoms::body) &&
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

  nsCOMPtr<nsIDOMNode> selectAllNode = FindUserSelectAllNode(aNode);
  
  if (selectAllNode)
  {
    return nsEditor::DeleteNode(selectAllNode);
  }
  return nsEditor::DeleteNode(aNode);
}

NS_IMETHODIMP nsHTMLEditor::DeleteText(nsIDOMCharacterData *aTextNode,
                                       PRUint32             aOffset,
                                       PRUint32             aLength)
{
  // do nothing if the node is read-only
  if (!IsModifiableNode(aTextNode)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> selectAllNode = FindUserSelectAllNode(aTextNode);
  
  if (selectAllNode)
  {
    return nsEditor::DeleteNode(selectAllNode);
  }
  return nsEditor::DeleteText(aTextNode, aOffset, aLength);
}

NS_IMETHODIMP nsHTMLEditor::InsertTextImpl(const nsAString& aStringToInsert, 
                                           nsCOMPtr<nsIDOMNode> *aInOutNode, 
                                           PRInt32 *aInOutOffset,
                                           nsIDOMDocument *aDoc)
{
  // do nothing if the node is read-only
  if (!IsModifiableNode(*aInOutNode)) {
    return NS_ERROR_FAILURE;
  }

  return nsEditor::InsertTextImpl(aStringToInsert, aInOutNode, aInOutOffset, aDoc);
}

void
nsHTMLEditor::ContentAppended(nsIDocument *aDocument, nsIContent* aContainer,
                              nsIContent* aFirstNewContent,
                              PRInt32 aIndexInContainer)
{
  ContentInserted(aDocument, aContainer, aFirstNewContent, aIndexInContainer);
}

void
nsHTMLEditor::ContentInserted(nsIDocument *aDocument, nsIContent* aContainer,
                              nsIContent* aChild, PRInt32 aIndexInContainer)
{
  if (!aChild) {
    return;
  }

  nsCOMPtr<nsIHTMLEditor> kungFuDeathGrip(this);

  if (ShouldReplaceRootElement()) {
    ResetRootElementAndEventTarget();
  }
  // We don't need to handle our own modifications
  else if (!mAction && (aContainer ? aContainer->IsEditable() : aDocument->IsEditable())) {
    if (IsMozEditorBogusNode(aChild)) {
      // Ignore insertion of the bogus node
      return;
    }
    mRules->DocumentModified();

    // Update spellcheck for only the newly-inserted node (bug 743819)
    if (mInlineSpellChecker) {
      nsRefPtr<nsRange> range = new nsRange();
      nsresult res = range->Set(aContainer, aIndexInContainer,
                                aContainer, aIndexInContainer + 1);
      if (NS_SUCCEEDED(res)) {
        mInlineSpellChecker->SpellCheckRange(range);
      }
    }
  }
}

void
nsHTMLEditor::ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer,
                             nsIContent* aChild, PRInt32 aIndexInContainer,
                             nsIContent* aPreviousSibling)
{
  nsCOMPtr<nsIHTMLEditor> kungFuDeathGrip(this);

  if (SameCOMIdentity(aChild, mRootElement)) {
    ResetRootElementAndEventTarget();
  }
  // We don't need to handle our own modifications
  else if (!mAction && (aContainer ? aContainer->IsEditable() : aDocument->IsEditable())) {
    if (aChild && IsMozEditorBogusNode(aChild)) {
      // Ignore removal of the bogus node
      return;
    }
    mRules->DocumentModified();
  }
}


/* This routine examines aNode and its ancestors looking for any node which has the
   -moz-user-select: all style lit.  Return the highest such ancestor.  */
already_AddRefed<nsIDOMNode>
nsHTMLEditor::FindUserSelectAllNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMElement> root = do_QueryInterface(GetRoot());
  if (!nsEditorUtils::IsDescendantOf(aNode, root))
    return nsnull;

  nsCOMPtr<nsIDOMNode> resultNode;  // starts out empty
  nsAutoString mozUserSelectValue;
  while (node)
  {
    // retrieve the computed style of -moz-user-select for node
    mHTMLCSSUtils->GetComputedProperty(node, nsEditProperty::cssMozUserSelect, mozUserSelectValue);
    if (mozUserSelectValue.EqualsLiteral("all"))
    {
      resultNode = node;
    }
    if (node != root)
    {
      nsCOMPtr<nsIDOMNode> tmp;
      node->GetParentNode(getter_AddRefs(tmp));
      node = tmp;
    }
    else
    {
      node = nsnull;
    }
  } 

  return resultNode.forget();
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

static nsresult
SetSelectionAroundHeadChildren(nsISelection* aSelection,
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
  PRUint32 childCount = headNode->GetChildCount();
  return aSelection->ExtendNative(headNode, childCount + 1);
}

NS_IMETHODIMP
nsHTMLEditor::GetHeadContentsAsHTML(nsAString& aOutputString)
{
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // Save current selection
  nsAutoSelectionReset selectionResetter(selection, this);

  res = SetSelectionAroundHeadChildren(selection, mDocWeak);
  NS_ENSURE_SUCCESS(res, res);

  res = OutputToString(NS_LITERAL_STRING("text/html"),
                       nsIDocumentEncoder::OutputSelectionOnly,
                       aOutputString);
  if (NS_SUCCEEDED(res))
  {
    // Selection always includes <body></body>,
    //  so terminate there
    nsReadingIterator<PRUnichar> findIter,endFindIter;
    aOutputString.BeginReading(findIter);
    aOutputString.EndReading(endFindIter);
    //counting on our parser to always lower case!!!
    if (CaseInsensitiveFindInReadable(NS_LITERAL_STRING("<body"),
                                      findIter, endFindIter))
    {
      nsReadingIterator<PRUnichar> beginIter;
      aOutputString.BeginReading(beginIter);
      PRInt32 offset = Distance(beginIter, findIter);//get the distance

      nsWritingIterator<PRUnichar> writeIter;
      aOutputString.BeginWriting(writeIter);
      // Ensure the string ends in a newline
      PRUnichar newline ('\n');
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
nsHTMLEditor::DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
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
nsHTMLEditor::StyleSheetLoaded(nsCSSStyleSheet* aSheet, bool aWasAlternate,
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
      nsCAutoString spec;
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
nsHTMLEditor::StartOperation(OperationID opID,
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
nsHTMLEditor::TagCanContainTag(nsIAtom* aParentTag, nsIAtom* aChildTag)
{
  MOZ_ASSERT(aParentTag && aChildTag);

  nsIParserService* parserService = nsContentUtils::GetParserService();

  PRInt32 childTagEnum;
  // XXX Should this handle #cdata-section too?
  if (aChildTag == nsGkAtoms::textTagName) {
    childTagEnum = eHTMLTag_text;
  } else {
    childTagEnum = parserService->HTMLAtomTagToId(aChildTag);
  }

  PRInt32 parentTagEnum = parserService->HTMLAtomTagToId(aParentTag);
  NS_ASSERTION(parentTagEnum < NS_HTML_TAG_MAX,
               "Fix the caller, this type of node can never contain children.");

  return nsHTMLEditUtils::CanContain(parentTagEnum, childTagEnum);
}

bool
nsHTMLEditor::IsContainer(nsIDOMNode *aNode)
{
  if (!aNode) {
    return false;
  }

  nsAutoString stringTag;

  nsresult rv = aNode->GetNodeName(stringTag);
  NS_ENSURE_SUCCESS(rv, false);

  PRInt32 tagEnum;
  // XXX Should this handle #cdata-section too?
  if (stringTag.EqualsLiteral("#text")) {
    tagEnum = eHTMLTag_text;
  }
  else {
    tagEnum = nsContentUtils::GetParserService()->HTMLStringTagToId(stringTag);
  }

  return nsHTMLEditUtils::IsContainer(tagEnum);
}


NS_IMETHODIMP 
nsHTMLEditor::SelectEntireDocument(nsISelection *aSelection)
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

  nsresult rv;
  nsCOMPtr<nsISelectionController> selCon;
  rv = GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelection> selection;
  rv = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                            getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> anchorNode;
  rv = selection->GetAnchorNode(getter_AddRefs(anchorNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> anchorContent = do_QueryInterface(anchorNode, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // If the anchor content has independent selection, we never need to explicitly
  // select its children.
  if (anchorContent->HasIndependentSelection()) {
    nsCOMPtr<nsISelectionPrivate> selPriv = do_QueryInterface(selection);
    NS_ENSURE_TRUE(selPriv, NS_ERROR_UNEXPECTED);
    rv = selPriv->SetAncestorLimiter(nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDOMNode> rootElement = do_QueryInterface(mRootElement, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    return selection->SelectAllChildren(rootElement);
  }

  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  nsIContent *rootContent = anchorContent->GetSelectionRootContent(ps);
  NS_ENSURE_TRUE(rootContent, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMNode> rootElement = do_QueryInterface(rootContent, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return selection->SelectAllChildren(rootElement);
}


// this will NOT find aAttribute unless aAttribute has a non-null value
// so singleton attributes like <Table border> will not be matched!
bool nsHTMLEditor::IsTextPropertySetByContent(nsIContent*      aContent,
                                              nsIAtom*         aProperty,
                                              const nsAString* aAttribute,
                                              const nsAString* aValue,
                                              nsAString*       outValue)
{
  MOZ_ASSERT(aContent && aProperty);
  MOZ_ASSERT_IF(aAttribute, aValue);
  bool isSet;
  IsTextPropertySetByContent(aContent->AsDOMNode(), aProperty, aAttribute,
                             aValue, isSet, outValue);
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
      node = nsnull;
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
  if (!element || !element->IsHTML() ||
      !nsHTMLEditUtils::IsTableElement(element) ||
      !IsDescendantOfEditorRoot(element)) {
    return false;
  }

  nsIContent* node = element;
  while (node->HasChildren()) {
    node = node->GetFirstChild();
  }

  // Set selection at beginning of the found node
  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, false);
  NS_ENSURE_TRUE(selection, false);

  return NS_SUCCEEDED(selection->CollapseNative(node, 0));
}            

///////////////////////////////////////////////////////////////////////////
// GetEnclosingTable: find ancestor who is a table, if any
//                  
nsCOMPtr<nsIDOMNode> 
nsHTMLEditor::GetEnclosingTable(nsIDOMNode *aNode)
{
  NS_PRECONDITION(aNode, "null node passed to nsHTMLEditor::GetEnclosingTable");
  nsCOMPtr<nsIDOMNode> tbl, tmp, node = aNode;

  while (!tbl)
  {
    tmp = GetBlockNodeParent(node);
    if (!tmp) break;
    if (nsHTMLEditUtils::IsTable(tmp)) tbl = tmp;
    node = tmp;
  }
  return tbl;
}


/* this method scans the selection for adjacent text nodes
 * and collapses them into a single text node.
 * "adjacent" means literally adjacent siblings of the same parent.
 * Uses nsEditor::JoinNodes so action is undoable. 
 * Should be called within the context of a batch transaction.
 */
NS_IMETHODIMP
nsHTMLEditor::CollapseAdjacentTextNodes(nsIDOMRange *aInRange)
{
  NS_ENSURE_TRUE(aInRange, NS_ERROR_NULL_POINTER);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  nsTArray<nsIDOMNode*> textNodes;
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
    // we assume a textNodes entry can't be nsnull
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

NS_IMETHODIMP 
nsHTMLEditor::SetSelectionAtDocumentStart(nsISelection *aSelection)
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
  NS_ENSURE_TRUE(inNode, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> sibling, child, unused;
  
  // Two possibilities: the container cold be empty of editable content.
  // If that is the case, we need to compare what is before and after inNode
  // to determine if we need a br.
  // Or it could not be empty, in which case we have to compare previous
  // sibling and first child to determine if we need a leading br,
  // and compare following sibling and last child to determine if we need a
  // trailing br.
  
  res = GetFirstEditableChild(inNode, address_of(child));
  NS_ENSURE_SUCCESS(res, res);
  
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
      res = GetFirstEditableChild(inNode, address_of(child));
      NS_ENSURE_SUCCESS(res, res);
      if (child && !IsBlockNode(child))
      {
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
      res = GetLastEditableChild(inNode, address_of(child));
      NS_ENSURE_SUCCESS(res, res);
      if (child && !IsBlockNode(child) && !nsTextEditUtils::IsBreak(child))
      {
        // insert br node
        PRUint32 len;
        res = GetLengthOfDOMNode(inNode, len);
        NS_ENSURE_SUCCESS(res, res);
        res = CreateBR(inNode, (PRInt32)len, address_of(unused));
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
  return RemoveContainer(inNode);
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
  *outNode = NULL;

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
nsHTMLEditor::GetPriorHTMLSibling(nsINode* aParent, PRInt32 aOffset)
{
  MOZ_ASSERT(aParent);

  nsIContent* node = aParent->GetChildAt(aOffset - 1);
  if (!node || IsEditable(node)) {
    return node;
  }

  return GetPriorHTMLSibling(node);
}

nsresult
nsHTMLEditor::GetPriorHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  *outNode = NULL;

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
  *outNode = nsnull;

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
nsHTMLEditor::GetNextHTMLSibling(nsINode* aParent, PRInt32 aOffset)
{
  MOZ_ASSERT(aParent);

  nsIContent* node = aParent->GetChildAt(aOffset + 1);
  if (!node || IsEditable(node)) {
    return node;
  }

  return GetNextHTMLSibling(node);
}

nsresult
nsHTMLEditor::GetNextHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  *outNode = NULL;

  nsCOMPtr<nsINode> parent = do_QueryInterface(inParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  *outNode = do_QueryInterface(GetNextHTMLSibling(parent, inOffset));
  return NS_OK;
}



///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLNode: returns the previous editable leaf node, if there is
//                   one within the <body>
//
nsresult
nsHTMLEditor::GetPriorHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);

  nsIContent* activeEditingHost = GetActiveEditingHost();
  if (!activeEditingHost) {
    *outNode = nsnull;
    return NS_OK;
  }

  nsresult res = GetPriorNode(inNode, true, address_of(*outNode),
                              bNoBlockCrossing);
  NS_ENSURE_SUCCESS(res, res);
  
  NS_ASSERTION(!*outNode || IsDescendantOfEditorRoot(*outNode),
               "GetPriorNode screwed up");
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLNode: same as above but takes {parent,offset} instead of node
//                       
nsresult
nsHTMLEditor::GetPriorHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);

  nsIContent* activeEditingHost = GetActiveEditingHost();
  if (!activeEditingHost) {
    *outNode = nsnull;
    return NS_OK;
  }

  nsresult res = GetPriorNode(inParent, inOffset, true, address_of(*outNode),
                              bNoBlockCrossing);
  NS_ENSURE_SUCCESS(res, res);
  
  NS_ASSERTION(!*outNode || IsDescendantOfEditorRoot(*outNode),
               "GetPriorNode screwed up");
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetNextHTMLNode: returns the next editable leaf node, if there is
//                   one within the <body>
//                       
nsresult
nsHTMLEditor::GetNextHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  nsresult res = GetNextNode(inNode, true, address_of(*outNode), bNoBlockCrossing);
  NS_ENSURE_SUCCESS(res, res);
  
  // if it's not in the body, then zero it out
  if (*outNode && !IsDescendantOfEditorRoot(*outNode)) {
    *outNode = nsnull;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetNHTMLextNode: same as above but takes {parent,offset} instead of node
//                       
nsresult
nsHTMLEditor::GetNextHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode, bool bNoBlockCrossing)
{
  NS_ENSURE_TRUE(outNode, NS_ERROR_NULL_POINTER);
  nsresult res = GetNextNode(inParent, inOffset, true, address_of(*outNode), bNoBlockCrossing);
  NS_ENSURE_SUCCESS(res, res);
  
  // if it's not in the body, then zero it out
  if (*outNode && !IsDescendantOfEditorRoot(*outNode)) {
    *outNode = nsnull;
  }
  return res;
}


nsresult 
nsHTMLEditor::IsFirstEditableChild( nsIDOMNode *aNode, bool *aOutIsFirst)
{
  // check parms
  NS_ENSURE_TRUE(aOutIsFirst && aNode, NS_ERROR_NULL_POINTER);
  
  // init out parms
  *aOutIsFirst = false;
  
  // find first editable child and compare it to aNode
  nsCOMPtr<nsIDOMNode> parent, firstChild;
  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);
  res = GetFirstEditableChild(parent, address_of(firstChild));
  NS_ENSURE_SUCCESS(res, res);
  
  *aOutIsFirst = (firstChild.get() == aNode);
  return res;
}


nsresult 
nsHTMLEditor::IsLastEditableChild( nsIDOMNode *aNode, bool *aOutIsLast)
{
  // check parms
  NS_ENSURE_TRUE(aOutIsLast && aNode, NS_ERROR_NULL_POINTER);
  
  // init out parms
  *aOutIsLast = false;
  
  // find last editable child and compare it to aNode
  nsCOMPtr<nsIDOMNode> parent, lastChild;
  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);
  res = GetLastEditableChild(parent, address_of(lastChild));
  NS_ENSURE_SUCCESS(res, res);
  
  *aOutIsLast = (lastChild.get() == aNode);
  return res;
}


nsresult 
nsHTMLEditor::GetFirstEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstChild)
{
  // check parms
  NS_ENSURE_TRUE(aOutFirstChild && aNode, NS_ERROR_NULL_POINTER);
  
  // init out parms
  *aOutFirstChild = nsnull;
  
  // find first editable child
  nsCOMPtr<nsIDOMNode> child;
  nsresult res = aNode->GetFirstChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(res, res);
  
  while (child && !IsEditable(child))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = child->GetNextSibling(getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
    child = tmp;
  }
  
  *aOutFirstChild = child;
  return res;
}


nsresult 
nsHTMLEditor::GetLastEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastChild)
{
  // check parms
  NS_ENSURE_TRUE(aOutLastChild && aNode, NS_ERROR_NULL_POINTER);
  
  // init out parms
  *aOutLastChild = aNode;
  
  // find last editable child
  nsCOMPtr<nsIDOMNode> child;
  nsresult res = aNode->GetLastChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(res, res);
  
  while (child && !IsEditable(child))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = child->GetPreviousSibling(getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
    child = tmp;
  }
  
  *aOutLastChild = child;
  return res;
}

nsresult 
nsHTMLEditor::GetFirstEditableLeaf( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstLeaf)
{
  // check parms
  NS_ENSURE_TRUE(aOutFirstLeaf && aNode, NS_ERROR_NULL_POINTER);
  
  // init out parms
  *aOutFirstLeaf = aNode;
  
  // find leftmost leaf
  nsCOMPtr<nsIDOMNode> child;
  nsresult res = NS_OK;
  child = GetLeftmostChild(aNode);  
  while (child && (!IsEditable(child) || !nsEditorUtils::IsLeafNode(child)))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = GetNextHTMLNode(child, address_of(tmp));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
    
    // only accept nodes that are descendants of aNode
    if (nsEditorUtils::IsDescendantOf(tmp, aNode))
      child = tmp;
    else
    {
      child = nsnull;  // this will abort the loop
    }
  }
  
  *aOutFirstLeaf = child;
  return res;
}


nsresult 
nsHTMLEditor::GetLastEditableLeaf(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastLeaf)
{
  // check parms
  NS_ENSURE_TRUE(aOutLastLeaf && aNode, NS_ERROR_NULL_POINTER);
  
  // init out parms
  *aOutLastLeaf = nsnull;
  
  // find rightmost leaf
  nsCOMPtr<nsIDOMNode> child = GetRightmostChild(aNode, false);
  nsresult res = NS_OK;
  while (child && (!IsEditable(child) || !nsEditorUtils::IsLeafNode(child)))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = GetPriorHTMLNode(child, address_of(tmp));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
    
    // only accept nodes that are descendants of aNode
    if (nsEditorUtils::IsDescendantOf(tmp, aNode))
      child = tmp;
    else
    {
      child = nsnull;
    }
  }
  
  *aOutLastLeaf = child;
  return res;
}

bool
nsHTMLEditor::IsTextInDirtyFrameVisible(nsIContent *aNode)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(aNode->NodeType() == nsIDOMNode::TEXT_NODE);

  bool isEmptyTextNode;
  nsresult rv = IsVisTextNode(aNode, &isEmptyTextNode, false);
  if (NS_FAILED(rv)) {
    // We are following the historical decision:
    //   if we don't know, we say it's visible...
    return true;
  }

  return !isEmptyTextNode;
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

  PRUint32 length = aNode->TextLength();
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
      nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode);
      nsWSRunObject wsRunObj(this, node, 0);
      nsCOMPtr<nsIDOMNode> visNode;
      PRInt32 outVisOffset=0;
      PRInt16 visType=0;
      wsRunObj.NextVisibleNode(node, 0, address_of(visNode),
                               &outVisOffset, &visType);
      if ( (visType == nsWSRunObject::eNormalWS) ||
           (visType == nsWSRunObject::eText) )
      {
        *outIsEmptyNode = (node != visNode);
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
      (aNode->IsElement() &&
       (nsHTMLEditUtils::IsNamedAnchor(aNode->AsElement())  ||
        nsHTMLEditUtils::IsFormWidget(aNode->AsElement())   ||
        (aListOrCellNotEmpty &&
         (nsHTMLEditUtils::IsListItem(aNode->AsElement())   ||
          nsHTMLEditUtils::IsTableCell(aNode->AsElement()))))))  {
    *outIsEmptyNode = false;
    return NS_OK;
  }
    
  // need this for later
  bool isListItemOrCell = aNode->IsElement() &&
       (nsHTMLEditUtils::IsListItem(aNode->AsElement()) ||
        nsHTMLEditUtils::IsTableCell(aNode->AsElement()));
       
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

        if (aSingleBRDoesntCount && !*aSeenBR && child->IsHTML(nsGkAtoms::br)) {
          // the first br in a block doesn't count if the caller so indicated
          *aSeenBR = true;
        } else {
          // is it an empty node of some sort?
          // note: list items or table cells are not considered empty
          // if they contain other lists or tables
          if (child->IsElement()) {
            if (isListItemOrCell) {
              if (nsHTMLEditUtils::IsList(child->AsElement()) || child->IsHTML(nsGkAtoms::table)) {
                // break out if we find we aren't empty
                *outIsEmptyNode = false;
                return NS_OK;
              }
            } else if (nsHTMLEditUtils::IsFormWidget(child->AsElement())) {
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
  nsresult res = NS_OK;
  if (IsCSSEnabled() && mHTMLCSSUtils) {
    PRInt32 count;
    res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(aElement, nsnull, &aAttribute, &aValue, &count,
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
        existingValue.AppendLiteral(" ");
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
nsHTMLEditor::RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                          const nsAString & aAttribute,
                                          bool aSuppressTransaction)
{
  nsresult res = NS_OK;
  if (IsCSSEnabled() && mHTMLCSSUtils) {
    res = mHTMLCSSUtils->RemoveCSSEquivalentToHTMLStyle(aElement, nsnull, &aAttribute, nsnull,
                                                        aSuppressTransaction);
    NS_ENSURE_SUCCESS(res, res);
  }

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
  PRUint32 flags = mFlags;
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
NS_IMETHODIMP
nsHTMLEditor::SetCSSBackgroundColor(const nsAString& aColor)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  nsRefPtr<Selection> selection = GetSelection();

  bool isCollapsed = selection->Collapsed();

  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  
  bool cancel, handled;
  nsTextRulesInfo ruleInfo(kOpSetTextProperty);
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(enumerator, NS_ERROR_FAILURE);

    // loop thru the ranges in the selection
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    nsAutoString bgcolor; bgcolor.AssignLiteral("bgcolor");
    nsCOMPtr<nsIDOMNode> cachedBlockParent = nsnull;
    while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
    {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(currentItem, NS_ERROR_FAILURE);
      
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
      
      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      PRInt32 startOffset, endOffset;
      res = range->GetStartContainer(getter_AddRefs(startNode));
      NS_ENSURE_SUCCESS(res, res);
      res = range->GetEndContainer(getter_AddRefs(endNode));
      NS_ENSURE_SUCCESS(res, res);
      res = range->GetStartOffset(&startOffset);
      NS_ENSURE_SUCCESS(res, res);
      res = range->GetEndOffset(&endOffset);
      NS_ENSURE_SUCCESS(res, res);
      if ((startNode == endNode) && IsTextNode(startNode))
      {
        // let's find the block container of the text node
        nsCOMPtr<nsIDOMNode> blockParent;
        blockParent = GetBlockNodeParent(startNode);
        // and apply the background color to that block container
        if (cachedBlockParent != blockParent)
        {
          cachedBlockParent = blockParent;
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(blockParent);
          PRInt32 count;
          res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, false);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
      else if ((startNode == endNode) && nsTextEditUtils::IsBody(startNode) && isCollapsed)
      {
        // we have no block in the document, let's apply the background to the body 
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(startNode);
        PRInt32 count;
        res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, false);
        NS_ENSURE_SUCCESS(res, res);
      }
      else if ((startNode == endNode) && (((endOffset-startOffset) == 1) || (!startOffset && !endOffset)))
      {
        // a unique node is selected, let's also apply the background color
        // to the containing block, possibly the node itself
        nsCOMPtr<nsIDOMNode> selectedNode = GetChildAt(startNode, startOffset);
        bool isBlock =false;
        res = NodeIsBlockStatic(selectedNode, &isBlock);
        NS_ENSURE_SUCCESS(res, res);
        nsCOMPtr<nsIDOMNode> blockParent = selectedNode;
        if (!isBlock) {
          blockParent = GetBlockNodeParent(selectedNode);
        }
        if (cachedBlockParent != blockParent)
        {
          cachedBlockParent = blockParent;
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(blockParent);
          PRInt32 count;
          res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, false);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
      else
      {
        // not the easy case.  range not contained in single text node. 
        // there are up to three phases here.  There are all the nodes
        // reported by the subtree iterator to be processed.  And there
        // are potentially a starting textnode and an ending textnode
        // which are only partially contained by the range.
        
        // lets handle the nodes reported by the iterator.  These nodes
        // are entirely contained in the selection range.  We build up
        // a list of them (since doing operations on the document during
        // iteration would perturb the iterator).

        nsCOMPtr<nsIContentIterator> iter =
          do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &res);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_TRUE(iter, NS_ERROR_FAILURE);

        nsCOMArray<nsIDOMNode> arrayOfNodes;
        nsCOMPtr<nsIDOMNode> node;
                
        // iterate range and build up array
        res = iter->Init(range);
        // init returns an error if no nodes in range.
        // this can easily happen with the subtree 
        // iterator if the selection doesn't contain
        // any *whole* nodes.
        if (NS_SUCCEEDED(res))
        {
          while (!iter->IsDone())
          {
            node = do_QueryInterface(iter->GetCurrentNode());
            NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

            if (IsEditable(node))
            {
              arrayOfNodes.AppendObject(node);
            }

            iter->Next();
          }
        }
        // first check the start parent of the range to see if it needs to 
        // be separately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(startNode) && IsEditable(startNode))
        {
          nsCOMPtr<nsIDOMNode> blockParent;
          blockParent = GetBlockNodeParent(startNode);
          if (cachedBlockParent != blockParent)
          {
            cachedBlockParent = blockParent;
            nsCOMPtr<nsIDOMElement> element = do_QueryInterface(blockParent);
            PRInt32 count;
            res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, false);
            NS_ENSURE_SUCCESS(res, res);
          }
        }
        
        // then loop through the list, set the property on each node
        PRInt32 listCount = arrayOfNodes.Count();
        PRInt32 j;
        for (j = 0; j < listCount; j++)
        {
          node = arrayOfNodes[j];
          // do we have a block here ?
          bool isBlock =false;
          res = NodeIsBlockStatic(node, &isBlock);
          NS_ENSURE_SUCCESS(res, res);
          nsCOMPtr<nsIDOMNode> blockParent = node;
          if (!isBlock) {
            // no we don't, let's find the block ancestor
            blockParent = GetBlockNodeParent(node);
          }
          if (cachedBlockParent != blockParent)
          {
            cachedBlockParent = blockParent;
            nsCOMPtr<nsIDOMElement> element = do_QueryInterface(blockParent);
            PRInt32 count;
            // and set the property on it
            res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, false);
            NS_ENSURE_SUCCESS(res, res);
          }
        }
        arrayOfNodes.Clear();
        
        // last check the end parent of the range to see if it needs to 
        // be separately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(endNode) && IsEditable(endNode))
        {
          nsCOMPtr<nsIDOMNode> blockParent;
          blockParent = GetBlockNodeParent(endNode);
          if (cachedBlockParent != blockParent)
          {
            cachedBlockParent = blockParent;
            nsCOMPtr<nsIDOMElement> element = do_QueryInterface(blockParent);
            PRInt32 count;
            res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, false);
            NS_ENSURE_SUCCESS(res, res);
          }
        }
      }
      enumerator->Next();
    }
  }
  if (!cancel)
  {
    // post-process
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return res;
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

  if (aNode1->Tag() != aNode2->Tag()) {
    return false;
  }

  if (!IsCSSEnabled() || !aNode1->IsHTML(nsGkAtoms::span)) {
    return true;
  }

  // If CSS is enabled, we are stricter about span nodes.
  return mHTMLCSSUtils->ElementsSameStyle(aNode1->AsDOMNode(),
                                          aNode2->AsDOMNode());
}

NS_IMETHODIMP
nsHTMLEditor::CopyLastEditableChildStyles(nsIDOMNode * aPreviousBlock, nsIDOMNode * aNewBlock,
                                          nsIDOMNode **aOutBrNode)
{
  *aOutBrNode = nsnull;
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
    res = GetLastEditableChild(child, address_of(tmp));
    NS_ENSURE_SUCCESS(res, res);
  }
  while (child && nsTextEditUtils::IsBreak(child)) {
    nsCOMPtr<nsIDOMNode> priorNode;
    res = GetPriorHTMLNode(child, address_of(priorNode));
    NS_ENSURE_SUCCESS(res, res);
    child = priorNode;
  }
  nsCOMPtr<nsIDOMNode> newStyles = nsnull, deepestStyle = nsnull;
  while (child && (child != aPreviousBlock)) {
    if (nsHTMLEditUtils::IsInlineStyle(child) ||
        nsEditor::NodeIsType(child, nsEditProperty::span)) {
      nsAutoString domTagName;
      child->GetNodeName(domTagName);
      ToLowerCase(domTagName);
      if (newStyles) {
        nsCOMPtr<nsIDOMNode> newContainer;
        res = InsertContainerAbove(newStyles, address_of(newContainer), domTagName);
        NS_ENSURE_SUCCESS(res, res);
        newStyles = newContainer;
      }
      else {
        res = CreateNode(domTagName, aNewBlock, 0, getter_AddRefs(newStyles));
        NS_ENSURE_SUCCESS(res, res);
        deepestStyle = newStyles;
      }
      res = CloneAttributes(newStyles, child);
      NS_ENSURE_SUCCESS(res, res);
    }
    nsCOMPtr<nsIDOMNode> tmp;
    res = child->GetParentNode(getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(res, res);
    child = tmp;
  }
  if (deepestStyle) {
    nsCOMPtr<nsIDOMNode> outBRNode;
    res = CreateBR(deepestStyle, 0, address_of(outBRNode));
    NS_ENSURE_SUCCESS(res, res);
    // Getters must addref
    outBRNode.forget(aOutBrNode);
  }
  return NS_OK;
}

nsresult
nsHTMLEditor::GetElementOrigin(nsIDOMElement * aElement, PRInt32 & aX, PRInt32 & aY)
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
    nsCOMPtr<nsISelection> selection;
    res = GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);
    res = CheckSelectionStateForAnonymousButtons(selection);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetSelectionContainer(nsIDOMElement ** aReturn)
{
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  // if we don't get the selection, just skip this
  if (NS_FAILED(res) || !selection) return res;

  nsCOMPtr<nsIDOMNode> focusNode;

  if (selection->Collapsed()) {
    res = selection->GetFocusNode(getter_AddRefs(focusNode));
    NS_ENSURE_SUCCESS(res, res);
  } else {

    PRInt32 rangeCount;
    res = selection->GetRangeCount(&rangeCount);
    NS_ENSURE_SUCCESS(res, res);

    if (rangeCount == 1) {

      nsCOMPtr<nsIDOMRange> range;
      res = selection->GetRangeAt(0, getter_AddRefs(range));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

      nsCOMPtr<nsIDOMNode> startContainer, endContainer;
      res = range->GetStartContainer(getter_AddRefs(startContainer));
      NS_ENSURE_SUCCESS(res, res);
      res = range->GetEndContainer(getter_AddRefs(endContainer));
      NS_ENSURE_SUCCESS(res, res);
      PRInt32 startOffset, endOffset;
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
      PRInt32 i;
      nsCOMPtr<nsIDOMRange> range;
      for (i = 0; i < rangeCount; i++)
      {
        res = selection->GetRangeAt(i, getter_AddRefs(range));
        NS_ENSURE_SUCCESS(res, res);
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
    PRUint16 nodeType;
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
  NS_ENSURE_TRUE(mDocWeak, nsnull);

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nsnull);

  nsCOMPtr<nsIContent> focusedContent = fm->GetFocusedContent();

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  bool inDesignMode = doc->HasFlag(NODE_IS_EDITABLE);
  if (!focusedContent) {
    // in designMode, nobody gets focus in most cases.
    if (inDesignMode && OurWindowHasFocus()) {
      nsCOMPtr<nsIContent> docRoot = doc->GetRootElement();
      return docRoot.forget();
    }
    return nsnull;
  }

  if (inDesignMode) {
    return OurWindowHasFocus() &&
      nsContentUtils::ContentIsDescendantOf(focusedContent, doc) ?
      focusedContent.forget() : nsnull;
  }

  // We're HTML editor for contenteditable

  // If the focused content isn't editable, or it has independent selection,
  // we don't have focus.
  if (!focusedContent->HasFlag(NODE_IS_EDITABLE) ||
      focusedContent->HasIndependentSelection()) {
    return nsnull;
  }
  // If our window is focused, we're focused.
  return OurWindowHasFocus() ? focusedContent.forget() : nsnull;
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
  NS_ENSURE_TRUE(mDocWeak, nsnull);

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, nsnull);
  if (doc->HasFlag(NODE_IS_EDITABLE)) {
    return doc->GetBodyElement();
  }

  // We're HTML editor for contenteditable
  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, nsnull);
  nsCOMPtr<nsIDOMNode> focusNode;
  rv = selection->GetFocusNode(getter_AddRefs(focusNode));
  NS_ENSURE_SUCCESS(rv, nsnull);
  nsCOMPtr<nsIContent> content = do_QueryInterface(focusNode);
  if (!content) {
    return nsnull;
  }

  // If the active content isn't editable, or it has independent selection,
  // we're not active.
  if (!content->HasFlag(NODE_IS_EDITABLE) ||
      content->HasIndependentSelection()) {
    return nsnull;
  }
  return content->GetEditingHost();
}

already_AddRefed<nsIDOMEventTarget>
nsHTMLEditor::GetDOMEventTarget()
{
  // Don't use getDocument here, because we have no way of knowing
  // whether Init() was ever called.  So we need to get the document
  // ourselves, if it exists.
  NS_PRECONDITION(mDocWeak, "This editor has not been initialized yet");
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryReferent(mDocWeak.get());
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
  mRootElement = nsnull;
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
    return nsnull;
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
nsHTMLEditor::IsEditable(nsIContent* aNode) {
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

// virtual MOZ_OVERRIDE
dom::Element*
nsHTMLEditor::GetEditorRoot()
{
  return GetActiveEditingHost();
}
