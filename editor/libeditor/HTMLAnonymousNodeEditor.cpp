/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditor.h"

#include "HTMLEditUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/dom/BindContext.h"
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
#include "mozilla/dom/Document.h"
#include "nsIDocumentObserver.h"
#include "nsStubMutationObserver.h"
#include "nsINode.h"
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"
#include "nsLiteralString.h"
#include "nsPresContext.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsUnicharUtils.h"
#include "nscore.h"
#include "nsContentUtils.h"  // for nsAutoScriptBlocker
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
static int32_t GetCSSFloatValue(nsComputedDOMStyle* aComputedStyle,
                                const nsACString& aProperty) {
  MOZ_ASSERT(aComputedStyle);

  // get the computed CSSValue of the property
  nsAutoString value;
  nsresult rv = aComputedStyle->GetPropertyValue(aProperty, value);
  if (NS_FAILED(rv)) {
    NS_WARNING("nsComputedDOMStyle::GetPropertyValue() failed");
    return 0;
  }

  // We only care about resolved values, not a big deal if the element is
  // undisplayed, for example, and the value is "auto" or what not.
  int32_t val = value.ToInteger(&rv);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "nsAString::ToInteger() failed");
  return NS_SUCCEEDED(rv) ? val : 0;
}

class ElementDeletionObserver final : public nsStubMutationObserver {
 public:
  ElementDeletionObserver(nsIContent* aNativeAnonNode,
                          Element* aObservedElement)
      : mNativeAnonNode(aNativeAnonNode), mObservedElement(aObservedElement) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

 protected:
  ~ElementDeletionObserver() = default;
  nsIContent* mNativeAnonNode;
  Element* mObservedElement;
};

NS_IMPL_ISUPPORTS(ElementDeletionObserver, nsIMutationObserver)

void ElementDeletionObserver::ParentChainChanged(nsIContent* aContent) {
  // If the native anonymous content has been unbound already in
  // DeleteRefToAnonymousNode, mNativeAnonNode's parentNode is null.
  if (aContent != mObservedElement || !mNativeAnonNode ||
      mNativeAnonNode->GetParent() != aContent) {
    return;
  }

  ManualNACPtr::RemoveContentFromNACArray(mNativeAnonNode);

  mObservedElement->RemoveMutationObserver(this);
  mObservedElement = nullptr;
  mNativeAnonNode->RemoveMutationObserver(this);
  mNativeAnonNode = nullptr;
  NS_RELEASE_THIS();
}

void ElementDeletionObserver::NodeWillBeDestroyed(const nsINode* aNode) {
  NS_ASSERTION(aNode == mNativeAnonNode || aNode == mObservedElement,
               "Wrong aNode!");
  if (aNode == mNativeAnonNode) {
    mObservedElement->RemoveMutationObserver(this);
    mObservedElement = nullptr;
  } else {
    mNativeAnonNode->RemoveMutationObserver(this);
    mNativeAnonNode->UnbindFromTree();
    mNativeAnonNode = nullptr;
  }

  NS_RELEASE_THIS();
}

ManualNACPtr HTMLEditor::CreateAnonymousElement(nsAtom* aTag,
                                                nsIContent& aParentContent,
                                                const nsAString& aAnonClass,
                                                bool aIsCreatedHidden) {
  // Don't put anonymous editor element into non-HTML element.
  // It is mainly for avoiding other anonymous element being inserted
  // into <svg:use>, but in general we probably don't want to insert
  // some random HTML anonymous element into a non-HTML element.
  if (!aParentContent.IsHTMLElement()) {
    return nullptr;
  }

  if (NS_WARN_IF(!GetDocument())) {
    return nullptr;
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return nullptr;
  }

  // Create a new node through the element factory
  RefPtr<Element> newElement = CreateHTMLContent(aTag);
  if (!newElement) {
    NS_WARNING("EditorBase::CreateHTMLContent() failed");
    return nullptr;
  }

  // add the "hidden" class if needed
  if (aIsCreatedHidden) {
    nsresult rv = newElement->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                                      NS_LITERAL_STRING("hidden"), true);
    if (NS_FAILED(rv)) {
      NS_WARNING("Element::SetAttr(nsGkAtoms::_class, hidden) failed");
      return nullptr;
    }
  }

  // add an _moz_anonclass attribute if needed
  if (!aAnonClass.IsEmpty()) {
    nsresult rv = newElement->SetAttr(
        kNameSpaceID_None, nsGkAtoms::_moz_anonclass, aAnonClass, true);
    if (NS_FAILED(rv)) {
      NS_WARNING("Element::SetAttr(nsGkAtoms::_moz_anonclass) failed");
      return nullptr;
    }
  }

  {
    nsAutoScriptBlocker scriptBlocker;

    // establish parenthood of the element
    newElement->SetIsNativeAnonymousRoot();
    BindContext context(*aParentContent.AsElement(),
                        BindContext::ForNativeAnonymous);
    nsresult rv = newElement->BindToTree(context, aParentContent);
    if (NS_FAILED(rv)) {
      NS_WARNING("Element::BindToTree(BindContext::ForNativeAnonymous) failed");
      newElement->UnbindFromTree();
      return nullptr;
    }
  }

  ManualNACPtr newNativeAnonymousContent(newElement.forget());

  // Must style the new element, otherwise the PostRecreateFramesFor call
  // below will do nothing.
  ServoStyleSet* styleSet = presShell->StyleSet();
  // Sometimes editor likes to append anonymous content to elements
  // in display:none subtrees, so avoid styling in those cases.
  if (ServoStyleSet::MayTraverseFrom(newNativeAnonymousContent)) {
    styleSet->StyleNewSubtree(newNativeAnonymousContent);
  }

  ElementDeletionObserver* observer = new ElementDeletionObserver(
      newNativeAnonymousContent, aParentContent.AsElement());
  NS_ADDREF(observer);  // NodeWillBeDestroyed releases.
  aParentContent.AddMutationObserver(observer);
  newNativeAnonymousContent->AddMutationObserver(observer);

#ifdef DEBUG
  // Editor anonymous content gets passed to PostRecreateFramesFor... which
  // can't _really_ deal with anonymous content (because it can't get the frame
  // tree ordering right).  But for us the ordering doesn't matter so this is
  // sort of ok.
  newNativeAnonymousContent->SetProperty(nsGkAtoms::restylableAnonymousNode,
                                         reinterpret_cast<void*>(true));
#endif  // DEBUG

  // display the element
  presShell->PostRecreateFramesFor(newNativeAnonymousContent);

  return newNativeAnonymousContent;
}

// Removes event listener and calls DeleteRefToAnonymousNode.
void HTMLEditor::RemoveListenerAndDeleteRef(const nsAString& aEvent,
                                            nsIDOMEventListener* aListener,
                                            bool aUseCapture,
                                            ManualNACPtr aElement,
                                            PresShell* aPresShell) {
  if (aElement) {
    aElement->RemoveEventListener(aEvent, aListener, aUseCapture);
  }
  DeleteRefToAnonymousNode(std::move(aElement), aPresShell);
}

// Deletes all references to an anonymous element
void HTMLEditor::DeleteRefToAnonymousNode(ManualNACPtr aContent,
                                          PresShell* aPresShell) {
  // call ContentRemoved() for the anonymous content
  // node so its references get removed from the frame manager's
  // undisplay map, and its layout frames get destroyed!

  if (NS_WARN_IF(!aContent)) {
    return;
  }

  if (NS_WARN_IF(!aContent->GetParent())) {
    // aContent was already removed?
    return;
  }

  nsAutoScriptBlocker scriptBlocker;
  // Need to check whether aPresShell has been destroyed (but not yet deleted).
  // See bug 338129.
  if (aContent->IsInComposedDoc() && aPresShell &&
      !aPresShell->IsDestroying()) {
    MOZ_ASSERT(aContent->IsRootOfAnonymousSubtree());
    MOZ_ASSERT(!aContent->GetPreviousSibling(), "NAC has no siblings");

    // FIXME(emilio): This is the only caller to PresShell::ContentRemoved that
    // passes NAC into it. This is not great!
    aPresShell->ContentRemoved(aContent, nullptr);
  }

  // The ManualNACPtr destructor will invoke UnbindFromTree.
}

void HTMLEditor::HideAnonymousEditingUIs() {
  if (mAbsolutelyPositionedObject) {
    HideGrabberInternal();
    NS_ASSERTION(!mAbsolutelyPositionedObject,
                 "HTMLEditor::HideGrabberInternal() failed, but ignored");
  }
  if (mInlineEditedCell) {
    HideInlineTableEditingUIInternal();
    NS_ASSERTION(
        !mInlineEditedCell,
        "HTMLEditor::HideInlineTableEditingUIInternal() failed, but ignored");
  }
  if (mResizedObject) {
    DebugOnly<nsresult> rvIgnored = HideResizersInternal();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::HideResizersInternal() failed, but ignored");
    NS_ASSERTION(!mResizedObject,
                 "HTMLEditor::HideResizersInternal() failed, but ignored");
  }
}

void HTMLEditor::HideAnonymousEditingUIsIfUnnecessary() {
  // XXX Perhaps, this is wrong approach to hide multiple UIs because
  //     hiding one UI may causes overwriting existing UI with newly
  //     created one.  In such case, we will leak ovewritten UI.
  if (!IsAbsolutePositionEditorEnabled() && mAbsolutelyPositionedObject) {
    // XXX If we're moving something, we need to cancel or commit the
    //     operation now.
    HideGrabberInternal();
    NS_ASSERTION(!mAbsolutelyPositionedObject,
                 "HTMLEditor::HideGrabberInternal() failed, but ignored");
  }
  if (!IsInlineTableEditorEnabled() && mInlineEditedCell) {
    // XXX If we're resizing a table element, we need to cancel or commit the
    //     operation now.
    HideInlineTableEditingUIInternal();
    NS_ASSERTION(
        !mInlineEditedCell,
        "HTMLEditor::HideInlineTableEditingUIInternal() failed, but ignored");
  }
  if (!IsObjectResizerEnabled() && mResizedObject) {
    // XXX If we're resizing something, we need to cancel or commit the
    //     operation now.
    DebugOnly<nsresult> rvIgnored = HideResizersInternal();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::HideResizersInternal() failed, but ignored");
    NS_ASSERTION(!mResizedObject,
                 "HTMLEditor::HideResizersInternal() failed, but ignored");
  }
}

NS_IMETHODIMP HTMLEditor::CheckSelectionStateForAnonymousButtons() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = RefreshEditingUI();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::RefereshEditingUI() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::RefreshEditingUI() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // First, we need to remove unnecessary editing UI now since some of them
  // may be disabled while them are visible.
  HideAnonymousEditingUIsIfUnnecessary();

  // early way out if all contextual UI extensions are disabled
  if (!IsObjectResizerEnabled() && !IsAbsolutePositionEditorEnabled() &&
      !IsInlineTableEditorEnabled()) {
    return NS_OK;
  }

  // Don't change selection state if we're moving.
  if (mIsMoving) {
    return NS_OK;
  }

  // let's get the containing element of the selection
  RefPtr<Element> selectionContainerElement = GetSelectionContainerElement();
  if (NS_WARN_IF(!selectionContainerElement)) {
    return NS_OK;
  }

  // If we're not in a document, don't try to add resizers
  if (!selectionContainerElement->IsInUncomposedDoc()) {
    return NS_OK;
  }

  // what's its tag?
  RefPtr<Element> focusElement = std::move(selectionContainerElement);
  nsAtom* focusTagAtom = focusElement->NodeInfo()->NameAtom();

  RefPtr<Element> absPosElement;
  if (IsAbsolutePositionEditorEnabled()) {
    // Absolute Positioning support is enabled, is the selection contained
    // in an absolutely positioned element ?
    absPosElement = GetAbsolutelyPositionedSelectionContainer();
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
  }

  RefPtr<Element> cellElement;
  if (IsObjectResizerEnabled() || IsInlineTableEditorEnabled()) {
    // Resizing or Inline Table Editing is enabled, we need to check if the
    // selection is contained in a table cell
    cellElement = GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::td);
  }

  if (IsObjectResizerEnabled() && cellElement) {
    // we are here because Resizing is enabled AND selection is contained in
    // a cell

    // get the enclosing table
    if (nsGkAtoms::img != focusTagAtom) {
      // the element container of the selection is not an image, so we'll show
      // the resizers around the table
      // XXX There may be a bug.  cellElement may be not in <table> in invalid
      //     tree.  So, perhaps, GetClosestAncestorTableElement() returns
      //     nullptr, we should not set focusTagAtom to nsGkAtoms::table.
      focusElement =
          HTMLEditUtils::GetClosestAncestorTableElement(*cellElement);
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

  if (IsAbsolutePositionEditorEnabled() && mAbsolutelyPositionedObject &&
      absPosElement != mAbsolutelyPositionedObject) {
    HideGrabberInternal();
    NS_ASSERTION(!mAbsolutelyPositionedObject,
                 "HTMLEditor::HideGrabberInternal() failed, but ignored");
  }

  if (IsObjectResizerEnabled() && mResizedObject &&
      mResizedObject != focusElement) {
    // Perhaps, even if HideResizersInternal() failed, we should try to hide
    // inline table editing UI.  However, it returns error only when we cannot
    // do anything.  So, it's okay for now.
    nsresult rv = HideResizersInternal();
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::HideResizersInternal() failed");
      return rv;
    }
    NS_ASSERTION(!mResizedObject,
                 "HTMLEditor::HideResizersInternal() failed, but ignored");
  }

  if (IsInlineTableEditorEnabled() && mInlineEditedCell &&
      mInlineEditedCell != cellElement) {
    HideInlineTableEditingUIInternal();
    NS_ASSERTION(
        !mInlineEditedCell,
        "HTMLEditor::HideInlineTableEditingUIInternal failed, but ignored");
  }

  // now, let's display all contextual UI for good
  nsIContent* hostContent = GetActiveEditingHost();

  if (IsObjectResizerEnabled() && focusElement &&
      HTMLEditUtils::IsSimplyEditableNode(*focusElement) &&
      focusElement != hostContent) {
    if (nsGkAtoms::img == focusTagAtom) {
      mResizedObjectIsAnImage = true;
    }
    if (mResizedObject) {
      nsresult rv = RefreshResizersInternal();
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::RefreshResizersInternal() failed");
        return rv;
      }
    } else {
      nsresult rv = ShowResizersInternal(*focusElement);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::ShowResizersInternal() failed");
        return rv;
      }
    }
  }

  if (IsAbsolutePositionEditorEnabled() && absPosElement &&
      HTMLEditUtils::IsSimplyEditableNode(*absPosElement) &&
      absPosElement != hostContent) {
    if (mAbsolutelyPositionedObject) {
      nsresult rv = RefreshGrabberInternal();
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::RefreshGrabberInternal() failed");
        return rv;
      }
    } else {
      nsresult rv = ShowGrabberInternal(*absPosElement);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::ShowGrabberInternal() failed");
        return rv;
      }
    }
  }

  // XXX Shouldn't we check whether the `<table>` element is editable or not?
  if (IsInlineTableEditorEnabled() && cellElement &&
      HTMLEditUtils::IsSimplyEditableNode(*cellElement) &&
      cellElement != hostContent) {
    if (mInlineEditedCell) {
      nsresult rv = RefreshInlineTableEditingUIInternal();
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::RefreshInlineTableEditingUIInternal() failed");
        return rv;
      }
    } else {
      nsresult rv = ShowInlineTableEditingUIInternal(*cellElement);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::ShowInlineTableEditingUIInternal() failed");
        return rv;
      }
    }
  }

  return NS_OK;
}

// Resizing and Absolute Positioning need to know everything about the
// containing box of the element: position, size, margins, borders
nsresult HTMLEditor::GetPositionAndDimensions(Element& aElement, int32_t& aX,
                                              int32_t& aY, int32_t& aW,
                                              int32_t& aH, int32_t& aBorderLeft,
                                              int32_t& aBorderTop,
                                              int32_t& aMarginLeft,
                                              int32_t& aMarginTop) {
  // Is the element positioned ? let's check the cheap way first...
  bool isPositioned =
      aElement.HasAttr(kNameSpaceID_None, nsGkAtoms::_moz_abspos);
  if (!isPositioned) {
    // hmmm... the expensive way now...
    nsAutoString positionValue;
    DebugOnly<nsresult> rvIgnored = CSSEditUtils::GetComputedProperty(
        aElement, *nsGkAtoms::position, positionValue);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "CSSEditUtils::GetComputedProperty(nsGkAtoms::"
                         "position) failed, but ignored");
    isPositioned = positionValue.EqualsLiteral("absolute");
  }

  if (isPositioned) {
    // Yes, it is absolutely positioned
    mResizedObjectIsAbsolutelyPositioned = true;

    // Get the all the computed css styles attached to the element node
    RefPtr<nsComputedDOMStyle> computedDOMStyle =
        CSSEditUtils::GetComputedStyle(&aElement);
    if (NS_WARN_IF(!computedDOMStyle)) {
      return NS_ERROR_FAILURE;
    }

    aBorderLeft = GetCSSFloatValue(computedDOMStyle,
                                   NS_LITERAL_CSTRING("border-left-width"));
    aBorderTop = GetCSSFloatValue(computedDOMStyle,
                                  NS_LITERAL_CSTRING("border-top-width"));
    aMarginLeft =
        GetCSSFloatValue(computedDOMStyle, NS_LITERAL_CSTRING("margin-left"));
    aMarginTop =
        GetCSSFloatValue(computedDOMStyle, NS_LITERAL_CSTRING("margin-top"));

    aX = GetCSSFloatValue(computedDOMStyle, NS_LITERAL_CSTRING("left")) +
         aMarginLeft + aBorderLeft;
    aY = GetCSSFloatValue(computedDOMStyle, NS_LITERAL_CSTRING("top")) +
         aMarginTop + aBorderTop;
    aW = GetCSSFloatValue(computedDOMStyle, NS_LITERAL_CSTRING("width"));
    aH = GetCSSFloatValue(computedDOMStyle, NS_LITERAL_CSTRING("height"));
  } else {
    mResizedObjectIsAbsolutelyPositioned = false;
    RefPtr<nsGenericHTMLElement> htmlElement =
        nsGenericHTMLElement::FromNode(aElement);
    if (!htmlElement) {
      return NS_ERROR_NULL_POINTER;
    }
    DebugOnly<nsresult> rvIgnored = GetElementOrigin(aElement, aX, aY);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "HTMLEditor::GetElementOrigin() failed, but ignored");

    aW = htmlElement->OffsetWidth();
    aH = htmlElement->OffsetHeight();

    aBorderLeft = 0;
    aBorderTop = 0;
    aMarginLeft = 0;
    aMarginTop = 0;
  }
  return NS_OK;
}

// self-explanatory
void HTMLEditor::SetAnonymousElementPosition(int32_t aX, int32_t aY,
                                             Element* aElement) {
  DebugOnly<nsresult> rvIgnored = NS_OK;
  rvIgnored =
      mCSSEditUtils->SetCSSPropertyPixels(*aElement, *nsGkAtoms::left, aX);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::left) "
                       "failed, but ignored");
  rvIgnored =
      mCSSEditUtils->SetCSSPropertyPixels(*aElement, *nsGkAtoms::top, aY);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::top) "
                       "failed, but ignored");
}

}  // namespace mozilla
