/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.org.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman (glazman@netscape.com) (Original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <math.h>

#include "nsHTMLEditor.h"

#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIEditor.h"
#include "nsIPresShell.h"

#include "nsISelection.h"

#include "nsTextEditUtils.h"
#include "nsEditorUtils.h"
#include "nsHTMLEditUtils.h"
#include "nsTextEditRules.h"
#include "nsIHTMLEditRules.h"

#include "nsIDOMHTMLElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMNodeList.h"

#include "nsIDOMEventTarget.h"
#include "nsIDOMEventReceiver.h"

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"

#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMRGBColor.h"

#define  BLACK_BG_RGB_TRIGGER 0xd0

NS_IMETHODIMP
nsHTMLEditor::AbsolutePositionSelection(PRBool aEnabled)
{
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this,
                                 aEnabled ? kOpSetAbsolutePosition :
                                            kOpRemoveAbsolutePosition,
                                 nsIEditor::eNext);
  
  // the line below does not match the code; should it be removed?
  // Find out if the selection is collapsed:
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(aEnabled ?
                           nsTextEditRules::kSetAbsolutePosition :
                           nsTextEditRules::kRemoveAbsolutePosition);
  PRBool cancel, handled;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res) || cancel)
    return res;
  
  return mRules->DidDoAction(selection, &ruleInfo, res);
}

NS_IMETHODIMP
nsHTMLEditor::GetAbsolutelyPositionedSelectionContainer(nsIDOMElement **_retval)
{
  nsCOMPtr<nsIDOMElement> element;
  nsresult res = GetSelectionContainer(getter_AddRefs(element));
  if (NS_FAILED(res)) return res;

  nsAutoString positionStr;
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(element);
  nsCOMPtr<nsIDOMNode> resultNode;

  do {
    res = mHTMLCSSUtils->GetComputedProperty(node, nsEditProperty::cssPosition,
                                             positionStr);
    if (NS_FAILED(res)) return res;
    if (positionStr.EqualsLiteral("absolute"))
      resultNode = node;
    else {
      nsCOMPtr<nsIDOMNode> parentNode;
      res = node->GetParentNode(getter_AddRefs(parentNode));
      if (NS_FAILED(res)) return res;
      node.swap(parentNode);
    }
  } while (!resultNode &&
           !nsEditor::NodeIsType(node, nsEditProperty::html));

  element = do_QueryInterface(resultNode ); 
  *_retval = element;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetSelectionContainerAbsolutelyPositioned(PRBool *aIsSelectionContainerAbsolutelyPositioned)
{
  *aIsSelectionContainerAbsolutelyPositioned = (mAbsolutelyPositionedObject != nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetAbsolutePositioningEnabled(PRBool * aIsEnabled)
{
  *aIsEnabled = mIsAbsolutelyPositioningEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetAbsolutePositioningEnabled(PRBool aIsEnabled)
{
  mIsAbsolutelyPositioningEnabled = aIsEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::RelativeChangeElementZIndex(nsIDOMElement * aElement,
                                          PRInt32 aChange,
                                          PRInt32 * aReturn)
{
  NS_ENSURE_ARG_POINTER(aElement);
  NS_ENSURE_ARG_POINTER(aReturn);
  if (!aChange) // early way out, no change
    return NS_OK;

  PRInt32 zIndex;
  nsresult res = GetElementZIndex(aElement, &zIndex);
  if (NS_FAILED(res)) return res;

  zIndex = PR_MAX(zIndex + aChange, 0);
  SetElementZIndex(aElement, zIndex);
  *aReturn = zIndex;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetElementZIndex(nsIDOMElement * aElement,
                               PRInt32 aZindex)
{
  NS_ENSURE_ARG_POINTER(aElement);
  
  nsAutoString zIndexStr;
  zIndexStr.AppendInt(aZindex);

  mHTMLCSSUtils->SetCSSProperty(aElement,
                                nsEditProperty::cssZIndex,
                                zIndexStr,
                                PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::RelativeChangeZIndex(PRInt32 aChange)
{
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this,
                                 (aChange < 0) ? kOpDecreaseZIndex :
                                                 kOpIncreaseZIndex,
                                 nsIEditor::eNext);
  
  // brade: can we get rid of this comment?
  // Find out if the selection is collapsed:
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsTextRulesInfo ruleInfo((aChange < 0) ? nsTextEditRules::kDecreaseZIndex:
                                           nsTextEditRules::kIncreaseZIndex);
  PRBool cancel, handled;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(res))
    return res;
  
  return mRules->DidDoAction(selection, &ruleInfo, res);
}

NS_IMETHODIMP
nsHTMLEditor::GetElementZIndex(nsIDOMElement * aElement,
                               PRInt32 * aZindex)
{
  nsAutoString zIndexStr;
  *aZindex = 0;

  nsresult res = mHTMLCSSUtils->GetSpecifiedProperty(aElement,
                                                     nsEditProperty::cssZIndex,
                                                     zIndexStr);
  if (NS_FAILED(res)) return res;
  if (zIndexStr.EqualsLiteral("auto")) {
    // we have to look at the positioned ancestors
    // cf. CSS 2 spec section 9.9.1
    nsCOMPtr<nsIDOMNode> parentNode;
    res = aElement->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsIDOMNode> node = parentNode;
    nsAutoString positionStr;
    while (node && 
           zIndexStr.EqualsLiteral("auto") &&
           !nsTextEditUtils::IsBody(node)) {
      res = mHTMLCSSUtils->GetComputedProperty(node,
                                               nsEditProperty::cssPosition,
                                               positionStr);
      if (NS_FAILED(res)) return res;
      if (positionStr.EqualsLiteral("absolute")) {
        // ah, we found one, what's its z-index ? If its z-index is auto,
        // we have to continue climbing the document's tree
        res = mHTMLCSSUtils->GetComputedProperty(node,
                                                 nsEditProperty::cssZIndex,
                                                 zIndexStr);
        if (NS_FAILED(res)) return res;
      }
      res = node->GetParentNode(getter_AddRefs(parentNode));
      if (NS_FAILED(res)) return res;
      node = parentNode;
    }
  }

  if (!zIndexStr.EqualsLiteral("auto")) {
    PRInt32 errorCode;
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
                                        PR_FALSE,
                                        aReturn);

  if (!*aReturn)
    return NS_ERROR_FAILURE;

  // add the mouse listener so we can detect a click on a resizer
  nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(*aReturn));
  evtTarget->AddEventListener(NS_LITERAL_STRING("mousedown"), mMouseListenerP, PR_FALSE);

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

  if (NS_FAILED(res)) return res;

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
  if (NS_FAILED(res)) return res;

  mAbsolutelyPositionedObject = nsnull;
  NS_ENSURE_TRUE(mGrabber, NS_ERROR_NULL_POINTER);

  // get the presshell's document observer interface.
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDocumentObserver> docObserver(do_QueryInterface(ps));
  if (!docObserver) return NS_ERROR_FAILURE;

  // get the root content node.

  nsCOMPtr<nsIDOMElement> bodyElement;
  res = GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIContent> bodyContent = do_QueryInterface(bodyElement);
  if (!bodyContent) return NS_ERROR_NULL_POINTER;

  DeleteRefToAnonymousNode(mGrabber, bodyContent, docObserver);
  mGrabber = nsnull;
  DeleteRefToAnonymousNode(mPositioningShadow, bodyContent, docObserver);
  mPositioningShadow = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::ShowGrabberOnElement(nsIDOMElement * aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);

  nsAutoString classValue;
  nsresult res = CheckPositionedElementBGandFG(aElement, classValue);
  if (NS_FAILED(res)) return res;

  res = aElement->SetAttribute(NS_LITERAL_STRING("_moz_abspos"),
                               classValue);
  if (NS_FAILED(res)) return res;

  // first, let's keep track of that element...
  mAbsolutelyPositionedObject = aElement;

  nsCOMPtr<nsIDOMElement> bodyElement;
  res = GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement)   return NS_ERROR_NULL_POINTER;

  res = CreateGrabber(bodyElement, getter_AddRefs(mGrabber));
  if (NS_FAILED(res)) return res;
  // and set its position
  return RefreshGrabber();
}

nsresult
nsHTMLEditor::StartMoving(nsIDOMElement *aHandle)
{
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult result = GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(result)) return result;
  if (!bodyElement)   return NS_ERROR_NULL_POINTER;

  // now, let's create the resizing shadow
  result = CreateShadow(getter_AddRefs(mPositioningShadow), bodyElement,
                        mAbsolutelyPositionedObject);
  if (NS_FAILED(result)) return result;
  result = SetShadowPosition(mPositioningShadow, mAbsolutelyPositionedObject,
                             mPositionedObjectX, mPositionedObjectY);
  if (NS_FAILED(result)) return result;

  // make the shadow appear
  mPositioningShadow->RemoveAttribute(NS_LITERAL_STRING("class"));

  // position it
  mHTMLCSSUtils->SetCSSPropertyPixels(mPositioningShadow,
                                      NS_LITERAL_STRING("width"),
                                      mPositionedObjectWidth);
  mHTMLCSSUtils->SetCSSPropertyPixels(mPositioningShadow,
                                      NS_LITERAL_STRING("height"),
                                      mPositionedObjectHeight);

  mIsMoving = PR_TRUE;
  return result;
}

void
nsHTMLEditor::SnapToGrid(PRInt32 & newX, PRInt32 & newY)
{
  if (mSnapToGridEnabled && mGridSize) {
    newX = (PRInt32) floor( ((float)newX / (float)mGridSize) + 0.5f ) * mGridSize;
    newY = (PRInt32) floor( ((float)newY / (float)mGridSize) + 0.5f ) * mGridSize;
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

    nsCOMPtr<nsIDOMEventReceiver> erP;
    res = GetDOMEventReceiver(getter_AddRefs(erP));
    if (NS_SUCCEEDED(res ))
    {
      res = erP->AddEventListenerByIID(mMouseMotionListenerP, NS_GET_IID(nsIDOMMouseMotionListener));
      NS_ASSERTION(NS_SUCCEEDED(res), "failed to register mouse motion listener");
    }
    else
      HandleEventListenerError();
  }
  mGrabberClicked = PR_TRUE;
  return res;
}

nsresult
nsHTMLEditor::EndMoving()
{
  if (mPositioningShadow) {
    nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
    if (!ps) return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIDocumentObserver> docObserver(do_QueryInterface(ps));
    if (!docObserver) return NS_ERROR_FAILURE;

    // get the root content node.

    nsCOMPtr<nsIDOMElement> bodyElement;
    nsresult res = GetRootElement(getter_AddRefs(bodyElement));
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsIContent> bodyContent( do_QueryInterface(bodyElement) );
    if (!bodyContent) return NS_ERROR_FAILURE;

    DeleteRefToAnonymousNode(mPositioningShadow, bodyContent, docObserver);
    mPositioningShadow = nsnull;
  }
  nsCOMPtr<nsIDOMEventReceiver> erP;
  nsresult res = GetDOMEventReceiver(getter_AddRefs(erP));

  if (NS_SUCCEEDED(res) && erP && mMouseMotionListenerP) {
    res = erP->RemoveEventListenerByIID(mMouseMotionListenerP, NS_GET_IID(nsIDOMMouseMotionListener));
    NS_ASSERTION(NS_SUCCEEDED(res), "failed to remove mouse motion listener");
  }
  mMouseMotionListenerP = nsnull;

  return NS_OK;
}
nsresult
nsHTMLEditor::SetFinalPosition(PRInt32 aX, PRInt32 aY)
{
  nsresult res = EndMoving();
  mGrabberClicked = PR_FALSE;
  mIsMoving = PR_FALSE;
  if (NS_FAILED(res)) return res;

  // we have now to set the new width and height of the resized object
  // we don't set the x and y position because we don't control that in
  // a normal HTML layout
  PRInt32 newX = mPositionedObjectX + aX - mOriginalX - (mPositionedObjectBorderLeft+mPositionedObjectMarginLeft);
  PRInt32 newY = mPositionedObjectY + aY - mOriginalY - (mPositionedObjectBorderTop+mPositionedObjectMarginTop);

  SnapToGrid(newX, newY);

  nsAutoString x, y;
  x.AppendInt(newX);
  y.AppendInt(newY);

  // we want one transaction only from a user's point of view
  nsAutoEditBatch batchIt(this);

  mHTMLCSSUtils->SetCSSPropertyPixels(mAbsolutelyPositionedObject,
                                      nsEditProperty::cssTop,
                                      newY,
                                      PR_FALSE);
  mHTMLCSSUtils->SetCSSPropertyPixels(mAbsolutelyPositionedObject,
                                      nsEditProperty::cssLeft,
                                      newX,
                                      PR_FALSE);
  // keep track of that size
  mPositionedObjectX  = newX;
  mPositionedObjectY  = newY;

  return RefreshResizers();
}

void
nsHTMLEditor::AddPositioningOffet(PRInt32 & aX, PRInt32 & aY)
{
  // Get the positioning offset
  nsresult res;
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &res);
  PRInt32 positioningOffset = 0;
  if (NS_SUCCEEDED(res) && prefBranch) {
    res = prefBranch->GetIntPref("editor.positioning.offset", &positioningOffset);
    if (NS_FAILED(res)) // paranoia
      positioningOffset = 0;
  }

  aX += positioningOffset;
  aY += positioningOffset;
}

NS_IMETHODIMP
nsHTMLEditor::AbsolutelyPositionElement(nsIDOMElement * aElement,
                                        PRBool aEnabled)
{
  NS_ENSURE_ARG_POINTER(aElement);

  nsAutoString positionStr;
  mHTMLCSSUtils->GetComputedProperty(aElement, nsEditProperty::cssPosition,
                                     positionStr);
  PRBool isPositioned = (positionStr.EqualsLiteral("absolute"));

  // nothing to do if the element is already in the state we want
  if (isPositioned == aEnabled)
    return NS_OK;

  nsAutoEditBatch batchIt(this);
  nsresult res;

  if (aEnabled) {
    PRInt32 x, y;
    GetElementOrigin(aElement, x, y);

    mHTMLCSSUtils->SetCSSProperty(aElement,
                                  nsEditProperty::cssPosition,
                                  NS_LITERAL_STRING("absolute"),
                                  PR_FALSE);

    AddPositioningOffet(x, y);
    SnapToGrid(x, y);
    SetElementPosition(aElement, x, y);

    // we may need to create a br if the positioned element is alone in its
    // container
    nsCOMPtr<nsIDOMNode> parentNode;
    res = aElement->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsIDOMNodeList> childNodes;
    res = parentNode->GetChildNodes(getter_AddRefs(childNodes));
    if (NS_FAILED(res)) return res;
    if (!childNodes) return NS_ERROR_NULL_POINTER;
    PRUint32 childCount;
    res = childNodes->GetLength(&childCount);
    if (NS_FAILED(res)) return res;

    if (childCount == 1) {
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateBR(parentNode, 0, address_of(brNode));
    }
  }
  else {
    nsAutoString emptyStr;

    mHTMLCSSUtils->RemoveCSSProperty(aElement,
                                     nsEditProperty::cssPosition,
                                     emptyStr, PR_FALSE);
    mHTMLCSSUtils->RemoveCSSProperty(aElement,
                                     nsEditProperty::cssTop,
                                     emptyStr, PR_FALSE);
    mHTMLCSSUtils->RemoveCSSProperty(aElement,
                                     nsEditProperty::cssLeft,
                                     emptyStr, PR_FALSE);
    mHTMLCSSUtils->RemoveCSSProperty(aElement,
                                     nsEditProperty::cssZIndex,
                                     emptyStr, PR_FALSE);

    if (!nsHTMLEditUtils::IsImage(aElement)) {
      mHTMLCSSUtils->RemoveCSSProperty(aElement,
                                       nsEditProperty::cssWidth,
                                       emptyStr, PR_FALSE);
      mHTMLCSSUtils->RemoveCSSProperty(aElement,
                                       nsEditProperty::cssHeight,
                                       emptyStr, PR_FALSE);
    }

    PRBool hasStyleOrIdOrClass;
    res = HasStyleOrIdOrClass(aElement, &hasStyleOrIdOrClass);
    if (NS_FAILED(res)) return res;
    if (!hasStyleOrIdOrClass && nsHTMLEditUtils::IsDiv(aElement)) {
      nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
      if (!htmlRules) return NS_ERROR_FAILURE;
      res = htmlRules->MakeSureElemStartsOrEndsOnCR(aElement);
      if (NS_FAILED(res)) return res;
      res = RemoveContainer(aElement);
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetSnapToGridEnabled(PRBool aEnabled)
{
  mSnapToGridEnabled = aEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetSnapToGridEnabled(PRBool * aIsEnabled)
{
  *aIsEnabled = mSnapToGridEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetGridSize(PRUint32 aSize)
{
  mGridSize = aSize;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetGridSize(PRUint32 * aSize)
{
  *aSize = mGridSize;
  return NS_OK;
}

// self-explanatory
NS_IMETHODIMP
nsHTMLEditor::SetElementPosition(nsIDOMElement *aElement, PRInt32 aX, PRInt32 aY)
{
  nsAutoEditBatch batchIt(this);

  mHTMLCSSUtils->SetCSSPropertyPixels(aElement,
                                      nsEditProperty::cssLeft,
                                      aX,
                                      PR_FALSE);
  mHTMLCSSUtils->SetCSSPropertyPixels(aElement,
                                      nsEditProperty::cssTop,
                                      aY,
                                      PR_FALSE);
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

  aReturn.Truncate();
  
  nsAutoString bgImageStr;
  nsresult res =
    mHTMLCSSUtils->GetComputedProperty(aElement,
                                       nsEditProperty::cssBackgroundImage,
                                       bgImageStr);
  if (NS_FAILED(res)) return res;
  if (bgImageStr.EqualsLiteral("none")) {
    nsAutoString bgColorStr;
    res =
      mHTMLCSSUtils->GetComputedProperty(aElement,
                                         nsEditProperty::cssBackgroundColor,
                                         bgColorStr);
    if (NS_FAILED(res)) return res;
    if (bgColorStr.EqualsLiteral("transparent")) {

      nsCOMPtr<nsIDOMViewCSS> viewCSS;
      res = mHTMLCSSUtils->GetDefaultViewCSS(aElement, getter_AddRefs(viewCSS));
      if (NS_FAILED(res)) return res;
      nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
      res = viewCSS->GetComputedStyle(aElement, EmptyString(), getter_AddRefs(cssDecl));
      if (NS_FAILED(res)) return res;
      // from these declarations, get the one we want and that one only
      nsCOMPtr<nsIDOMCSSValue> colorCssValue;
      res = cssDecl->GetPropertyCSSValue(NS_LITERAL_STRING("color"), getter_AddRefs(colorCssValue));
      if (NS_FAILED(res)) return res;

      PRUint16 type;
      res = colorCssValue->GetCssValueType(&type);
      if (NS_FAILED(res)) return res;
      if (nsIDOMCSSValue::CSS_PRIMITIVE_VALUE == type) {
        nsCOMPtr<nsIDOMCSSPrimitiveValue> val = do_QueryInterface(colorCssValue);
        res = val->GetPrimitiveType(&type);
        if (NS_FAILED(res)) return res;
        if (nsIDOMCSSPrimitiveValue::CSS_RGBCOLOR == type) {
          nsCOMPtr<nsIDOMRGBColor> rgbColor;
          res = val->GetRGBColorValue(getter_AddRefs(rgbColor));
          if (NS_FAILED(res)) return res;
          nsCOMPtr<nsIDOMCSSPrimitiveValue> red, green, blue;
          float r, g, b;
          res = rgbColor->GetRed(getter_AddRefs(red));
          if (NS_FAILED(res)) return res;
          res = rgbColor->GetGreen(getter_AddRefs(green));
          if (NS_FAILED(res)) return res;
          res = rgbColor->GetBlue(getter_AddRefs(blue));
          if (NS_FAILED(res)) return res;
          res = red->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, &r);
          if (NS_FAILED(res)) return res;
          res = green->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, &g);
          if (NS_FAILED(res)) return res;
          res = blue->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, &b);
          if (NS_FAILED(res)) return res;
          if (r >= BLACK_BG_RGB_TRIGGER &&
              g >= BLACK_BG_RGB_TRIGGER &&
              b >= BLACK_BG_RGB_TRIGGER)
            aReturn = NS_LITERAL_STRING("black");
          else
            aReturn = NS_LITERAL_STRING("white");
          return NS_OK;
        }
      }
    }
  }

  return NS_OK;
}
