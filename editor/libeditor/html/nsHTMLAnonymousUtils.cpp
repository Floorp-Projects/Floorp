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

#include "nsIDOMHTMLElement.h"
#include "nsIDOMNSHTMLElement.h"

#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMCSSStyleDeclaration.h"

#include "nsUnicharUtils.h"

// retrieve an integer stored into a CSS computed float value
static PRInt32 GetCSSFloatValue(nsIDOMCSSStyleDeclaration * aDecl,
                                const nsAString & aProperty)
{
  NS_ENSURE_ARG_POINTER(aDecl);

  nsCOMPtr<nsIDOMCSSValue> value;
  // get the computed CSSValue of the property
  nsresult res = aDecl->GetPropertyCSSValue(aProperty, getter_AddRefs(value));
  if (NS_FAILED(res) || !value) return 0;

  // check the type of the returned CSSValue; we handle here only
  // pixel and enum types
  nsCOMPtr<nsIDOMCSSPrimitiveValue> val = do_QueryInterface(value);
  PRUint16 type;
  val->GetPrimitiveType(&type);

  float f;
  switch (type) {
    case nsIDOMCSSPrimitiveValue::CSS_PX:
      // the value is in pixels, just get it
      res = val->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_PX, &f);
      if (NS_FAILED(res)) return 0;
      break;
    case nsIDOMCSSPrimitiveValue::CSS_IDENT: {
      // the value is keyword, we have to map these keywords into
      // numeric values
      nsAutoString str;
      res = val->GetStringValue(str);
      if (str.EqualsLiteral("thin"))
        f = 1;
      if (str.EqualsLiteral("medium"))
        f = 3;
      if (str.EqualsLiteral("thick"))
        f = 5;
      break;
    }
    default:
      f = 0;
  }

  return (PRInt32) f;
}

// Returns in *aReturn an anonymous nsDOMElement of type aTag,
// child of aParentNode. If aIsCreatedHidden is true, the class
// "hidden" is added to the created element. If aAnonClass is not
// the empty string, it becomes the value of the attribute "_moz_anonclass"
nsresult
nsHTMLEditor::CreateAnonymousElement(const nsAString & aTag, nsIDOMNode *  aParentNode,
                                     const nsAString & aAnonClass, PRBool aIsCreatedHidden,
                                     nsIDOMElement ** aReturn)
{
  NS_ENSURE_ARG_POINTER(aParentNode);
  NS_ENSURE_ARG_POINTER(aReturn);

  nsCOMPtr<nsIContent> parentContent( do_QueryInterface(aParentNode) );
  if (!parentContent)
    return NS_OK;

  // Get the document
  nsCOMPtr<nsIDOMDocument> domDoc;
  GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (!doc) return NS_ERROR_NULL_POINTER;

  // Get the pres shell
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  // Create a new node through the element factory
  nsCOMPtr<nsIContent> newContent;
  nsresult res = CreateHTMLContent(aTag, getter_AddRefs(newContent));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMElement> newElement = do_QueryInterface(newContent);
  if (!newElement)
    return NS_ERROR_FAILURE;

  // add the "hidden" class if needed
  if (aIsCreatedHidden) {
    res = newElement->SetAttribute(NS_LITERAL_STRING("class"),
                                   NS_LITERAL_STRING("hidden"));
    if (NS_FAILED(res)) return res;
  }

  // add an _moz_anonclass attribute if needed
  if (!aAnonClass.IsEmpty()) {
    res = newElement->SetAttribute(NS_LITERAL_STRING("_moz_anonclass"),
                                   aAnonClass);
    if (NS_FAILED(res)) return res;
  }

  // establish parenthood of the element
  newContent->SetNativeAnonymous(PR_TRUE);
  newContent->SetParent(parentContent);
  newContent->SetDocument(doc, PR_TRUE, PR_TRUE);
  newContent->SetBindingParent(newContent);
  // display the element
  ps->RecreateFramesFor(newContent);

  *aReturn = newElement;
  NS_IF_ADDREF(*aReturn);
  return NS_OK;
}

// Deletes all references to an anonymous element
void
nsHTMLEditor::DeleteRefToAnonymousNode(nsIDOMElement* aElement,
                                       nsIContent * aParentContent,
                                       nsIDocumentObserver * aDocObserver)
{
  // call ContentRemoved() for the anonymous content
  // node so its references get removed from the frame manager's
  // undisplay map, and its layout frames get destroyed!

  if (aElement) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    if (content) {
      aDocObserver->ContentRemoved(nsnull, aParentContent, content, -1);
      content->SetParent(nsnull);
      content->SetBindingParent(nsnull);
      content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
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
  if (!mIsObjectResizingEnabled &&
      !mIsAbsolutelyPositioningEnabled &&
      !mIsInlineTableEditingEnabled)
    return NS_OK;

  nsCOMPtr<nsIDOMElement> focusElement;
  // let's get the containing element of the selection
  nsresult res  = GetSelectionContainer(getter_AddRefs(focusElement));
  if (!focusElement) return NS_OK;
  if (NS_FAILED(res)) return res;

  // what's its tag?
  nsAutoString focusTagName;
  res = focusElement->GetTagName(focusTagName);
  if (NS_FAILED(res)) return res;
  ToLowerCase(focusTagName);
  nsCOMPtr<nsIAtom> focusTagAtom = do_GetAtom(focusTagName);

  nsCOMPtr<nsIDOMElement> absPosElement;
  if (mIsAbsolutelyPositioningEnabled) {
    // Absolute Positioning support is enabled, is the selection contained
    // in an absolutely positioned element ?
    res = GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(absPosElement));
    if (NS_FAILED(res)) return res;
  }

  nsCOMPtr<nsIDOMElement> cellElement;
  if (mIsObjectResizingEnabled || mIsInlineTableEditingEnabled) {
    // Resizing or Inline Table Editing is enabled, we need to check if the
    // selection is contained in a table cell
    res = GetElementOrParentByTagName(NS_LITERAL_STRING("td"),
                                      nsnull,
                                      getter_AddRefs(cellElement));
    if (NS_FAILED(res)) return res;
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

  // first let's cancel old settings if needed
  PRBool refreshResizing     = (mResizedObject != nsnull);
  PRBool refreshPositioning  = (mAbsolutelyPositionedObject != nsnull);
  PRBool refreshTableEditing = (mInlineEditedCell != nsnull);

  if (mIsAbsolutelyPositioningEnabled && mAbsolutelyPositionedObject &&
      absPosElement != mAbsolutelyPositionedObject) {
    res = HideGrabber();
    if (NS_FAILED(res)) return res;
    refreshPositioning = PR_FALSE;
  }

  if (mIsObjectResizingEnabled && mResizedObject &&
      mResizedObject != focusElement) {
    res = HideResizers();
    if (NS_FAILED(res)) return res;
    refreshResizing = PR_FALSE;
  }

  if (mIsInlineTableEditingEnabled && mInlineEditedCell &&
      mInlineEditedCell != cellElement) {
    res = HideInlineTableEditingUI();
    if (NS_FAILED(res)) return res;
    refreshTableEditing = PR_FALSE;
  }

  // now, let's display all contextual UI for good

  if (mIsObjectResizingEnabled && focusElement) {
    if (nsEditProperty::img == focusTagAtom)
      mResizedObjectIsAnImage = PR_TRUE;
    if (refreshResizing)
      res = RefreshResizers();
    else
      res = ShowResizers(focusElement);
    if (NS_FAILED(res)) return res;
  }

  if (mIsAbsolutelyPositioningEnabled && absPosElement) {
    if (refreshPositioning)
      res = RefreshGrabber();
    else
      res = ShowGrabberOnElement(absPosElement);
    if (NS_FAILED(res)) return res;
  }

  if (mIsInlineTableEditingEnabled && cellElement) {
    if (refreshTableEditing)
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
                                       PRInt32 & aX, PRInt32 & aY,
                                       PRInt32 & aW, PRInt32 & aH,
                                       PRInt32 & aBorderLeft,
                                       PRInt32 & aBorderTop,
                                       PRInt32 & aMarginLeft,
                                       PRInt32 & aMarginTop)
{
  NS_ENSURE_ARG_POINTER(aElement);

  // Is the element positioned ? let's check the cheap way first...
  PRBool isPositioned = PR_FALSE;
  nsresult res = aElement->HasAttribute(NS_LITERAL_STRING("_moz_abspos"), &isPositioned);
  if (NS_FAILED(res)) return res;
  if (!isPositioned) {
    // hmmm... the expensive way now...
    nsAutoString positionStr;
    mHTMLCSSUtils->GetComputedProperty(aElement, nsEditProperty::cssPosition,
                                       positionStr);
    isPositioned = positionStr.EqualsLiteral("absolute");
  }

  if (isPositioned) {
    // Yes, it is absolutely positioned
    mResizedObjectIsAbsolutelyPositioned = PR_TRUE;

    nsCOMPtr<nsIDOMViewCSS> viewCSS;
    res = mHTMLCSSUtils->GetDefaultViewCSS(aElement, getter_AddRefs(viewCSS));
    if (NS_FAILED(res)) return res;

    nsAutoString empty;
    nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
    // Get the all the computed css styles attached to the element node
    res = viewCSS->GetComputedStyle(aElement, empty, getter_AddRefs(cssDecl));
    if (NS_FAILED(res)) return res;

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
    mResizedObjectIsAbsolutelyPositioned = PR_FALSE;
    nsCOMPtr<nsIDOMNSHTMLElement> nsElement = do_QueryInterface(aElement);
    if (!nsElement) {return NS_ERROR_NULL_POINTER; }

    GetElementOrigin(aElement, aX, aY);

    res = nsElement->GetOffsetWidth(&aW);
    if (NS_FAILED(res)) return res;
    res = nsElement->GetOffsetHeight(&aH);

    aBorderLeft = 0;
    aBorderTop  = 0;
    aMarginLeft = 0;
    aMarginTop = 0;
  }
  return res;
}

// self-explanatory
void
nsHTMLEditor::SetAnonymousElementPosition(PRInt32 aX, PRInt32 aY, nsIDOMElement *aElement)
{
  mHTMLCSSUtils->SetCSSPropertyPixels(aElement, NS_LITERAL_STRING("left"), aX);
  mHTMLCSSUtils->SetCSSPropertyPixels(aElement, NS_LITERAL_STRING("top"), aY);
}
