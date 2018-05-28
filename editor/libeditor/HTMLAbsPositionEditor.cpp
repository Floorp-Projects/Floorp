/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include <math.h>

#include "HTMLEditorObjectResizerUtils.h"
#include "HTMLEditRules.h"
#include "HTMLEditUtils.h"
#include "TextEditUtils.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEditRules.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsCOMPtr.h"
#include "nsComputedDOMStyle.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsROCSSPrimitiveValue.h"
#include "nsIDOMEventListener.h"
#include "nsDOMCSSRGBColor.h"
#include "nsIDOMWindow.h"
#include "nsIHTMLObjectResizer.h"
#include "nsINode.h"
#include "nsIPresShell.h"
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nscore.h"
#include <algorithm>

namespace mozilla {

using namespace dom;

nsresult
HTMLEditor::SetSelectionToAbsoluteOrStatic(bool aEnabled)
{
  AutoPlaceholderBatch beginBatching(this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
                                      *this,
                                      aEnabled ?
                                        EditSubAction::eSetPositionToAbsolute :
                                        EditSubAction::eSetPositionToStatic,
                                      nsIEditor::eNext);

  // the line below does not match the code; should it be removed?
  // Find out if the selection is collapsed:
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  EditSubActionInfo subActionInfo(
                      aEnabled ? EditSubAction::eSetPositionToAbsolute :
                                 EditSubAction::eSetPositionToStatic);
  bool cancel, handled;
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv =
    rules->WillDoAction(selection, subActionInfo, &cancel, &handled);
  if (NS_FAILED(rv) || cancel) {
    return rv;
  }

  return rules->DidDoAction(selection, subActionInfo, rv);
}

already_AddRefed<Element>
HTMLEditor::GetAbsolutelyPositionedSelectionContainer()
{
  nsAutoString positionStr;
  RefPtr<Element> element = GetSelectionContainer();

  while (element && !element->IsHTMLElement(nsGkAtoms::html)) {
    nsresult rv =
      CSSEditUtils::GetComputedProperty(*element, *nsGkAtoms::position,
                                        positionStr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
    if (positionStr.EqualsLiteral("absolute")) {
      return element.forget();
    }
    element = element->GetParentElement();
  }
  return nullptr;
}

NS_IMETHODIMP
HTMLEditor::GetAbsolutePositioningEnabled(bool* aIsEnabled)
{
  *aIsEnabled = AbsolutePositioningEnabled();
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetAbsolutePositioningEnabled(bool aIsEnabled)
{
  mIsAbsolutelyPositioningEnabled = aIsEnabled;
  return NS_OK;
}

nsresult
HTMLEditor::RelativeChangeElementZIndex(Element& aElement,
                                        int32_t aChange,
                                        int32_t* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  if (!aChange) // early way out, no change
    return NS_OK;

  int32_t zIndex = GetZIndex(aElement);
  zIndex = std::max(zIndex + aChange, 0);
  SetZIndex(aElement, zIndex);
  *aReturn = zIndex;

  return NS_OK;
}

void
HTMLEditor::SetZIndex(Element& aElement,
                      int32_t aZindex)
{
  nsAutoString zIndexStr;
  zIndexStr.AppendInt(aZindex);

  mCSSEditUtils->SetCSSProperty(aElement, *nsGkAtoms::z_index, zIndexStr);
}

nsresult
HTMLEditor::AddZIndex(int32_t aChange)
{
  AutoPlaceholderBatch beginBatching(this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
                                      *this,
                                      aChange < 0 ?
                                        EditSubAction::eDecreaseZIndex :
                                        EditSubAction::eIncreaseZIndex,
                                      nsIEditor::eNext);

  // brade: can we get rid of this comment?
  // Find out if the selection is collapsed:
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  EditSubActionInfo subActionInfo(aChange < 0 ? EditSubAction::eDecreaseZIndex :
                                                EditSubAction::eIncreaseZIndex);
  bool cancel, handled;
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv =
    rules->WillDoAction(selection, subActionInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  return rules->DidDoAction(selection, subActionInfo, rv);
}

int32_t
HTMLEditor::GetZIndex(Element& aElement)
{
  nsAutoString zIndexStr;

  nsresult rv =
    CSSEditUtils::GetSpecifiedProperty(aElement, *nsGkAtoms::z_index,
                                       zIndexStr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return 0;
  }
  if (zIndexStr.EqualsLiteral("auto")) {
    // we have to look at the positioned ancestors
    // cf. CSS 2 spec section 9.9.1
    nsCOMPtr<nsINode> node = aElement.GetParentNode();
    nsAutoString positionStr;
    while (node && zIndexStr.EqualsLiteral("auto") &&
           !node->IsHTMLElement(nsGkAtoms::body)) {
      rv = CSSEditUtils::GetComputedProperty(*node, *nsGkAtoms::position,
                                             positionStr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return 0;
      }
      if (positionStr.EqualsLiteral("absolute")) {
        // ah, we found one, what's its z-index ? If its z-index is auto,
        // we have to continue climbing the document's tree
        rv = CSSEditUtils::GetComputedProperty(*node, *nsGkAtoms::z_index,
                                               zIndexStr);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return 0;
        }
      }
      node = node->GetParentNode();
    }
  }

  if (zIndexStr.EqualsLiteral("auto")) {
    return 0;
  }

  nsresult errorCode;
  return zIndexStr.ToInteger(&errorCode);
}

ManualNACPtr
HTMLEditor::CreateGrabber(nsIContent& aParentContent)
{
  // let's create a grabber through the element factory
  ManualNACPtr ret =
    CreateAnonymousElement(nsGkAtoms::span, aParentContent,
                           NS_LITERAL_STRING("mozGrabber"), false);
  if (NS_WARN_IF(!ret)) {
    return nullptr;
  }

  // add the mouse listener so we can detect a click on a resizer
  ret->AddEventListener(NS_LITERAL_STRING("mousedown"),
			mEventListener, false);

  return ret;
}

NS_IMETHODIMP
HTMLEditor::RefreshGrabber()
{
  NS_ENSURE_TRUE(mAbsolutelyPositionedObject, NS_ERROR_NULL_POINTER);

  nsresult rv =
    GetPositionAndDimensions(
      *mAbsolutelyPositionedObject,
      mPositionedObjectX,
      mPositionedObjectY,
      mPositionedObjectWidth,
      mPositionedObjectHeight,
      mPositionedObjectBorderLeft,
      mPositionedObjectBorderTop,
      mPositionedObjectMarginLeft,
      mPositionedObjectMarginTop);
  NS_ENSURE_SUCCESS(rv, rv);

  SetAnonymousElementPosition(mPositionedObjectX+12,
                              mPositionedObjectY-14,
                              mGrabber);
  return NS_OK;
}

void
HTMLEditor::HideGrabber()
{
  nsresult rv = mAbsolutelyPositionedObject->UnsetAttr(kNameSpaceID_None,
                                                       nsGkAtoms::_moz_abspos,
                                                       true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mAbsolutelyPositionedObject = nullptr;
  if (NS_WARN_IF(!mGrabber)) {
    return;
  }

  // get the presshell's document observer interface.
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  // We allow the pres shell to be null; when it is, we presume there
  // are no document observers to notify, but we still want to
  // UnbindFromTree.

  DeleteRefToAnonymousNode(Move(mGrabber), ps);
  DeleteRefToAnonymousNode(Move(mPositioningShadow), ps);
}

nsresult
HTMLEditor::ShowGrabber(Element& aElement)
{
  if (NS_WARN_IF(!IsDescendantOfEditorRoot(&aElement))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mGrabber) {
    NS_ERROR("call HideGrabber first");
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoString classValue;
  nsresult rv =
    GetTemporaryStyleForFocusedPositionedElement(aElement, classValue);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aElement.SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_abspos,
                        classValue, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // first, let's keep track of that element...
  mAbsolutelyPositionedObject = &aElement;

  nsIContent* parentContent = aElement.GetParent();
  if (NS_WARN_IF(!parentContent)) {
    return NS_ERROR_FAILURE;
  }

  mGrabber = CreateGrabber(*parentContent);
  NS_ENSURE_TRUE(mGrabber, NS_ERROR_FAILURE);

  mHasShownGrabber = true;

  // and set its position
  return RefreshGrabber();
}

nsresult
HTMLEditor::StartMoving()
{
  nsCOMPtr<nsIContent> parentContent = mGrabber->GetParent();
  if (NS_WARN_IF(!parentContent) || NS_WARN_IF(!mAbsolutelyPositionedObject)) {
    return NS_ERROR_FAILURE;
  }

  // now, let's create the resizing shadow
  mPositioningShadow =
    CreateShadow(*parentContent, *mAbsolutelyPositionedObject);
  NS_ENSURE_TRUE(mPositioningShadow, NS_ERROR_FAILURE);
  nsresult rv = SetShadowPosition(mPositioningShadow,
                                  mAbsolutelyPositionedObject,
                                  mPositionedObjectX, mPositionedObjectY);
  NS_ENSURE_SUCCESS(rv, rv);

  // make the shadow appear
  mPositioningShadow->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_class, true);

  // position it
  mCSSEditUtils->SetCSSPropertyPixels(*mPositioningShadow, *nsGkAtoms::width,
                                      mPositionedObjectWidth);
  mCSSEditUtils->SetCSSPropertyPixels(*mPositioningShadow, *nsGkAtoms::height,
                                      mPositionedObjectHeight);

  mIsMoving = true;
  return NS_OK; // XXX Looks like nobody refers this result
}

void
HTMLEditor::SnapToGrid(int32_t& newX, int32_t& newY)
{
  if (mSnapToGridEnabled && mGridSize) {
    newX = (int32_t) floor( ((float)newX / (float)mGridSize) + 0.5f ) * mGridSize;
    newY = (int32_t) floor( ((float)newY / (float)mGridSize) + 0.5f ) * mGridSize;
  }
}

nsresult
HTMLEditor::GrabberClicked()
{
  // add a mouse move listener to the editor
  nsresult rv = NS_OK;
  if (!mMouseMotionListenerP) {
    mMouseMotionListenerP = new ResizerMouseMotionListener(*this);
    if (!mMouseMotionListenerP) {return NS_ERROR_NULL_POINTER;}

    EventTarget* piTarget = GetDOMEventTarget();
    NS_ENSURE_TRUE(piTarget, NS_ERROR_FAILURE);

    rv = piTarget->AddEventListener(NS_LITERAL_STRING("mousemove"),
				    mMouseMotionListenerP,
				    false, false);
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "failed to register mouse motion listener");
  }
  mGrabberClicked = true;
  return rv;
}

nsresult
HTMLEditor::EndMoving()
{
  if (mPositioningShadow) {
    nsCOMPtr<nsIPresShell> ps = GetPresShell();
    NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

    DeleteRefToAnonymousNode(Move(mPositioningShadow), ps);

    mPositioningShadow = nullptr;
  }
  RefPtr<EventTarget> piTarget = GetDOMEventTarget();

  if (piTarget && mMouseMotionListenerP) {
    piTarget->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
				  mMouseMotionListenerP,
				  false);
  }
  mMouseMotionListenerP = nullptr;

  mGrabberClicked = false;
  mIsMoving = false;
  RefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return CheckSelectionStateForAnonymousButtons(selection);
}
nsresult
HTMLEditor::SetFinalPosition(int32_t aX,
                             int32_t aY)
{
  nsresult rv = EndMoving();
  NS_ENSURE_SUCCESS(rv, rv);

  // we have now to set the new width and height of the resized object
  // we don't set the x and y position because we don't control that in
  // a normal HTML layout
  int32_t newX = mPositionedObjectX + aX - mOriginalX - (mPositionedObjectBorderLeft+mPositionedObjectMarginLeft);
  int32_t newY = mPositionedObjectY + aY - mOriginalY - (mPositionedObjectBorderTop+mPositionedObjectMarginTop);

  SnapToGrid(newX, newY);

  nsAutoString x, y;
  x.AppendInt(newX);
  y.AppendInt(newY);

  // we want one transaction only from a user's point of view
  AutoPlaceholderBatch batchIt(this);

  nsCOMPtr<Element> absolutelyPositionedObject =
    do_QueryInterface(mAbsolutelyPositionedObject);
  NS_ENSURE_STATE(absolutelyPositionedObject);
  mCSSEditUtils->SetCSSPropertyPixels(*absolutelyPositionedObject,
                                      *nsGkAtoms::top, newY);
  mCSSEditUtils->SetCSSPropertyPixels(*absolutelyPositionedObject,
                                      *nsGkAtoms::left, newX);
  // keep track of that size
  mPositionedObjectX  = newX;
  mPositionedObjectY  = newY;

  return RefreshResizers();
}

void
HTMLEditor::AddPositioningOffset(int32_t& aX,
                                 int32_t& aY)
{
  // Get the positioning offset
  int32_t positioningOffset =
    Preferences::GetInt("editor.positioning.offset", 0);

  aX += positioningOffset;
  aY += positioningOffset;
}

nsresult
HTMLEditor::SetPositionToAbsoluteOrStatic(Element& aElement,
                                          bool aEnabled)
{
  nsAutoString positionStr;
  CSSEditUtils::GetComputedProperty(aElement, *nsGkAtoms::position,
                                    positionStr);
  bool isPositioned = (positionStr.EqualsLiteral("absolute"));

  // nothing to do if the element is already in the state we want
  if (isPositioned == aEnabled) {
    return NS_OK;
  }

  if (aEnabled) {
    return SetPositionToAbsolute(aElement);
  }

  return SetPositionToStatic(aElement);
}

nsresult
HTMLEditor::SetPositionToAbsolute(Element& aElement)
{
  AutoPlaceholderBatch batchIt(this);

  int32_t x, y;
  GetElementOrigin(aElement, x, y);

  mCSSEditUtils->SetCSSProperty(aElement, *nsGkAtoms::position,
                                NS_LITERAL_STRING("absolute"));

  AddPositioningOffset(x, y);
  SnapToGrid(x, y);
  SetTopAndLeft(aElement, x, y);

  // we may need to create a br if the positioned element is alone in its
  // container
  nsINode* parentNode = aElement.GetParentNode();
  if (parentNode->GetChildCount() == 1) {
    RefPtr<Selection> selection = GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }
    RefPtr<Element> newBrElement =
      InsertBrElementWithTransaction(*selection,
                                     EditorRawDOMPoint(parentNode, 0));
    if (NS_WARN_IF(!newBrElement)) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

nsresult
HTMLEditor::SetPositionToStatic(Element& aElement)
{
  AutoPlaceholderBatch batchIt(this);

  mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::position,
                                   EmptyString());
  mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::top,
                                   EmptyString());
  mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::left,
                                   EmptyString());
  mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::z_index,
                                   EmptyString());

  if (!HTMLEditUtils::IsImage(&aElement)) {
    mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::width,
                                     EmptyString());
    mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::height,
                                     EmptyString());
  }

  if (aElement.IsHTMLElement(nsGkAtoms::div) &&
      !HasStyleOrIdOrClass(&aElement)) {
    RefPtr<HTMLEditRules> htmlRules =
      static_cast<HTMLEditRules*>(mRules.get());
    NS_ENSURE_TRUE(htmlRules, NS_ERROR_FAILURE);
    nsresult rv = htmlRules->MakeSureElemStartsAndEndsOnCR(aElement);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = RemoveContainerWithTransaction(aElement);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetSnapToGridEnabled(bool aEnabled)
{
  mSnapToGridEnabled = aEnabled;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetSnapToGridEnabled(bool* aIsEnabled)
{
  *aIsEnabled = mSnapToGridEnabled;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetGridSize(uint32_t aSize)
{
  mGridSize = aSize;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetGridSize(uint32_t* aSize)
{
  *aSize = mGridSize;
  return NS_OK;
}

// self-explanatory
void
HTMLEditor::SetTopAndLeft(Element& aElement,
                          int32_t aX,
                          int32_t aY)
{
  AutoPlaceholderBatch batchIt(this);
  mCSSEditUtils->SetCSSPropertyPixels(aElement, *nsGkAtoms::left, aX);
  mCSSEditUtils->SetCSSPropertyPixels(aElement, *nsGkAtoms::top, aY);
}

nsresult
HTMLEditor::GetTemporaryStyleForFocusedPositionedElement(Element& aElement,
                                                         nsAString& aReturn)
{
  // we are going to outline the positioned element and bring it to the
  // front to overlap any other element intersecting with it. But
  // first, let's see what's the background and foreground colors of the
  // positioned element.
  // if background-image computed value is 'none,
  //   If the background color is 'auto' and R G B values of the foreground are
  //       each above #d0, use a black background
  //   If the background color is 'auto' and at least one of R G B values of
  //       the foreground is below #d0, use a white background
  // Otherwise don't change background/foreground
  aReturn.Truncate();

  nsAutoString bgImageStr;
  nsresult rv =
    CSSEditUtils::GetComputedProperty(aElement, *nsGkAtoms::background_image,
                                      bgImageStr);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!bgImageStr.EqualsLiteral("none")) {
    return NS_OK;
  }

  nsAutoString bgColorStr;
  rv =
    CSSEditUtils::GetComputedProperty(aElement, *nsGkAtoms::backgroundColor,
                                      bgColorStr);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!bgColorStr.EqualsLiteral("rgba(0, 0, 0, 0)")) {
    return NS_OK;
  }

  RefPtr<ComputedStyle> style =
    nsComputedDOMStyle::GetComputedStyle(&aElement, nullptr);
  NS_ENSURE_STATE(style);

  const uint8_t kBlackBgTrigger = 0xd0;

  nscolor color = style->StyleColor()->mColor;
  if (NS_GET_R(color) >= kBlackBgTrigger &&
      NS_GET_G(color) >= kBlackBgTrigger &&
      NS_GET_B(color) >= kBlackBgTrigger) {
    aReturn.AssignLiteral("black");
  } else {
    aReturn.AssignLiteral("white");
  }

  return NS_OK;
}

} // namespace mozilla
