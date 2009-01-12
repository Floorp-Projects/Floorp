/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsTextAttrs.h"

#include "nsAccessNode.h"
#include "nsHyperTextAccessibleWrap.h"

////////////////////////////////////////////////////////////////////////////////
// Constants and structures

/**
 * Item of the gCSSTextAttrsMap map.
 */
struct nsCSSTextAttrMapItem
{
  const char* mCSSName;
  const char* mCSSValue;
  nsIAtom** mAttrName;
  const char* mAttrValue;
};

/**
 * The map of CSS properties to text attributes.
 */
const char* const kAnyValue = nsnull;
const char* const kCopyValue = nsnull;

static nsCSSTextAttrMapItem gCSSTextAttrsMap[] =
{
  // CSS name            CSS value        Attribute name                                Attribute value
  { "color",             kAnyValue,       &nsAccessibilityAtoms::color,                 kCopyValue },
  { "font-family",       kAnyValue,       &nsAccessibilityAtoms::fontFamily,            kCopyValue },
  { "font-style",        kAnyValue,       &nsAccessibilityAtoms::fontStyle,             kCopyValue },
  { "font-weight",       kAnyValue,       &nsAccessibilityAtoms::fontWeight,            kCopyValue },
  { "text-decoration",   "line-through",  &nsAccessibilityAtoms::textLineThroughStyle,  "solid" },
  { "text-decoration",   "underline",     &nsAccessibilityAtoms::textUnderlineStyle,    "solid" },
  { "vertical-align",    kAnyValue,       &nsAccessibilityAtoms::textPosition,          kCopyValue }
};

////////////////////////////////////////////////////////////////////////////////
// nsTextAttrs

nsTextAttrsMgr::nsTextAttrsMgr(nsHyperTextAccessible *aHyperTextAcc,
                               nsIDOMNode *aHyperTextNode,
                               PRBool aIncludeDefAttrs,
                               nsIDOMNode *aOffsetNode) :
  mHyperTextAcc(aHyperTextAcc), mHyperTextNode(aHyperTextNode),
  mIncludeDefAttrs(aIncludeDefAttrs), mOffsetNode(aOffsetNode)
{
}

nsresult
nsTextAttrsMgr::GetAttributes(nsIPersistentProperties *aAttributes,
                              PRInt32 *aStartHTOffset,
                              PRInt32 *aEndHTOffset)
{
  // 1. Hyper text accessible and its DOM node must be specified always.
  // 2. Offset DOM node and result hyper text offsets must be specifed in
  // the case of text attributes.
  // 3. Offset DOM node and result hyper text offsets must not be specifed but
  // include default text attributes flag and attributes list must be specified
  // in the case of default text attributes.
  NS_PRECONDITION(mHyperTextAcc && mHyperTextNode &&
                  ((mOffsetNode && aStartHTOffset && aEndHTOffset) ||
                  (!mOffsetNode && !aStartHTOffset && !aEndHTOffset &&
                   mIncludeDefAttrs && aAttributes)),
                  "Wrong usage of nsTextAttrsMgr!");

  nsCOMPtr<nsIDOMElement> hyperTextElm =
    nsCoreUtils::GetDOMElementFor(mHyperTextNode);
  nsCOMPtr<nsIDOMElement> offsetElm;
  if (mOffsetNode)
    offsetElm = nsCoreUtils::GetDOMElementFor(mOffsetNode);

  nsIFrame *rootFrame = nsCoreUtils::GetFrameFor(hyperTextElm);
  nsIFrame *frame = nsnull;
  if (offsetElm)
    frame = nsCoreUtils::GetFrameFor(offsetElm);

  nsTPtrArray<nsITextAttr> textAttrArray(10);

  // "language" text attribute
  nsLangTextAttr langTextAttr(mHyperTextAcc, mHyperTextNode, mOffsetNode);
  textAttrArray.AppendElement(&langTextAttr);

  // "color" text attribute
  nsCSSTextAttr colorTextAttr(0, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(&colorTextAttr);

  // "font-family" text attribute
  nsCSSTextAttr fontFamilyTextAttr(1, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(&fontFamilyTextAttr);

  // "font-style" text attribute
  nsCSSTextAttr fontStyleTextAttr(2, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(&fontStyleTextAttr);

  // "font-weight" text attribute
  nsCSSTextAttr fontWeightTextAttr(3, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(&fontWeightTextAttr);

  // "text-line-through-style" text attribute
  nsCSSTextAttr lineThroughTextAttr(4, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(&lineThroughTextAttr);

  // "text-underline-style" text attribute
  nsCSSTextAttr underlineTextAttr(5, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(&underlineTextAttr);

  // "text-position" text attribute
  nsCSSTextAttr posTextAttr(6, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(&posTextAttr);

  // "background-color" text attribute
  nsBGColorTextAttr bgColorTextAttr(rootFrame, frame);
  textAttrArray.AppendElement(&bgColorTextAttr);

  // "font-size" text attribute
  nsFontSizeTextAttr fontSizeTextAttr(rootFrame, frame);
  textAttrArray.AppendElement(&fontSizeTextAttr);

  // Expose text attributes if applicable.
  if (aAttributes) {
    PRUint32 len = textAttrArray.Length();
    for (PRUint32 idx = 0; idx < len; idx++) {
      nsITextAttr *textAttr = textAttrArray[idx];

      nsAutoString value;
      if (textAttr->GetValue(value, mIncludeDefAttrs))
        nsAccUtils::SetAccAttr(aAttributes, textAttr->GetName(), value);
    }
  }

  nsresult rv = NS_OK;

  // Expose text attributes range where they are applied if applicable.
  if (mOffsetNode)
    rv = GetRange(textAttrArray, aStartHTOffset, aEndHTOffset);

  textAttrArray.Clear();
  return rv;
}

nsresult
nsTextAttrsMgr::GetRange(const nsTPtrArray<nsITextAttr>& aTextAttrArray,
                         PRInt32 *aStartHTOffset, PRInt32 *aEndHTOffset)
{
  nsCOMPtr<nsIDOMElement> rootElm =
    nsCoreUtils::GetDOMElementFor(mHyperTextNode);
  NS_ENSURE_STATE(rootElm);

  nsCOMPtr<nsIDOMNode> tmpNode(mOffsetNode);
  nsCOMPtr<nsIDOMNode> currNode(mOffsetNode);

  PRUint32 len = aTextAttrArray.Length();

  // Navigate backwards and forwards from current node to the root node to
  // calculate range bounds for the text attribute. Navigation sequence is the
  // following:
  // 1. Navigate through the siblings.
  // 2. If the traversed sibling has children then navigate from its leaf child
  //    to it through whole tree of the traversed sibling.
  // 3. Get the parent and cycle algorithm until the root node.

  // Navigate backwards (find the start offset).
  while (currNode && currNode != rootElm) {
    nsCOMPtr<nsIDOMElement> currElm(nsCoreUtils::GetDOMElementFor(currNode));
    NS_ENSURE_STATE(currElm);

    if (currNode != mOffsetNode) {
      PRBool stop = PR_FALSE;
      for (PRUint32 idx = 0; idx < len; idx++) {
        nsITextAttr *textAttr = aTextAttrArray[idx];
        if (!textAttr->Equal(currElm)) {

          PRInt32 startHTOffset = 0;
          nsCOMPtr<nsIAccessible> startAcc;
          nsresult rv = mHyperTextAcc->
            DOMPointToHypertextOffset(tmpNode, -1, &startHTOffset,
                                      getter_AddRefs(startAcc));
          NS_ENSURE_SUCCESS(rv, rv);

          if (!startAcc)
            startHTOffset = 0;

          if (startHTOffset > *aStartHTOffset)
            *aStartHTOffset = startHTOffset;

          stop = PR_TRUE;
          break;
        }
      }
      if (stop)
        break;
    }

    currNode->GetPreviousSibling(getter_AddRefs(tmpNode));
    if (tmpNode) {
      // Navigate through the subtree of traversed children to calculate
      // left bound of the range.
      FindStartOffsetInSubtree(aTextAttrArray, tmpNode, currNode,
                               aStartHTOffset);
    }

    currNode->GetParentNode(getter_AddRefs(tmpNode));
    currNode.swap(tmpNode);
  }

  // Navigate forwards (find the end offset).
  PRBool moveIntoSubtree = PR_TRUE;
  currNode = mOffsetNode;

  while (currNode && currNode != rootElm) {
    nsCOMPtr<nsIDOMElement> currElm(nsCoreUtils::GetDOMElementFor(currNode));
    NS_ENSURE_STATE(currElm);

    // Stop new end offset searching if the given text attribute changes its
    // value.
    PRBool stop = PR_FALSE;
    for (PRUint32 idx = 0; idx < len; idx++) {
      nsITextAttr *textAttr = aTextAttrArray[idx];
      if (!textAttr->Equal(currElm)) {

        PRInt32 endHTOffset = 0;
        nsresult rv = mHyperTextAcc->
          DOMPointToHypertextOffset(currNode, -1, &endHTOffset);
        NS_ENSURE_SUCCESS(rv, rv);

        if (endHTOffset < *aEndHTOffset)
          *aEndHTOffset = endHTOffset;

        stop = PR_TRUE;
        break;
      }
    }

    if (stop)
      break;

    if (moveIntoSubtree) {
      // Navigate through subtree of traversed node. We use 'moveIntoSubtree'
      // flag to avoid traversing the same subtree twice.
      currNode->GetFirstChild(getter_AddRefs(tmpNode));
      if (tmpNode)
        FindEndOffsetInSubtree(aTextAttrArray, tmpNode, aEndHTOffset);
    }

    currNode->GetNextSibling(getter_AddRefs(tmpNode));
    moveIntoSubtree = PR_TRUE;
    if (!tmpNode) {
      currNode->GetParentNode(getter_AddRefs(tmpNode));
      moveIntoSubtree = PR_FALSE;
    }

    currNode.swap(tmpNode);
  }

  return NS_OK;
}

PRBool
nsTextAttrsMgr::FindEndOffsetInSubtree(const nsTPtrArray<nsITextAttr>& aTextAttrArray,
                                       nsIDOMNode *aCurrNode,
                                       PRInt32 *aHTOffset)
{
  if (!aCurrNode)
    return PR_FALSE;

  nsCOMPtr<nsIDOMElement> currElm(nsCoreUtils::GetDOMElementFor(aCurrNode));
  NS_ENSURE_STATE(currElm);

  // If the given text attribute (pointed by nsTextAttr object) changes its
  // value on the traversed element then fit the end of range.
  PRUint32 len = aTextAttrArray.Length();
  for (PRUint32 idx = 0; idx < len; idx++) {
    nsITextAttr *textAttr = aTextAttrArray[idx];
    if (!textAttr->Equal(currElm)) {
      PRInt32 endHTOffset = 0;
      nsresult rv = mHyperTextAcc->
        DOMPointToHypertextOffset(aCurrNode, -1, &endHTOffset);
      NS_ENSURE_SUCCESS(rv, PR_FALSE);

      if (endHTOffset < *aHTOffset)
        *aHTOffset = endHTOffset;

      return PR_TRUE;
    }
  }

  // Deeply traverse into the tree to fit the end of range.
  nsCOMPtr<nsIDOMNode> nextNode;
  aCurrNode->GetFirstChild(getter_AddRefs(nextNode));
  if (nextNode) {
    PRBool res = FindEndOffsetInSubtree(aTextAttrArray, nextNode, aHTOffset);
    if (res)
      return res;
  }

  aCurrNode->GetNextSibling(getter_AddRefs(nextNode));
  if (nextNode) {
    if (FindEndOffsetInSubtree(aTextAttrArray, nextNode, aHTOffset))
      return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsTextAttrsMgr::FindStartOffsetInSubtree(const nsTPtrArray<nsITextAttr>& aTextAttrArray,
                                         nsIDOMNode *aCurrNode,
                                         nsIDOMNode *aPrevNode,
                                         PRInt32 *aHTOffset)
{
  if (!aCurrNode)
    return PR_FALSE;

  // Find the closest element back to the traversed element.
  nsCOMPtr<nsIDOMNode> nextNode;
  aCurrNode->GetLastChild(getter_AddRefs(nextNode));
  if (nextNode) {
    if (FindStartOffsetInSubtree(aTextAttrArray, nextNode, aPrevNode, aHTOffset))
      return PR_TRUE;
  }

  nsCOMPtr<nsIDOMElement> currElm(nsCoreUtils::GetDOMElementFor(aCurrNode));
  NS_ENSURE_STATE(currElm);

  // If the given text attribute (pointed by nsTextAttr object) changes its
  // value on the traversed element then fit the start of range.
  PRUint32 len = aTextAttrArray.Length();
  for (PRUint32 idx = 0; idx < len; idx++) {
    nsITextAttr *textAttr = aTextAttrArray[idx];
    if (!textAttr->Equal(currElm)) {

      PRInt32 startHTOffset = 0;
      nsCOMPtr<nsIAccessible> startAcc;
      nsresult rv = mHyperTextAcc->
        DOMPointToHypertextOffset(aPrevNode, -1, &startHTOffset,
                                  getter_AddRefs(startAcc));
      NS_ENSURE_SUCCESS(rv, PR_FALSE);

      if (!startAcc)
        startHTOffset = 0;

      if (startHTOffset > *aHTOffset)
        *aHTOffset = startHTOffset;

      return PR_TRUE;
    }
  }

  // Moving backwards to find the start of range.
  aCurrNode->GetPreviousSibling(getter_AddRefs(nextNode));
  if (nextNode) {
    if (FindStartOffsetInSubtree(aTextAttrArray, nextNode, aCurrNode, aHTOffset))
      return PR_TRUE;
  }

  return PR_FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// nsLangTextAttr

nsLangTextAttr::nsLangTextAttr(nsHyperTextAccessible *aRootAcc, 
                               nsIDOMNode *aRootNode, nsIDOMNode *aNode) :
  nsTextAttr(aNode == nsnull)
{
  mRootContent = do_QueryInterface(aRootNode);

  nsresult rv = aRootAcc->GetLanguage(mRootNativeValue);
  mIsRootDefined = NS_SUCCEEDED(rv) && !mRootNativeValue.IsEmpty();

  if (aNode) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
    mIsDefined = GetLang(content, mNativeValue);
  }
}

PRBool
nsLangTextAttr::GetValueFor(nsIDOMElement *aElm, nsAutoString *aValue)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElm);
  return GetLang(content, *aValue);
}

void
nsLangTextAttr::Format(const nsAutoString& aValue, nsAString& aFormattedValue)
{
  aFormattedValue = aValue;
}

PRBool
nsLangTextAttr::GetLang(nsIContent *aContent, nsAString& aLang)
{
  nsCoreUtils::GetLanguageFor(aContent, mRootContent, aLang);
  return !aLang.IsEmpty();
}

////////////////////////////////////////////////////////////////////////////////
// nsCSSTextAttr

nsCSSTextAttr::nsCSSTextAttr(PRUint32 aIndex, nsIDOMElement *aRootElm,
                             nsIDOMElement *aElm) :
  nsTextAttr(aElm == nsnull), mIndex(aIndex)
{
  mIsRootDefined = GetValueFor(aRootElm, &mRootNativeValue);

  if (aElm)
    mIsDefined = GetValueFor(aElm, &mNativeValue);
}

nsIAtom*
nsCSSTextAttr::GetName()
{
  return *gCSSTextAttrsMap[mIndex].mAttrName;
}

PRBool
nsCSSTextAttr::GetValueFor(nsIDOMElement *aElm, nsAutoString *aValue)
{
  nsCOMPtr<nsIDOMCSSStyleDeclaration> currStyleDecl;
  nsCoreUtils::GetComputedStyleDeclaration(EmptyString(), aElm,
                                           getter_AddRefs(currStyleDecl));
  if (!currStyleDecl)
    return PR_FALSE;

  NS_ConvertASCIItoUTF16 cssName(gCSSTextAttrsMap[mIndex].mCSSName);

  nsresult rv = currStyleDecl->GetPropertyValue(cssName, *aValue);
  if (NS_FAILED(rv))
    return PR_TRUE;

  const char *cssValue = gCSSTextAttrsMap[mIndex].mCSSValue;
  if (cssValue != kAnyValue && !aValue->EqualsASCII(cssValue))
    return PR_FALSE;

  return PR_TRUE;
}

void
nsCSSTextAttr::Format(const nsAutoString& aValue, nsAString& aFormattedValue)
{
  const char *attrValue = gCSSTextAttrsMap[mIndex].mAttrValue;
  if (attrValue != kCopyValue)
    AppendASCIItoUTF16(attrValue, aFormattedValue);
  else
    aFormattedValue = aValue;
}

////////////////////////////////////////////////////////////////////////////////
// nsBackgroundTextAttr

nsBGColorTextAttr::nsBGColorTextAttr(nsIFrame *aRootFrame, nsIFrame *aFrame) :
  nsTextAttr(aFrame == nsnull), mRootFrame(aRootFrame)
{
  mIsRootDefined = GetColor(mRootFrame, &mRootNativeValue);
  if (aFrame)
    mIsDefined = GetColor(aFrame, &mNativeValue);
}

PRBool
nsBGColorTextAttr::GetValueFor(nsIDOMElement *aElm, nscolor *aValue)
{
  nsIFrame *frame = nsCoreUtils::GetFrameFor(aElm);
  if (!frame)
    return PR_FALSE;

  return GetColor(frame, aValue);
}

void
nsBGColorTextAttr::Format(const nscolor& aValue, nsAString& aFormattedValue)
{
  // Combine the string like rgb(R, G, B) from nscolor.
  nsAutoString value;
  value.AppendLiteral("rgb(");
  value.AppendInt(NS_GET_R(aValue));
  value.AppendLiteral(", ");
  value.AppendInt(NS_GET_G(aValue));
  value.AppendLiteral(", ");
  value.AppendInt(NS_GET_B(aValue));
  value.Append(')');

  aFormattedValue = value;
}

PRBool
nsBGColorTextAttr::GetColor(nsIFrame *aFrame, nscolor *aColor)
{
  const nsStyleBackground *styleBackground = aFrame->GetStyleBackground();

  if (!styleBackground->IsTransparent()) {
    *aColor = styleBackground->mBackgroundColor;
    return PR_TRUE;
  }

  nsIFrame *parentFrame = aFrame->GetParent();
  if (!parentFrame) {
    *aColor = aFrame->PresContext()->DefaultBackgroundColor();
    return PR_TRUE;
  }

  // Each frame of parents chain for the initially passed 'aFrame' has
  // transparent background color. So background color isn't changed from
  // 'mRootFrame' to initially passed 'aFrame'.
  if (parentFrame == mRootFrame)
    return PR_FALSE;

  return GetColor(parentFrame, aColor);
}

////////////////////////////////////////////////////////////////////////////////
// nsFontSizeTextAttr

nsFontSizeTextAttr::nsFontSizeTextAttr(nsIFrame *aRootFrame, nsIFrame *aFrame) :
  nsTextAttr(aFrame == nsnull)
{
  mDC = aRootFrame->PresContext()->DeviceContext();

  mRootNativeValue = GetFontSize(aRootFrame);
  mIsRootDefined = PR_TRUE;

  if (aFrame) {
    mNativeValue = GetFontSize(aFrame);
    mIsDefined = PR_TRUE;
  }
}

PRBool
nsFontSizeTextAttr::GetValueFor(nsIDOMElement *aElm, nscoord *aValue)
{
  nsIFrame *frame = nsCoreUtils::GetFrameFor(aElm);
  if (!frame)
    return PR_FALSE;
  
  *aValue = GetFontSize(frame);
  return PR_TRUE;
}

void
nsFontSizeTextAttr::Format(const nscoord& aValue, nsAString& aFormattedValue)
{
  // Convert from nscoord to pt.
  //
  // Note: according to IA2, "The conversion doesn't have to be exact.
  // The intent is to give the user a feel for the size of the text."
  // 
  // ATK does not specify a unit and will likely follow IA2 here.
  //
  // XXX todo: consider sharing this code with layout module? (bug 474621)
  float inches = static_cast<float>(aValue) /
    static_cast<float>(mDC->AppUnitsPerInch());
  int pts = static_cast<int>(inches * 72 + .5); // 72 pts per inch

  nsAutoString value;
  value.AppendInt(pts);
  value.Append(NS_LITERAL_STRING("pt"));
  aFormattedValue = value;
}

nscoord
nsFontSizeTextAttr::GetFontSize(nsIFrame *aFrame)
{
  nsStyleFont* styleFont =
    (nsStyleFont*)(aFrame->GetStyleDataExternal(eStyleStruct_Font));

  return styleFont->mSize;
}
