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
#include "nsPresContext.h"

#include "nsISelection.h"

#include "nsTextEditUtils.h"
#include "nsEditorUtils.h"
#include "nsHTMLEditUtils.h"
#include "nsTextEditRules.h"

#include "nsIDOMHTMLElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMEventTarget.h"

#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIMutationObserver.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"

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

  return (PRInt32) f;
}

class nsElementDeletionObserver : public nsIMutationObserver
{
public:
  nsElementDeletionObserver(nsINode* aNativeAnonNode, nsINode* aObservedNode)
  : mNativeAnonNode(aNativeAnonNode), mObservedNode(aObservedNode) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER
protected:
  nsINode* mNativeAnonNode;
  nsINode* mObservedNode;
};

NS_IMPL_ISUPPORTS1(nsElementDeletionObserver, nsIMutationObserver)
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
  *aReturn = nsnull;

  nsCOMPtr<nsIContent> parentContent( do_QueryInterface(aParentNode) );
  NS_ENSURE_TRUE(parentContent, NS_OK);

  // Get the document
  nsCOMPtr<nsIDOMDocument> domDoc;
  GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_NULL_POINTER);

  // Get the pres shell
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  // Create a new node through the element factory
  nsCOMPtr<nsIContent> newContent;
  nsresult res = CreateHTMLContent(aTag, getter_AddRefs(newContent));
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMElement> newElement = do_QueryInterface(newContent);
  NS_ENSURE_TRUE(newElement, NS_ERROR_FAILURE);

  // add the "hidden" class if needed
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
    newContent->SetNativeAnonymous();
    res = newContent->BindToTree(doc, parentContent, parentContent, PR_TRUE);
    if (NS_FAILED(res)) {
      newContent->UnbindFromTree();
      return res;
    }
  }

  nsElementDeletionObserver* observer =
    new nsElementDeletionObserver(newContent, parentContent);
  if (!observer) {
    newContent->UnbindFromTree();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(observer); // NodeWillBeDestroyed releases.
  parentContent->AddMutationObserver(observer);
  newContent->AddMutationObserver(observer);

  // display the element
  ps->RecreateFramesFor(newContent);

  *aReturn = newElement;
  NS_IF_ADDREF(*aReturn);
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
      // In that case presContext->GetPresShell() returns nsnull.
      // See bug 338129.
      if (aShell && aShell->GetPresContext() &&
          aShell->GetPresContext()->GetPresShell() == aShell) {
        nsCOMPtr<nsIDocumentObserver> docObserver = do_QueryInterface(aShell);
        if (docObserver) {
          // Call BeginUpdate() so that the nsCSSFrameConstructor/PresShell
          // knows we're messing with the frame tree.
          nsCOMPtr<nsIDOMDocument> domDocument;
          GetDocument(getter_AddRefs(domDocument));
          nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
          if (document)
            docObserver->BeginUpdate(document, UPDATE_CONTENT_MODEL);

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
                                      nsnull,
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
      mResizedObjectIsAnImage = PR_TRUE;
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
                                       PRInt32 & aX, PRInt32 & aY,
                                       PRInt32 & aW, PRInt32 & aH,
                                       PRInt32 & aBorderLeft,
                                       PRInt32 & aBorderTop,
                                       PRInt32 & aMarginLeft,
                                       PRInt32 & aMarginTop)
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
    mResizedObjectIsAbsolutelyPositioned = PR_TRUE;

    nsCOMPtr<nsIDOMWindow> window;
    res = mHTMLCSSUtils->GetDefaultViewCSS(aElement, getter_AddRefs(window));
    NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
    // Get the all the computed css styles attached to the element node
    res = window->GetComputedStyle(aElement, EmptyString(), getter_AddRefs(cssDecl));
    NS_ENSURE_SUCCESS(res, res);

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
    NS_ENSURE_SUCCESS(res, res);
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
