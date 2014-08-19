/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsComputedDOMStyle.h"
#include "nsDebug.h"
#include "nsEditProperty.h"
#include "nsError.h"
#include "nsHTMLCSSUtils.h"
#include "nsHTMLEditor.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsID.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSValue.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLInlineTableEditor.h"
#include "nsIHTMLObjectResizer.h"
#include "nsIMutationObserver.h"
#include "nsINode.h"
#include "nsIPresShell.h"
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"
#include "nsLiteralString.h"
#include "nsPresContext.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsUnicharUtils.h"
#include "nscore.h"
#include "nsContentUtils.h" // for nsAutoScriptBlocker

class nsIDOMEventListener;
class nsISelection;

using namespace mozilla;
using namespace mozilla::dom;

// retrieve an integer stored into a CSS computed float value
static int32_t GetCSSFloatValue(nsIDOMCSSStyleDeclaration * aDecl,
                                const nsAString & aProperty)
{
  MOZ_ASSERT(aDecl);

  nsCOMPtr<nsIDOMCSSValue> value;
  // get the computed CSSValue of the property
  nsresult res = aDecl->GetPropertyCSSValue(aProperty, getter_AddRefs(value));
  if (NS_FAILED(res) || !value) return 0;

  // check the type of the returned CSSValue; we handle here only
  // pixel and enum types
  nsCOMPtr<nsIDOMCSSPrimitiveValue> val = do_QueryInterface(value);
  uint16_t type;
  val->GetPrimitiveType(&type);

  float f = 0;
  switch (type) {
    case nsIDOMCSSPrimitiveValue::CSS_PX:
      // the value is in pixels, just get it
      res = val->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_PX, &f);
      NS_ENSURE_SUCCESS(res, 0);
      break;
    case nsIDOMCSSPrimitiveValue::CSS_IDENT: {
      // the value is keyword, we have to map these keywords into
      // numeric values
      nsAutoString str;
      res = val->GetStringValue(str);
      if (str.EqualsLiteral("thin"))
        f = 1;
      else if (str.EqualsLiteral("medium"))
        f = 3;
      else if (str.EqualsLiteral("thick"))
        f = 5;
      break;
    }
  }

  return (int32_t) f;
}

class nsElementDeletionObserver MOZ_FINAL : public nsIMutationObserver
{
public:
  nsElementDeletionObserver(nsINode* aNativeAnonNode, nsINode* aObservedNode)
  : mNativeAnonNode(aNativeAnonNode), mObservedNode(aObservedNode) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER
protected:
  ~nsElementDeletionObserver() {}
  nsINode* mNativeAnonNode;
  nsINode* mObservedNode;
};

NS_IMPL_ISUPPORTS(nsElementDeletionObserver, nsIMutationObserver)
NS_IMPL_NSIMUTATIONOBSERVER_CONTENT(nsElementDeletionObserver)

void
nsElementDeletionObserver::NodeWillBeDestroyed(const nsINode* aNode)
{
  NS_ASSERTION(aNode == mNativeAnonNode || aNode == mObservedNode,
               "Wrong aNode!");
  if (aNode == mNativeAnonNode) {
    mObservedNode->RemoveMutationObserver(this);
  } else {
    mNativeAnonNode->RemoveMutationObserver(this);
    static_cast<nsIContent*>(mNativeAnonNode)->UnbindFromTree();
  }

  NS_RELEASE_THIS();
}

// Returns in *aReturn an anonymous nsDOMElement of type aTag,
// child of aParentNode. If aIsCreatedHidden is true, the class
// "hidden" is added to the created element. If aAnonClass is not
// the empty string, it becomes the value of the attribute "_moz_anonclass"
nsresult
nsHTMLEditor::CreateAnonymousElement(const nsAString & aTag, nsIDOMNode *  aParentNode,
                                     const nsAString & aAnonClass, bool aIsCreatedHidden,
                                     nsIDOMElement ** aReturn)
{
  NS_ENSURE_ARG_POINTER(aParentNode);
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nullptr;

  nsCOMPtr<nsIContent> parentContent( do_QueryInterface(aParentNode) );
  NS_ENSURE_TRUE(parentContent, NS_OK);

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_NULL_POINTER);

  // Get the pres shell
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  // Create a new node through the element factory
  nsCOMPtr<Element> newContent =
    CreateHTMLContent(nsCOMPtr<nsIAtom>(do_GetAtom(aTag)));
  NS_ENSURE_STATE(newContent);

  nsCOMPtr<nsIDOMElement> newElement = do_QueryInterface(newContent);
  NS_ENSURE_TRUE(newElement, NS_ERROR_FAILURE);

  // add the "hidden" class if needed
  nsresult res;
  if (aIsCreatedHidden) {
    res = newElement->SetAttribute(NS_LITERAL_STRING("class"),
                                   NS_LITERAL_STRING("hidden"));
    NS_ENSURE_SUCCESS(res, res);
  }

  // add an _moz_anonclass attribute if needed
  if (!aAnonClass.IsEmpty()) {
    res = newElement->SetAttribute(NS_LITERAL_STRING("_moz_anonclass"),
                                   aAnonClass);
    NS_ENSURE_SUCCESS(res, res);
  }

  {
    nsAutoScriptBlocker scriptBlocker;

    // establish parenthood of the element
    newContent->SetIsNativeAnonymousRoot();
    res = newContent->BindToTree(doc, parentContent, parentContent, true);
    if (NS_FAILED(res)) {
      newContent->UnbindFromTree();
      return res;
    }
  }

  nsElementDeletionObserver* observer =
    new nsElementDeletionObserver(newContent, parentContent);
  NS_ADDREF(observer); // NodeWillBeDestroyed releases.
  parentContent->AddMutationObserver(observer);
  newContent->AddMutationObserver(observer);

  // display the element
  ps->RecreateFramesFor(newContent);

  newElement.forget(aReturn);
  return NS_OK;
}

// Removes event listener and calls DeleteRefToAnonymousNode.
void
nsHTMLEditor::RemoveListenerAndDeleteRef(const nsAString& aEvent,
                                         nsIDOMEventListener* aListener,
                                         bool aUseCapture,
                                         nsIDOMElement* aElement,
                                         nsIContent * aParentContent,
                                         nsIPresShell* aShell)
{
  nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(aElement));
  if (evtTarget) {
    evtTarget->RemoveEventListener(aEvent, aListener, aUseCapture);
  }
  DeleteRefToAnonymousNode(aElement, aParentContent, aShell);
}

// Deletes all references to an anonymous element
void
nsHTMLEditor::DeleteRefToAnonymousNode(nsIDOMElement* aElement,
                                       nsIContent* aParentContent,
                                       nsIPresShell* aShell)
{
  // call ContentRemoved() for the anonymous content
  // node so its references get removed from the frame manager's
  // undisplay map, and its layout frames get destroyed!

  if (aElement) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    if (content) {
      nsAutoScriptBlocker scriptBlocker;
      // Need to check whether aShell has been destroyed (but not yet deleted).
      // In that case presContext->GetPresShell() returns nullptr.
      // See bug 338129.
      if (aShell && aShell->GetPresContext() &&
          aShell->GetPresContext()->GetPresShell() == aShell) {
        nsCOMPtr<nsIDocumentObserver> docObserver = do_QueryInterface(aShell);
        if (docObserver) {
          // Call BeginUpdate() so that the nsCSSFrameConstructor/PresShell
          // knows we're messing with the frame tree.
          nsCOMPtr<nsIDocument> document = GetDocument();
          if (document)
            docObserver->BeginUpdate(document, UPDATE_CONTENT_MODEL);

          // XXX This is wrong (bug 439258).  Once it's fixed, the NS_WARNING
          // in RestyleManager::RestyleForRemove should be changed back
          // to an assertion.
          docObserver->ContentRemoved(content->GetCurrentDoc(),
                                      aParentContent, content, -1,
                                      content->GetPreviousSibling());
          if (document)
            docObserver->EndUpdate(document, UPDATE_CONTENT_MODEL);
        }
      }
      content->UnbindFromTree();
    }
  }
}

// The following method is mostly called by a selection listener. When a
// selection change is notified, the method is called to check if resizing
// handles, a grabber and/or inline table editing UI need to be displayed
// or refreshed
NS_IMETHODIMP
nsHTMLEditor::CheckSelectionStateForAnonymousButtons(nsISelection * aSelection)
{
  NS_ENSURE_ARG_POINTER(aSelection);

  // early way out if all contextual UI extensions are disabled
  NS_ENSURE_TRUE(mIsObjectResizingEnabled ||
      mIsAbsolutelyPositioningEnabled ||
      mIsInlineTableEditingEnabled, NS_OK);

  // Don't change selection state if we're moving.
  if (mIsMoving) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMElement> focusElement;
  // let's get the containing element of the selection
  nsresult res  = GetSelectionContainer(getter_AddRefs(focusElement));
  NS_ENSURE_TRUE(focusElement, NS_OK);
  NS_ENSURE_SUCCESS(res, res);

  // If we're not in a document, don't try to add resizers
  nsCOMPtr<dom::Element> focusElementNode = do_QueryInterface(focusElement);
  NS_ENSURE_STATE(focusElementNode);
  if (!focusElementNode->IsInDoc()) {
    return NS_OK;
  }

  // what's its tag?
  nsAutoString focusTagName;
  res = focusElement->GetTagName(focusTagName);
  NS_ENSURE_SUCCESS(res, res);
  ToLowerCase(focusTagName);
  nsCOMPtr<nsIAtom> focusTagAtom = do_GetAtom(focusTagName);

  nsCOMPtr<nsIDOMElement> absPosElement;
  if (mIsAbsolutelyPositioningEnabled) {
    // Absolute Positioning support is enabled, is the selection contained
    // in an absolutely positioned element ?
    res = GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(absPosElement));
    NS_ENSURE_SUCCESS(res, res);
  }

  nsCOMPtr<nsIDOMElement> cellElement;
  if (mIsObjectResizingEnabled || mIsInlineTableEditingEnabled) {
    // Resizing or Inline Table Editing is enabled, we need to check if the
    // selection is contained in a table cell
    res = GetElementOrParentByTagName(NS_LITERAL_STRING("td"),
                                      nullptr,
                                      getter_AddRefs(cellElement));
    NS_ENSURE_SUCCESS(res, res);
  }

  if (mIsObjectResizingEnabled && cellElement) {
    // we are here because Resizing is enabled AND selection is contained in
    // a cell

    // get the enclosing table
    if (nsEditProperty::img != focusTagAtom) {
      // the element container of the selection is not an image, so we'll show
      // the resizers around the table
      nsCOMPtr<nsIDOMNode> tableNode = GetEnclosingTable(cellElement);
      focusElement = do_QueryInterface(tableNode);
      focusTagAtom = nsEditProperty::table;
    }
  }

  // we allow resizers only around images, tables, and absolutely positioned
  // elements. If we don't have image/table, let's look at the latter case.
  if (nsEditProperty::img != focusTagAtom &&
      nsEditProperty::table != focusTagAtom)
    focusElement = absPosElement;

  // at this point, focusElement  contains the element for Resizing,
  //                cellElement   contains the element for InlineTableEditing
  //                absPosElement contains the element for Positioning

  // Note: All the Hide/Show methods below may change attributes on real
  // content which means a DOMAttrModified handler may cause arbitrary
  // side effects while this code runs (bug 420439).

  if (mIsAbsolutelyPositioningEnabled && mAbsolutelyPositionedObject &&
      absPosElement != mAbsolutelyPositionedObject) {
    res = HideGrabber();
    NS_ENSURE_SUCCESS(res, res);
    NS_ASSERTION(!mAbsolutelyPositionedObject, "HideGrabber failed");
  }

  if (mIsObjectResizingEnabled && mResizedObject &&
      mResizedObject != focusElement) {
    res = HideResizers();
    NS_ENSURE_SUCCESS(res, res);
    NS_ASSERTION(!mResizedObject, "HideResizers failed");
  }

  if (mIsInlineTableEditingEnabled && mInlineEditedCell &&
      mInlineEditedCell != cellElement) {
    res = HideInlineTableEditingUI();
    NS_ENSURE_SUCCESS(res, res);
    NS_ASSERTION(!mInlineEditedCell, "HideInlineTableEditingUI failed");
  }

  // now, let's display all contextual UI for good
  nsIContent* hostContent = GetActiveEditingHost();
  nsCOMPtr<nsIDOMNode> hostNode = do_QueryInterface(hostContent);

  if (mIsObjectResizingEnabled && focusElement &&
      IsModifiableNode(focusElement) && focusElement != hostNode) {
    if (nsEditProperty::img == focusTagAtom)
      mResizedObjectIsAnImage = true;
    if (mResizedObject)
      res = RefreshResizers();
    else
      res = ShowResizers(focusElement);
    NS_ENSURE_SUCCESS(res, res);
  }

  if (mIsAbsolutelyPositioningEnabled && absPosElement &&
      IsModifiableNode(absPosElement) && absPosElement != hostNode) {
    if (mAbsolutelyPositionedObject)
      res = RefreshGrabber();
    else
      res = ShowGrabberOnElement(absPosElement);
    NS_ENSURE_SUCCESS(res, res);
  }

  if (mIsInlineTableEditingEnabled && cellElement &&
      IsModifiableNode(cellElement) && cellElement != hostNode) {
    if (mInlineEditedCell)
      res = RefreshInlineTableEditingUI();
    else
      res = ShowInlineTableEditingUI(cellElement);
  }

  return res;
}

// Resizing and Absolute Positioning need to know everything about the
// containing box of the element: position, size, margins, borders
nsresult
nsHTMLEditor::GetPositionAndDimensions(nsIDOMElement * aElement,
                                       int32_t & aX, int32_t & aY,
                                       int32_t & aW, int32_t & aH,
                                       int32_t & aBorderLeft,
                                       int32_t & aBorderTop,
                                       int32_t & aMarginLeft,
                                       int32_t & aMarginTop)
{
  NS_ENSURE_ARG_POINTER(aElement);

  // Is the element positioned ? let's check the cheap way first...
  bool isPositioned = false;
  nsresult res = aElement->HasAttribute(NS_LITERAL_STRING("_moz_abspos"), &isPositioned);
  NS_ENSURE_SUCCESS(res, res);
  if (!isPositioned) {
    // hmmm... the expensive way now...
    nsAutoString positionStr;
    mHTMLCSSUtils->GetComputedProperty(aElement, nsEditProperty::cssPosition,
                                       positionStr);
    isPositioned = positionStr.EqualsLiteral("absolute");
  }

  if (isPositioned) {
    // Yes, it is absolutely positioned
    mResizedObjectIsAbsolutelyPositioned = true;

    // Get the all the computed css styles attached to the element node
    nsRefPtr<nsComputedDOMStyle> cssDecl =
      mHTMLCSSUtils->GetComputedStyle(aElement);
    NS_ENSURE_STATE(cssDecl);

    aBorderLeft = GetCSSFloatValue(cssDecl, NS_LITERAL_STRING("border-left-width"));
    aBorderTop  = GetCSSFloatValue(cssDecl, NS_LITERAL_STRING("border-top-width"));
    aMarginLeft = GetCSSFloatValue(cssDecl, NS_LITERAL_STRING("margin-left"));
    aMarginTop  = GetCSSFloatValue(cssDecl, NS_LITERAL_STRING("margin-top"));

    aX = GetCSSFloatValue(cssDecl, NS_LITERAL_STRING("left")) +
         aMarginLeft + aBorderLeft;
    aY = GetCSSFloatValue(cssDecl, NS_LITERAL_STRING("top")) +
         aMarginTop + aBorderTop;
    aW = GetCSSFloatValue(cssDecl, NS_LITERAL_STRING("width"));
    aH = GetCSSFloatValue(cssDecl, NS_LITERAL_STRING("height"));
  }
  else {
    mResizedObjectIsAbsolutelyPositioned = false;
    nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(aElement);
    if (!htmlElement) {
      return NS_ERROR_NULL_POINTER;
    }
    GetElementOrigin(aElement, aX, aY);

    res = htmlElement->GetOffsetWidth(&aW);
    NS_ENSURE_SUCCESS(res, res);
    res = htmlElement->GetOffsetHeight(&aH);

    aBorderLeft = 0;
    aBorderTop  = 0;
    aMarginLeft = 0;
    aMarginTop = 0;
  }
  return res;
}

// self-explanatory
void
nsHTMLEditor::SetAnonymousElementPosition(int32_t aX, int32_t aY, nsIDOMElement *aElement)
{
  mHTMLCSSUtils->SetCSSPropertyPixels(aElement, NS_LITERAL_STRING("left"), aX);
  mHTMLCSSUtils->SetCSSPropertyPixels(aElement, NS_LITERAL_STRING("top"), aY);
}
