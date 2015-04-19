/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "mozilla/Preferences.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Element.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsComputedDOMStyle.h"
#include "nsDebug.h"
#include "nsEditRules.h"
#include "nsEditor.h"
#include "nsEditorUtils.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsHTMLCSSUtils.h"
#include "nsHTMLEditRules.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"
#include "nsHTMLObjectResizer.h"
#include "nsIContent.h"
#include "nsROCSSPrimitiveValue.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNode.h"
#include "nsDOMCSSRGBColor.h"
#include "nsIDOMWindow.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLObjectResizer.h"
#include "nsINode.h"
#include "nsIPresShell.h"
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTextEditRules.h"
#include "nsTextEditUtils.h"
#include "nscore.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

#define  BLACK_BG_RGB_TRIGGER 0xd0

NS_IMETHODIMP
nsHTMLEditor::AbsolutePositionSelection(bool aEnabled)
{
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this,
                                 aEnabled ? EditAction::setAbsolutePosition :
                                            EditAction::removeAbsolutePosition,
                                 nsIEditor::eNext);
  
  // the line below does not match the code; should it be removed?
  // Find out if the selection is collapsed:
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsTextRulesInfo ruleInfo(aEnabled ? EditAction::setAbsolutePosition :
                                      EditAction::removeAbsolutePosition);
  bool cancel, handled;
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res) || cancel)
    return res;
  
  return mRules->DidDoAction(selection, &ruleInfo, res);
}

NS_IMETHODIMP
nsHTMLEditor::GetAbsolutelyPositionedSelectionContainer(nsIDOMElement **_retval)
{
  nsCOMPtr<nsIDOMElement> element;
  nsresult res = GetSelectionContainer(getter_AddRefs(element));
  NS_ENSURE_SUCCESS(res, res);

  nsAutoString positionStr;
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(element);
  nsCOMPtr<nsIDOMNode> resultNode;

  while (!resultNode && node && !nsEditor::NodeIsType(node, nsGkAtoms::html)) {
    res = mHTMLCSSUtils->GetComputedProperty(node, nsGkAtoms::position,
                                             positionStr);
    NS_ENSURE_SUCCESS(res, res);
    if (positionStr.EqualsLiteral("absolute"))
      resultNode = node;
    else {
      nsCOMPtr<nsIDOMNode> parentNode;
      res = node->GetParentNode(getter_AddRefs(parentNode));
      NS_ENSURE_SUCCESS(res, res);
      node.swap(parentNode);
    }
  }

  element = do_QueryInterface(resultNode ); 
  *_retval = element;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetSelectionContainerAbsolutelyPositioned(bool *aIsSelectionContainerAbsolutelyPositioned)
{
  *aIsSelectionContainerAbsolutelyPositioned = (mAbsolutelyPositionedObject != nullptr);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetAbsolutePositioningEnabled(bool * aIsEnabled)
{
  *aIsEnabled = mIsAbsolutelyPositioningEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetAbsolutePositioningEnabled(bool aIsEnabled)
{
  mIsAbsolutelyPositioningEnabled = aIsEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::RelativeChangeElementZIndex(nsIDOMElement * aElement,
                                          int32_t aChange,
                                          int32_t * aReturn)
{
  NS_ENSURE_ARG_POINTER(aElement);
  NS_ENSURE_ARG_POINTER(aReturn);
  if (!aChange) // early way out, no change
    return NS_OK;

  int32_t zIndex;
  nsresult res = GetElementZIndex(aElement, &zIndex);
  NS_ENSURE_SUCCESS(res, res);

  zIndex = std::max(zIndex + aChange, 0);
  SetElementZIndex(aElement, zIndex);
  *aReturn = zIndex;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetElementZIndex(nsIDOMElement* aElement, int32_t aZindex)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  nsAutoString zIndexStr;
  zIndexStr.AppendInt(aZindex);

  mHTMLCSSUtils->SetCSSProperty(*element, *nsGkAtoms::z_index, zIndexStr);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::RelativeChangeZIndex(int32_t aChange)
{
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this,
                                 (aChange < 0) ? EditAction::decreaseZIndex :
                                                 EditAction::increaseZIndex,
                                 nsIEditor::eNext);
  
  // brade: can we get rid of this comment?
  // Find out if the selection is collapsed:
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsTextRulesInfo ruleInfo(aChange < 0 ? EditAction::decreaseZIndex :
                                         EditAction::increaseZIndex);
  bool cancel, handled;
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(res))
    return res;
  
  return mRules->DidDoAction(selection, &ruleInfo, res);
}

NS_IMETHODIMP
nsHTMLEditor::GetElementZIndex(nsIDOMElement * aElement,
                               int32_t * aZindex)
{
  nsAutoString zIndexStr;
  *aZindex = 0;

  nsresult res = mHTMLCSSUtils->GetSpecifiedProperty(aElement,
                                                     nsGkAtoms::z_index,
                                                     zIndexStr);
  NS_ENSURE_SUCCESS(res, res);
  if (zIndexStr.EqualsLiteral("auto")) {
    // we have to look at the positioned ancestors
    // cf. CSS 2 spec section 9.9.1
    nsCOMPtr<nsIDOMNode> parentNode;
    res = aElement->GetParentNode(getter_AddRefs(parentNode));
    NS_ENSURE_SUCCESS(res, res);
    nsCOMPtr<nsIDOMNode> node = parentNode;
    nsAutoString positionStr;
    while (node && 
           zIndexStr.EqualsLiteral("auto") &&
           !nsTextEditUtils::IsBody(node)) {
      res = mHTMLCSSUtils->GetComputedProperty(node, nsGkAtoms::position,
                                               positionStr);
      NS_ENSURE_SUCCESS(res, res);
      if (positionStr.EqualsLiteral("absolute")) {
        // ah, we found one, what's its z-index ? If its z-index is auto,
        // we have to continue climbing the document's tree
        res = mHTMLCSSUtils->GetComputedProperty(node, nsGkAtoms::z_index,
                                                 zIndexStr);
        NS_ENSURE_SUCCESS(res, res);
      }
      res = node->GetParentNode(getter_AddRefs(parentNode));
      NS_ENSURE_SUCCESS(res, res);
      node = parentNode;
    }
  }

  if (!zIndexStr.EqualsLiteral("auto")) {
    nsresult errorCode;
    *aZindex = zIndexStr.ToInteger(&errorCode);
  }

  return NS_OK;
}

nsresult
nsHTMLEditor::CreateGrabber(nsIDOMNode * aParentNode, nsIDOMElement ** aReturn)
{
  // let's create a grabber through the element factory
  nsresult res = CreateAnonymousElement(NS_LITERAL_STRING("span"),
                                        aParentNode,
                                        NS_LITERAL_STRING("mozGrabber"),
                                        false,
                                        aReturn);

  NS_ENSURE_TRUE(*aReturn, NS_ERROR_FAILURE);

  // add the mouse listener so we can detect a click on a resizer
  nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(*aReturn));
  evtTarget->AddEventListener(NS_LITERAL_STRING("mousedown"),
                              mEventListener, false);

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::RefreshGrabber()
{
  NS_ENSURE_TRUE(mAbsolutelyPositionedObject, NS_ERROR_NULL_POINTER);

  nsresult res = GetPositionAndDimensions(mAbsolutelyPositionedObject,
                                         mPositionedObjectX,
                                         mPositionedObjectY,
                                         mPositionedObjectWidth,
                                         mPositionedObjectHeight,
                                         mPositionedObjectBorderLeft,
                                         mPositionedObjectBorderTop,
                                         mPositionedObjectMarginLeft,
                                         mPositionedObjectMarginTop);

  NS_ENSURE_SUCCESS(res, res);

  SetAnonymousElementPosition(mPositionedObjectX+12,
                              mPositionedObjectY-14,
                              mGrabber);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::HideGrabber()
{
  nsresult res =
    mAbsolutelyPositionedObject->RemoveAttribute(NS_LITERAL_STRING("_moz_abspos"));
  NS_ENSURE_SUCCESS(res, res);

  mAbsolutelyPositionedObject = nullptr;
  NS_ENSURE_TRUE(mGrabber, NS_ERROR_NULL_POINTER);

  // get the presshell's document observer interface.
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  // We allow the pres shell to be null; when it is, we presume there
  // are no document observers to notify, but we still want to
  // UnbindFromTree.

  nsCOMPtr<nsIDOMNode> parentNode;
  res = mGrabber->GetParentNode(getter_AddRefs(parentNode));
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIContent> parentContent = do_QueryInterface(parentNode);
  NS_ENSURE_TRUE(parentContent, NS_ERROR_NULL_POINTER);

  DeleteRefToAnonymousNode(mGrabber, parentContent, ps);
  mGrabber = nullptr;
  DeleteRefToAnonymousNode(mPositioningShadow, parentContent, ps);
  mPositioningShadow = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::ShowGrabberOnElement(nsIDOMElement * aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);

  if (mGrabber) {
    NS_ERROR("call HideGrabber first");
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoString classValue;
  nsresult res = CheckPositionedElementBGandFG(aElement, classValue);
  NS_ENSURE_SUCCESS(res, res);

  res = aElement->SetAttribute(NS_LITERAL_STRING("_moz_abspos"),
                               classValue);
  NS_ENSURE_SUCCESS(res, res);

  // first, let's keep track of that element...
  mAbsolutelyPositionedObject = aElement;

  nsCOMPtr<nsIDOMNode> parentNode;
  res = aElement->GetParentNode(getter_AddRefs(parentNode));
  NS_ENSURE_SUCCESS(res, res);

  res = CreateGrabber(parentNode, getter_AddRefs(mGrabber));
  NS_ENSURE_SUCCESS(res, res);

  // and set its position
  return RefreshGrabber();
}

nsresult
nsHTMLEditor::StartMoving(nsIDOMElement *aHandle)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  nsresult res = mGrabber->GetParentNode(getter_AddRefs(parentNode));
  NS_ENSURE_SUCCESS(res, res);

  // now, let's create the resizing shadow
  res = CreateShadow(getter_AddRefs(mPositioningShadow),
                                 parentNode, mAbsolutelyPositionedObject);
  NS_ENSURE_SUCCESS(res,res);
  res = SetShadowPosition(mPositioningShadow, mAbsolutelyPositionedObject,
                             mPositionedObjectX, mPositionedObjectY);
  NS_ENSURE_SUCCESS(res,res);

  // make the shadow appear
  mPositioningShadow->RemoveAttribute(NS_LITERAL_STRING("class"));

  // position it
  mHTMLCSSUtils->SetCSSPropertyPixels(mPositioningShadow,
                                      NS_LITERAL_STRING("width"),
                                      mPositionedObjectWidth);
  mHTMLCSSUtils->SetCSSPropertyPixels(mPositioningShadow,
                                      NS_LITERAL_STRING("height"),
                                      mPositionedObjectHeight);

  mIsMoving = true;
  return res;
}

void
nsHTMLEditor::SnapToGrid(int32_t & newX, int32_t & newY)
{
  if (mSnapToGridEnabled && mGridSize) {
    newX = (int32_t) floor( ((float)newX / (float)mGridSize) + 0.5f ) * mGridSize;
    newY = (int32_t) floor( ((float)newY / (float)mGridSize) + 0.5f ) * mGridSize;
  }
}

nsresult
nsHTMLEditor::GrabberClicked()
{
  // add a mouse move listener to the editor
  nsresult res = NS_OK;
  if (!mMouseMotionListenerP) {
    mMouseMotionListenerP = new ResizerMouseMotionListener(this);
    if (!mMouseMotionListenerP) {return NS_ERROR_NULL_POINTER;}

    nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();
    NS_ENSURE_TRUE(piTarget, NS_ERROR_FAILURE);

    res = piTarget->AddEventListener(NS_LITERAL_STRING("mousemove"),
                                     mMouseMotionListenerP,
                                     false, false);
    NS_ASSERTION(NS_SUCCEEDED(res),
                 "failed to register mouse motion listener");
  }
  mGrabberClicked = true;
  return res;
}

nsresult
nsHTMLEditor::EndMoving()
{
  if (mPositioningShadow) {
    nsCOMPtr<nsIPresShell> ps = GetPresShell();
    NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIDOMNode> parentNode;
    nsresult res = mGrabber->GetParentNode(getter_AddRefs(parentNode));
    NS_ENSURE_SUCCESS(res, res);

    nsCOMPtr<nsIContent> parentContent( do_QueryInterface(parentNode) );
    NS_ENSURE_TRUE(parentContent, NS_ERROR_FAILURE);

    DeleteRefToAnonymousNode(mPositioningShadow, parentContent, ps);

    mPositioningShadow = nullptr;
  }
  nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();

  if (piTarget && mMouseMotionListenerP) {
#ifdef DEBUG
    nsresult res =
#endif
    piTarget->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                  mMouseMotionListenerP,
                                  false);
    NS_ASSERTION(NS_SUCCEEDED(res), "failed to remove mouse motion listener");
  }
  mMouseMotionListenerP = nullptr;

  mGrabberClicked = false;
  mIsMoving = false;
  nsRefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return CheckSelectionStateForAnonymousButtons(selection);
}
nsresult
nsHTMLEditor::SetFinalPosition(int32_t aX, int32_t aY)
{
  nsresult res = EndMoving();
  NS_ENSURE_SUCCESS(res, res);

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
  nsAutoEditBatch batchIt(this);

  nsCOMPtr<Element> absolutelyPositionedObject =
    do_QueryInterface(mAbsolutelyPositionedObject);
  NS_ENSURE_STATE(absolutelyPositionedObject);
  mHTMLCSSUtils->SetCSSPropertyPixels(*absolutelyPositionedObject,
                                      *nsGkAtoms::top, newY);
  mHTMLCSSUtils->SetCSSPropertyPixels(*absolutelyPositionedObject,
                                      *nsGkAtoms::left, newX);
  // keep track of that size
  mPositionedObjectX  = newX;
  mPositionedObjectY  = newY;

  return RefreshResizers();
}

void
nsHTMLEditor::AddPositioningOffset(int32_t & aX, int32_t & aY)
{
  // Get the positioning offset
  int32_t positioningOffset =
    Preferences::GetInt("editor.positioning.offset", 0);

  aX += positioningOffset;
  aY += positioningOffset;
}

NS_IMETHODIMP
nsHTMLEditor::AbsolutelyPositionElement(nsIDOMElement* aElement,
                                        bool aEnabled)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  nsAutoString positionStr;
  mHTMLCSSUtils->GetComputedProperty(aElement, nsGkAtoms::position,
                                     positionStr);
  bool isPositioned = (positionStr.EqualsLiteral("absolute"));

  // nothing to do if the element is already in the state we want
  if (isPositioned == aEnabled)
    return NS_OK;

  nsAutoEditBatch batchIt(this);

  if (aEnabled) {
    int32_t x, y;
    GetElementOrigin(aElement, x, y);

    mHTMLCSSUtils->SetCSSProperty(*element, *nsGkAtoms::position,
                                  NS_LITERAL_STRING("absolute"));

    AddPositioningOffset(x, y);
    SnapToGrid(x, y);
    SetElementPosition(aElement, x, y);

    // we may need to create a br if the positioned element is alone in its
    // container
    nsCOMPtr<nsINode> element = do_QueryInterface(aElement);
    NS_ENSURE_STATE(element);

    nsINode* parentNode = element->GetParentNode();
    if (parentNode->GetChildCount() == 1) {
      nsCOMPtr<nsIDOMNode> brNode;
      nsresult res = CreateBR(parentNode->AsDOMNode(), 0, address_of(brNode));
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  else {
    mHTMLCSSUtils->RemoveCSSProperty(*element, *nsGkAtoms::position,
                                     EmptyString());
    mHTMLCSSUtils->RemoveCSSProperty(*element, *nsGkAtoms::top,
                                     EmptyString());
    mHTMLCSSUtils->RemoveCSSProperty(*element, *nsGkAtoms::left,
                                     EmptyString());
    mHTMLCSSUtils->RemoveCSSProperty(*element, *nsGkAtoms::z_index,
                                     EmptyString());

    if (!nsHTMLEditUtils::IsImage(aElement)) {
      mHTMLCSSUtils->RemoveCSSProperty(*element, *nsGkAtoms::width,
                                       EmptyString());
      mHTMLCSSUtils->RemoveCSSProperty(*element, *nsGkAtoms::height,
                                       EmptyString());
    }

    nsCOMPtr<dom::Element> element = do_QueryInterface(aElement);
    if (element && element->IsHTMLElement(nsGkAtoms::div) &&
        !HasStyleOrIdOrClass(element)) {
      nsRefPtr<nsHTMLEditRules> htmlRules = static_cast<nsHTMLEditRules*>(mRules.get());
      NS_ENSURE_TRUE(htmlRules, NS_ERROR_FAILURE);
      nsresult res = htmlRules->MakeSureElemStartsOrEndsOnCR(aElement);
      NS_ENSURE_SUCCESS(res, res);
      res = RemoveContainer(element);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetSnapToGridEnabled(bool aEnabled)
{
  mSnapToGridEnabled = aEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetSnapToGridEnabled(bool * aIsEnabled)
{
  *aIsEnabled = mSnapToGridEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetGridSize(uint32_t aSize)
{
  mGridSize = aSize;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetGridSize(uint32_t * aSize)
{
  *aSize = mGridSize;
  return NS_OK;
}

// self-explanatory
NS_IMETHODIMP
nsHTMLEditor::SetElementPosition(nsIDOMElement *aElement, int32_t aX, int32_t aY)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_STATE(element);
  nsAutoEditBatch batchIt(this);

  mHTMLCSSUtils->SetCSSPropertyPixels(*element, *nsGkAtoms::left, aX);
  mHTMLCSSUtils->SetCSSPropertyPixels(*element, *nsGkAtoms::top, aY);
  return NS_OK;
}

// self-explanatory
NS_IMETHODIMP
nsHTMLEditor::GetPositionedElement(nsIDOMElement ** aReturn)
{
  *aReturn = mAbsolutelyPositionedObject;
  NS_IF_ADDREF(*aReturn);
  return NS_OK;
}

nsresult
nsHTMLEditor::CheckPositionedElementBGandFG(nsIDOMElement * aElement,
                                            nsAString & aReturn)
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
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_STATE(element || !aElement);

  aReturn.Truncate();
  
  nsAutoString bgImageStr;
  nsresult res =
    mHTMLCSSUtils->GetComputedProperty(aElement, nsGkAtoms::background_image,
                                       bgImageStr);
  NS_ENSURE_SUCCESS(res, res);
  if (bgImageStr.EqualsLiteral("none")) {
    nsAutoString bgColorStr;
    res =
      mHTMLCSSUtils->GetComputedProperty(aElement, nsGkAtoms::backgroundColor,
                                         bgColorStr);
    NS_ENSURE_SUCCESS(res, res);
    if (bgColorStr.EqualsLiteral("transparent")) {
      nsRefPtr<nsComputedDOMStyle> cssDecl =
        mHTMLCSSUtils->GetComputedStyle(element);
      NS_ENSURE_STATE(cssDecl);

      // from these declarations, get the one we want and that one only
      ErrorResult error;
      nsRefPtr<dom::CSSValue> cssVal = cssDecl->GetPropertyCSSValue(NS_LITERAL_STRING("color"), error);
      NS_ENSURE_SUCCESS(error.ErrorCode(), error.ErrorCode());

      nsROCSSPrimitiveValue* val = cssVal->AsPrimitiveValue();
      NS_ENSURE_TRUE(val, NS_ERROR_FAILURE);

      if (nsIDOMCSSPrimitiveValue::CSS_RGBCOLOR == val->PrimitiveType()) {
        nsDOMCSSRGBColor* rgbVal = val->GetRGBColorValue(error);
        NS_ENSURE_SUCCESS(error.ErrorCode(), error.ErrorCode());
        float r = rgbVal->Red()->
          GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, error);
        NS_ENSURE_SUCCESS(error.ErrorCode(), error.ErrorCode());
        float g = rgbVal->Green()->
          GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, error);
        NS_ENSURE_SUCCESS(error.ErrorCode(), error.ErrorCode());
        float b = rgbVal->Blue()->
          GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, error);
        NS_ENSURE_SUCCESS(error.ErrorCode(), error.ErrorCode());
        if (r >= BLACK_BG_RGB_TRIGGER &&
            g >= BLACK_BG_RGB_TRIGGER &&
            b >= BLACK_BG_RGB_TRIGGER)
          aReturn.AssignLiteral("black");
        else
          aReturn.AssignLiteral("white");
        return NS_OK;
      }
    }
  }

  return NS_OK;
}
