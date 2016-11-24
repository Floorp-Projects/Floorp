/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsComputedDOMStyle.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
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
#include "nsIHTMLInlineTableEditor.h"
#include "nsIHTMLObjectResizer.h"
#include "nsStubMutationObserver.h"
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

namespace mozilla {

using namespace dom;

// retrieve an integer stored into a CSS computed float value
static int32_t GetCSSFloatValue(nsIDOMCSSStyleDeclaration * aDecl,
                                const nsAString & aProperty)
{
  MOZ_ASSERT(aDecl);

  nsCOMPtr<nsIDOMCSSValue> value;
  // get the computed CSSValue of the property
  nsresult rv = aDecl->GetPropertyCSSValue(aProperty, getter_AddRefs(value));
  if (NS_FAILED(rv) || !value) {
    return 0;
  }

  // check the type of the returned CSSValue; we handle here only
  // pixel and enum types
  nsCOMPtr<nsIDOMCSSPrimitiveValue> val = do_QueryInterface(value);
  uint16_t type;
  val->GetPrimitiveType(&type);

  float f = 0;
  switch (type) {
    case nsIDOMCSSPrimitiveValue::CSS_PX:
      // the value is in pixels, just get it
      rv = val->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_PX, &f);
      NS_ENSURE_SUCCESS(rv, 0);
      break;
    case nsIDOMCSSPrimitiveValue::CSS_IDENT: {
      // the value is keyword, we have to map these keywords into
      // numeric values
      nsAutoString str;
      val->GetStringValue(str);
      if (str.EqualsLiteral("thin")) {
        f = 1;
      } else if (str.EqualsLiteral("medium")) {
        f = 3;
      } else if (str.EqualsLiteral("thick")) {
        f = 5;
      }
      break;
    }
  }

  return (int32_t) f;
}

class ElementDeletionObserver final : public nsStubMutationObserver
{
public:
  ElementDeletionObserver(nsIContent* aNativeAnonNode,
                          nsIContent* aObservedNode)
    : mNativeAnonNode(aNativeAnonNode)
    , mObservedNode(aObservedNode)
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

protected:
  ~ElementDeletionObserver() {}
  nsIContent* mNativeAnonNode;
  nsIContent* mObservedNode;
};

NS_IMPL_ISUPPORTS(ElementDeletionObserver, nsIMutationObserver)

void
ElementDeletionObserver::ParentChainChanged(nsIContent* aContent)
{
  // If the native anonymous content has been unbound already in
  // DeleteRefToAnonymousNode, mNativeAnonNode's parentNode is null.
  if (aContent == mObservedNode && mNativeAnonNode &&
      mNativeAnonNode->GetParentNode() == aContent) {
    // If the observed node has been moved to another document, there isn't much
    // we can do easily. But at least be safe and unbind the native anonymous
    // content and stop observing changes.
    if (mNativeAnonNode->OwnerDoc() != mObservedNode->OwnerDoc()) {
      mObservedNode->RemoveMutationObserver(this);
      mObservedNode = nullptr;
      mNativeAnonNode->RemoveMutationObserver(this);
      mNativeAnonNode->UnbindFromTree();
      mNativeAnonNode = nullptr;
      NS_RELEASE_THIS();
      return;
    }

    // We're staying in the same document, just rebind the native anonymous
    // node so that the subtree root points to the right object etc.
    mNativeAnonNode->UnbindFromTree();
    mNativeAnonNode->BindToTree(mObservedNode->GetUncomposedDoc(), mObservedNode,
                                mObservedNode, true);
  }
}

void
ElementDeletionObserver::NodeWillBeDestroyed(const nsINode* aNode)
{
  NS_ASSERTION(aNode == mNativeAnonNode || aNode == mObservedNode,
               "Wrong aNode!");
  if (aNode == mNativeAnonNode) {
    mObservedNode->RemoveMutationObserver(this);
    mObservedNode = nullptr;
  } else {
    mNativeAnonNode->RemoveMutationObserver(this);
    mNativeAnonNode->UnbindFromTree();
    mNativeAnonNode = nullptr;
  }

  NS_RELEASE_THIS();
}

// Returns in *aReturn an anonymous nsDOMElement of type aTag,
// child of aParentNode. If aIsCreatedHidden is true, the class
// "hidden" is added to the created element. If aAnonClass is not
// the empty string, it becomes the value of the attribute "_moz_anonclass"
NS_IMETHODIMP
HTMLEditor::CreateAnonymousElement(const nsAString& aTag,
                                   nsIDOMNode* aParentNode,
                                   const nsAString& aAnonClass,
                                   bool aIsCreatedHidden,
                                   nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nullptr;

  nsCOMPtr<nsIAtom> atom = NS_Atomize(aTag);
  RefPtr<Element> element =
    CreateAnonymousElement(atom, aParentNode, aAnonClass, aIsCreatedHidden);
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMElement> newElement =
    static_cast<nsIDOMElement*>(GetAsDOMNode(element));
  newElement.forget(aReturn);
  return NS_OK;
}

already_AddRefed<Element>
HTMLEditor::CreateAnonymousElement(nsIAtom* aTag,
                                   nsIDOMNode* aParentNode,
                                   const nsAString& aAnonClass,
                                   bool aIsCreatedHidden)
{
  if (NS_WARN_IF(!aParentNode)) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> parentContent = do_QueryInterface(aParentNode);
  if (NS_WARN_IF(!parentContent)) {
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }

  // Get the pres shell
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  if (NS_WARN_IF(!ps)) {
    return nullptr;
  }

  // Create a new node through the element factory
  RefPtr<Element> newContent = CreateHTMLContent(aTag);
  if (NS_WARN_IF(!newContent)) {
    return nullptr;
  }

  // add the "hidden" class if needed
  if (aIsCreatedHidden) {
    nsresult rv =
      newContent->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                          NS_LITERAL_STRING("hidden"), true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  // add an _moz_anonclass attribute if needed
  if (!aAnonClass.IsEmpty()) {
    nsresult rv =
      newContent->SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_anonclass,
                          aAnonClass, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  {
    nsAutoScriptBlocker scriptBlocker;

    // establish parenthood of the element
    newContent->SetIsNativeAnonymousRoot();
    nsresult rv =
      newContent->BindToTree(doc, parentContent, parentContent, true);
    if (NS_FAILED(rv)) {
      newContent->UnbindFromTree();
      return nullptr;
    }
  }

  ElementDeletionObserver* observer =
    new ElementDeletionObserver(newContent, parentContent);
  NS_ADDREF(observer); // NodeWillBeDestroyed releases.
  parentContent->AddMutationObserver(observer);
  newContent->AddMutationObserver(observer);

#ifdef DEBUG
  // Editor anonymous content gets passed to RecreateFramesFor... which can't
  // _really_ deal with anonymous content (because it can't get the frame tree
  // ordering right).  But for us the ordering doesn't matter so this is sort of
  // ok.
  newContent->SetProperty(nsGkAtoms::restylableAnonymousNode,
			  reinterpret_cast<void*>(true));
#endif // DEBUG

  // display the element
  ps->RecreateFramesFor(newContent);

  return newContent.forget();
}

// Removes event listener and calls DeleteRefToAnonymousNode.
void
HTMLEditor::RemoveListenerAndDeleteRef(const nsAString& aEvent,
                                       nsIDOMEventListener* aListener,
                                       bool aUseCapture,
                                       Element* aElement,
                                       nsIContent* aParentContent,
                                       nsIPresShell* aShell)
{
  nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(aElement));
  if (evtTarget) {
    evtTarget->RemoveEventListener(aEvent, aListener, aUseCapture);
  }
  DeleteRefToAnonymousNode(static_cast<nsIDOMElement*>(GetAsDOMNode(aElement)), aParentContent, aShell);
}

// Deletes all references to an anonymous element
void
HTMLEditor::DeleteRefToAnonymousNode(nsIDOMElement* aElement,
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
      if (content->IsInComposedDoc() && aShell && aShell->GetPresContext() &&
          aShell->GetPresContext()->GetPresShell() == aShell) {
        nsCOMPtr<nsIDocumentObserver> docObserver = do_QueryInterface(aShell);
        if (docObserver) {
          // Call BeginUpdate() so that the nsCSSFrameConstructor/PresShell
          // knows we're messing with the frame tree.
          nsCOMPtr<nsIDocument> document = GetDocument();
          if (document) {
            docObserver->BeginUpdate(document, UPDATE_CONTENT_MODEL);
          }

          // XXX This is wrong (bug 439258).  Once it's fixed, the NS_WARNING
          // in RestyleManager::RestyleForRemove should be changed back
          // to an assertion.
          docObserver->ContentRemoved(content->GetComposedDoc(),
                                      aParentContent, content, -1,
                                      content->GetPreviousSibling());
          if (document) {
            docObserver->EndUpdate(document, UPDATE_CONTENT_MODEL);
          }
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
HTMLEditor::CheckSelectionStateForAnonymousButtons(nsISelection* aSelection)
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
  nsresult rv = GetSelectionContainer(getter_AddRefs(focusElement));
  NS_ENSURE_TRUE(focusElement, NS_OK);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're not in a document, don't try to add resizers
  nsCOMPtr<dom::Element> focusElementNode = do_QueryInterface(focusElement);
  NS_ENSURE_STATE(focusElementNode);
  if (!focusElementNode->IsInUncomposedDoc()) {
    return NS_OK;
  }

  // what's its tag?
  nsAutoString focusTagName;
  rv = focusElement->GetTagName(focusTagName);
  NS_ENSURE_SUCCESS(rv, rv);
  ToLowerCase(focusTagName);
  nsCOMPtr<nsIAtom> focusTagAtom = NS_Atomize(focusTagName);

  nsCOMPtr<nsIDOMElement> absPosElement;
  if (mIsAbsolutelyPositioningEnabled) {
    // Absolute Positioning support is enabled, is the selection contained
    // in an absolutely positioned element ?
    rv =
      GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(absPosElement));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIDOMElement> cellElement;
  if (mIsObjectResizingEnabled || mIsInlineTableEditingEnabled) {
    // Resizing or Inline Table Editing is enabled, we need to check if the
    // selection is contained in a table cell
    rv = GetElementOrParentByTagName(NS_LITERAL_STRING("td"),
                                     nullptr,
                                     getter_AddRefs(cellElement));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mIsObjectResizingEnabled && cellElement) {
    // we are here because Resizing is enabled AND selection is contained in
    // a cell

    // get the enclosing table
    if (nsGkAtoms::img != focusTagAtom) {
      // the element container of the selection is not an image, so we'll show
      // the resizers around the table
      nsCOMPtr<nsIDOMNode> tableNode = GetEnclosingTable(cellElement);
      focusElement = do_QueryInterface(tableNode);
      focusTagAtom = nsGkAtoms::table;
    }
  }

  // we allow resizers only around images, tables, and absolutely positioned
  // elements. If we don't have image/table, let's look at the latter case.
  if (nsGkAtoms::img != focusTagAtom && nsGkAtoms::table != focusTagAtom) {
    focusElement = absPosElement;
  }

  // at this point, focusElement  contains the element for Resizing,
  //                cellElement   contains the element for InlineTableEditing
  //                absPosElement contains the element for Positioning

  // Note: All the Hide/Show methods below may change attributes on real
  // content which means a DOMAttrModified handler may cause arbitrary
  // side effects while this code runs (bug 420439).

  if (mIsAbsolutelyPositioningEnabled && mAbsolutelyPositionedObject &&
      absPosElement != GetAsDOMNode(mAbsolutelyPositionedObject)) {
    rv = HideGrabber();
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(!mAbsolutelyPositionedObject, "HideGrabber failed");
  }

  if (mIsObjectResizingEnabled && mResizedObject &&
      GetAsDOMNode(mResizedObject) != focusElement) {
    rv = HideResizers();
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(!mResizedObject, "HideResizers failed");
  }

  if (mIsInlineTableEditingEnabled && mInlineEditedCell &&
      mInlineEditedCell != cellElement) {
    rv = HideInlineTableEditingUI();
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(!mInlineEditedCell, "HideInlineTableEditingUI failed");
  }

  // now, let's display all contextual UI for good
  nsIContent* hostContent = GetActiveEditingHost();
  nsCOMPtr<nsIDOMNode> hostNode = do_QueryInterface(hostContent);

  if (mIsObjectResizingEnabled && focusElement &&
      IsModifiableNode(focusElement) && focusElement != hostNode) {
    if (nsGkAtoms::img == focusTagAtom) {
      mResizedObjectIsAnImage = true;
    }
    if (mResizedObject) {
      nsresult rv = RefreshResizers();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      nsresult rv = ShowResizers(focusElement);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  if (mIsAbsolutelyPositioningEnabled && absPosElement &&
      IsModifiableNode(absPosElement) && absPosElement != hostNode) {
    if (mAbsolutelyPositionedObject) {
      nsresult rv = RefreshGrabber();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      nsresult rv = ShowGrabberOnElement(absPosElement);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  if (mIsInlineTableEditingEnabled && cellElement &&
      IsModifiableNode(cellElement) && cellElement != hostNode) {
    if (mInlineEditedCell) {
      nsresult rv = RefreshInlineTableEditingUI();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      nsresult rv = ShowInlineTableEditingUI(cellElement);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}

// Resizing and Absolute Positioning need to know everything about the
// containing box of the element: position, size, margins, borders
nsresult
HTMLEditor::GetPositionAndDimensions(nsIDOMElement* aElement,
                                     int32_t& aX,
                                     int32_t& aY,
                                     int32_t& aW,
                                     int32_t& aH,
                                     int32_t& aBorderLeft,
                                     int32_t& aBorderTop,
                                     int32_t& aMarginLeft,
                                     int32_t& aMarginTop)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  // Is the element positioned ? let's check the cheap way first...
  bool isPositioned = false;
  nsresult rv =
    aElement->HasAttribute(NS_LITERAL_STRING("_moz_abspos"), &isPositioned);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isPositioned) {
    // hmmm... the expensive way now...
    nsAutoString positionStr;
    mCSSEditUtils->GetComputedProperty(*element, *nsGkAtoms::position,
                                       positionStr);
    isPositioned = positionStr.EqualsLiteral("absolute");
  }

  if (isPositioned) {
    // Yes, it is absolutely positioned
    mResizedObjectIsAbsolutelyPositioned = true;

    // Get the all the computed css styles attached to the element node
    RefPtr<nsComputedDOMStyle> cssDecl =
      mCSSEditUtils->GetComputedStyle(element);
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
  } else {
    mResizedObjectIsAbsolutelyPositioned = false;
    nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(aElement);
    if (!htmlElement) {
      return NS_ERROR_NULL_POINTER;
    }
    GetElementOrigin(aElement, aX, aY);

    if (NS_WARN_IF(NS_FAILED(htmlElement->GetOffsetWidth(&aW))) ||
        NS_WARN_IF(NS_FAILED(htmlElement->GetOffsetHeight(&aH)))) {
      return rv;
    }

    aBorderLeft = 0;
    aBorderTop  = 0;
    aMarginLeft = 0;
    aMarginTop = 0;
  }
  return NS_OK;
}

// self-explanatory
void
HTMLEditor::SetAnonymousElementPosition(int32_t aX,
                                        int32_t aY,
                                        nsIDOMElement* aElement)
{
  mCSSEditUtils->SetCSSPropertyPixels(aElement, NS_LITERAL_STRING("left"), aX);
  mCSSEditUtils->SetCSSPropertyPixels(aElement, NS_LITERAL_STRING("top"), aY);
}

} // namespace mozilla
