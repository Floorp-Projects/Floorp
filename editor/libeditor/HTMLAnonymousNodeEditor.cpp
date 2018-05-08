/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/CSSPrimitiveValueBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsComputedDOMStyle.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"
#include "nsIContent.h"
#include "nsID.h"
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
#include "nsROCSSPrimitiveValue.h"

class nsIDOMEventListener;

namespace mozilla {

using namespace dom;

// Retrieve the rounded number of CSS pixels from a computed CSS property.
//
// Note that this should only be called for properties whose resolved value
// is CSS pixels (like width, height, left, top, right, bottom, margin, padding,
// border-*-width, ...).
//
// See: https://drafts.csswg.org/cssom/#resolved-values
static int32_t
GetCSSFloatValue(nsComputedDOMStyle* aComputedStyle,
                 const nsAString& aProperty)
{
  MOZ_ASSERT(aComputedStyle);

  // get the computed CSSValue of the property
  nsAutoString value;
  nsresult rv = aComputedStyle->GetPropertyValue(aProperty, value);
  if (NS_FAILED(rv)) {
    return 0;
  }

  // We only care about resolved values, not a big deal if the element is
  // undisplayed, for example, and the value is "auto" or what not.
  int32_t val = value.ToInteger(&rv);
  return NS_SUCCEEDED(rv) ? val : 0;
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

ManualNACPtr
HTMLEditor::CreateAnonymousElement(nsAtom* aTag,
                                   nsIContent& aParentContent,
                                   const nsAString& aAnonClass,
                                   bool aIsCreatedHidden)
{
  // Don't put anonymous editor element into non-HTML element.
  // It is mainly for avoiding other anonymous element being inserted
  // into <svg:use>, but in general we probably don't want to insert
  // some random HTML anonymous element into a non-HTML element.
  if (!aParentContent.IsHTMLElement()) {
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
  RefPtr<Element> newContentRaw = CreateHTMLContent(aTag);
  if (NS_WARN_IF(!newContentRaw)) {
    return nullptr;
  }

  // add the "hidden" class if needed
  if (aIsCreatedHidden) {
    nsresult rv =
      newContentRaw->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                             NS_LITERAL_STRING("hidden"), true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  // add an _moz_anonclass attribute if needed
  if (!aAnonClass.IsEmpty()) {
    nsresult rv =
      newContentRaw->SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_anonclass,
                             aAnonClass, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  {
    nsAutoScriptBlocker scriptBlocker;

    // establish parenthood of the element
    newContentRaw->SetIsNativeAnonymousRoot();
    nsresult rv =
      newContentRaw->BindToTree(doc, &aParentContent, &aParentContent, true);
    if (NS_FAILED(rv)) {
      newContentRaw->UnbindFromTree();
      return nullptr;
    }
  }

  ManualNACPtr newContent(newContentRaw.forget());

  // Must style the new element, otherwise the PostRecreateFramesFor call
  // below will do nothing.
  ServoStyleSet* styleSet = ps->StyleSet();
  // Sometimes editor likes to append anonymous content to elements
  // in display:none subtrees, so avoid styling in those cases.
  if (ServoStyleSet::MayTraverseFrom(newContent)) {
    styleSet->StyleNewSubtree(newContent);
  }

  ElementDeletionObserver* observer =
    new ElementDeletionObserver(newContent, &aParentContent);
  NS_ADDREF(observer); // NodeWillBeDestroyed releases.
  aParentContent.AddMutationObserver(observer);
  newContent->AddMutationObserver(observer);

#ifdef DEBUG
  // Editor anonymous content gets passed to PostRecreateFramesFor... which
  // can't _really_ deal with anonymous content (because it can't get the frame
  // tree ordering right).  But for us the ordering doesn't matter so this is
  // sort of ok.
  newContent->SetProperty(nsGkAtoms::restylableAnonymousNode,
			  reinterpret_cast<void*>(true));
#endif // DEBUG

  // display the element
  ps->PostRecreateFramesFor(newContent);

  return Move(newContent);
}

// Removes event listener and calls DeleteRefToAnonymousNode.
void
HTMLEditor::RemoveListenerAndDeleteRef(const nsAString& aEvent,
                                       nsIDOMEventListener* aListener,
                                       bool aUseCapture,
                                       ManualNACPtr aElement,
                                       nsIPresShell* aShell)
{
  if (aElement) {
    aElement->RemoveEventListener(aEvent, aListener, aUseCapture);
  }
  DeleteRefToAnonymousNode(Move(aElement), aShell);
}

// Deletes all references to an anonymous element
void
HTMLEditor::DeleteRefToAnonymousNode(ManualNACPtr aContent,
                                     nsIPresShell* aShell)
{
  // call ContentRemoved() for the anonymous content
  // node so its references get removed from the frame manager's
  // undisplay map, and its layout frames get destroyed!

  if (NS_WARN_IF(!aContent)) {
    return;
  }

  nsIContent* parentContent = aContent->GetParent();
  if (NS_WARN_IF(!parentContent)) {
    // aContent was already removed?
    return;
  }

  nsAutoScriptBlocker scriptBlocker;
  // Need to check whether aShell has been destroyed (but not yet deleted).
  // See bug 338129.
  if (aContent->IsInComposedDoc() && aShell && !aShell->IsDestroying()) {
    // Call BeginUpdate() so that the nsCSSFrameConstructor/PresShell
    // knows we're messing with the frame tree.
    //
    // FIXME(emilio): Shouldn't this use the document update mechanism instead?
    // Also, is it really needed? This is NAC anyway.
    nsCOMPtr<nsIDocument> document = GetDocument();
    if (document) {
      aShell->BeginUpdate(document, UPDATE_CONTENT_MODEL);
    }

    MOZ_ASSERT(aContent->IsRootOfAnonymousSubtree());
    MOZ_ASSERT(!aContent->GetPreviousSibling(), "NAC has no siblings");

    // FIXME(emilio): This is the only caller to PresShell::ContentRemoved that
    // passes NAC into it. This is not great!
    aShell->ContentRemoved(aContent, nullptr);

    if (document) {
      aShell->EndUpdate(document, UPDATE_CONTENT_MODEL);
    }
  }

  // The ManualNACPtr destructor will invoke UnbindFromTree.
}

// The following method is mostly called by a selection listener. When a
// selection change is notified, the method is called to check if resizing
// handles, a grabber and/or inline table editing UI need to be displayed
// or refreshed
NS_IMETHODIMP
HTMLEditor::CheckSelectionStateForAnonymousButtons(Selection* aSelection)
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

  // let's get the containing element of the selection
  RefPtr<Element> focusElement = GetSelectionContainer();
  NS_ENSURE_TRUE(focusElement, NS_OK);

  // If we're not in a document, don't try to add resizers
  if (!focusElement->IsInUncomposedDoc()) {
    return NS_OK;
  }

  // what's its tag?
  nsAtom* focusTagAtom = focusElement->NodeInfo()->NameAtom();

  RefPtr<Element> absPosElement;
  if (mIsAbsolutelyPositioningEnabled) {
    // Absolute Positioning support is enabled, is the selection contained
    // in an absolutely positioned element ?
    absPosElement = GetAbsolutelyPositionedSelectionContainer();
  }

  RefPtr<Element> cellElement;
  if (mIsObjectResizingEnabled || mIsInlineTableEditingEnabled) {
    // Resizing or Inline Table Editing is enabled, we need to check if the
    // selection is contained in a table cell
    cellElement = GetElementOrParentByTagName(NS_LITERAL_STRING("td"), nullptr);
  }

  if (mIsObjectResizingEnabled && cellElement) {
    // we are here because Resizing is enabled AND selection is contained in
    // a cell

    // get the enclosing table
    if (nsGkAtoms::img != focusTagAtom) {
      // the element container of the selection is not an image, so we'll show
      // the resizers around the table
      focusElement = GetEnclosingTable(cellElement);
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
      absPosElement != mAbsolutelyPositionedObject) {
    HideGrabber();
    NS_ASSERTION(!mAbsolutelyPositionedObject, "HideGrabber failed");
  }

  if (mIsObjectResizingEnabled && mResizedObject &&
      mResizedObject != focusElement) {
    nsresult rv = HideResizers();
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(!mResizedObject, "HideResizers failed");
  }

  if (mIsInlineTableEditingEnabled && mInlineEditedCell &&
      mInlineEditedCell != cellElement) {
    nsresult rv = HideInlineTableEditingUI();
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(!mInlineEditedCell, "HideInlineTableEditingUI failed");
  }

  // now, let's display all contextual UI for good
  nsIContent* hostContent = GetActiveEditingHost();

  if (mIsObjectResizingEnabled && focusElement &&
      IsModifiableNode(focusElement) && focusElement != hostContent) {
    if (nsGkAtoms::img == focusTagAtom) {
      mResizedObjectIsAnImage = true;
    }
    if (mResizedObject) {
      nsresult rv = RefreshResizers();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      nsresult rv = ShowResizers(*focusElement);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  if (mIsAbsolutelyPositioningEnabled && absPosElement &&
      IsModifiableNode(absPosElement) && absPosElement != hostContent) {
    if (mAbsolutelyPositionedObject) {
      nsresult rv = RefreshGrabber();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      nsresult rv = ShowGrabber(*absPosElement);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  if (mIsInlineTableEditingEnabled && cellElement &&
      IsModifiableNode(cellElement) && cellElement != hostContent) {
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
HTMLEditor::GetPositionAndDimensions(Element& aElement,
                                     int32_t& aX,
                                     int32_t& aY,
                                     int32_t& aW,
                                     int32_t& aH,
                                     int32_t& aBorderLeft,
                                     int32_t& aBorderTop,
                                     int32_t& aMarginLeft,
                                     int32_t& aMarginTop)
{
  // Is the element positioned ? let's check the cheap way first...
  bool isPositioned =
    aElement.HasAttr(kNameSpaceID_None, nsGkAtoms::_moz_abspos);
  if (!isPositioned) {
    // hmmm... the expensive way now...
    nsAutoString positionStr;
    CSSEditUtils::GetComputedProperty(aElement, *nsGkAtoms::position,
                                       positionStr);
    isPositioned = positionStr.EqualsLiteral("absolute");
  }

  if (isPositioned) {
    // Yes, it is absolutely positioned
    mResizedObjectIsAbsolutelyPositioned = true;

    // Get the all the computed css styles attached to the element node
    RefPtr<nsComputedDOMStyle> cssDecl =
      CSSEditUtils::GetComputedStyle(&aElement);
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
    RefPtr<nsGenericHTMLElement> htmlElement =
      nsGenericHTMLElement::FromNode(aElement);
    if (!htmlElement) {
      return NS_ERROR_NULL_POINTER;
    }
    GetElementOrigin(aElement, aX, aY);

    aW = htmlElement->OffsetWidth();
    aH = htmlElement->OffsetHeight();

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
                                        Element* aElement)
{
  mCSSEditUtils->SetCSSPropertyPixels(*aElement, *nsGkAtoms::left, aX);
  mCSSEditUtils->SetCSSPropertyPixels(*aElement, *nsGkAtoms::top, aY);
}

} // namespace mozilla
