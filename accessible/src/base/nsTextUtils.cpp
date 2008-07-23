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

#include "nsTextUtils.h"

#include "nsAccessNode.h"

////////////////////////////////////////////////////////////////////////////////
// nsLangTextAttr

PRBool
nsLangTextAttr::equal(nsIDOMElement *aElm)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElm));
  if (!content)
    return PR_FALSE;

  nsAutoString lang;
  nsAccUtils::GetLanguageFor(content, mRootContent, lang);

  return lang == mLang;
}

////////////////////////////////////////////////////////////////////////////////
// nsCSSTextAttr

/**
 * Item of the gCSSTextAttrsMap map.
 */
struct nsCSSTextAttrMapItem
{
  const char* mCSSName;
  const char* mCSSValue;
  const char* mAttrName;
  const char* mAttrValue;
};

/**
 * The map of CSS properties to text attributes.
 */

const char* const kAnyValue = nsnull;
const char* const kCopyName = nsnull;
const char* const kCopyValue = nsnull;

static nsCSSTextAttrMapItem gCSSTextAttrsMap[] = {
  // CSS name            CSS value        Attribute name              Attribute name
  { "background-color",  kAnyValue,       kCopyName,                  kCopyValue },
  { "color",             kAnyValue,       kCopyName,                  kCopyValue },
  { "font-family",       kAnyValue,       kCopyName,                  kCopyValue },
  { "font-size",         kAnyValue,       kCopyName,                  kCopyValue },
  { "font-style",        kAnyValue,       kCopyName,                  kCopyValue },
  { "font-weight",       kAnyValue,       kCopyName,                  kCopyValue },
  { "text-decoration",   "line-through",  "text-line-through-style",  "solid" },
  { "text-decoration",   "underline",     "text-underline-style",     "solid" },
  { "text-align",        kAnyValue,       kCopyName,                  kCopyValue },
  { "text-indent",       kAnyValue,       kCopyName,                  kCopyValue }
};

nsCSSTextAttr::nsCSSTextAttr(PRBool aIncludeDefAttrValue, nsIDOMElement *aElm,
                             nsIDOMElement *aRootElm) :
  mIndex(-1), mIncludeDefAttrValue(aIncludeDefAttrValue)
{
  nsAccessNode::GetComputedStyleDeclaration(EmptyString(), aElm,
                                            getter_AddRefs(mStyleDecl));

  if (!mIncludeDefAttrValue)
    nsAccessNode::GetComputedStyleDeclaration(EmptyString(), aRootElm,
                                              getter_AddRefs(mDefStyleDecl));
}

PRBool
nsCSSTextAttr::equal(nsIDOMElement *aElm)
{
  if (!aElm || !mStyleDecl)
    return PR_FALSE;

  nsCOMPtr<nsIDOMCSSStyleDeclaration> currStyleDecl;
  nsAccessNode::GetComputedStyleDeclaration(EmptyString(), aElm,
                                            getter_AddRefs(currStyleDecl));
  if (!currStyleDecl)
    return PR_FALSE;

  NS_ConvertASCIItoUTF16 cssName(gCSSTextAttrsMap[mIndex].mCSSName);

  nsAutoString currValue;
  nsresult rv = currStyleDecl->GetPropertyValue(cssName, currValue);
  if (NS_FAILED(rv))
    return PR_FALSE;

  nsAutoString value;
  rv = mStyleDecl->GetPropertyValue(cssName, value);
  return NS_SUCCEEDED(rv) && value == currValue;
}

PRBool
nsCSSTextAttr::iterate()
{
  return ++mIndex < static_cast<PRInt32>(NS_ARRAY_LENGTH(gCSSTextAttrsMap));
}

PRBool
nsCSSTextAttr::get(nsACString& aName, nsAString& aValue)
{
  if (!mStyleDecl)
    return PR_FALSE;

  NS_ConvertASCIItoUTF16 cssName(gCSSTextAttrsMap[mIndex].mCSSName);
  nsresult rv = mStyleDecl->GetPropertyValue(cssName, aValue);
  if (NS_FAILED(rv))
    return PR_FALSE;

  // Don't expose text attribute if corresponding CSS value on the element
  // equals to CSS value on the root element and we don't want to include
  // default values.
  if (!mIncludeDefAttrValue) {
    if (!mDefStyleDecl)
      return PR_FALSE;

    nsAutoString defValue;
    mDefStyleDecl->GetPropertyValue(cssName, defValue);
    if (defValue == aValue)
      return PR_FALSE;
  }

  // Don't expose text attribute if its required specific CSS value isn't
  // matched with the CSS value we got.
  const char *cssValue = gCSSTextAttrsMap[mIndex].mCSSValue;
  if (cssValue != kAnyValue && !aValue.EqualsASCII(cssValue))
    return PR_FALSE;

  // Get the name of text attribute.
  if (gCSSTextAttrsMap[mIndex].mAttrName != kCopyName)
    aName = gCSSTextAttrsMap[mIndex].mAttrName;
  else
    aName = gCSSTextAttrsMap[mIndex].mCSSName;

  // Get the value of text attribute.
  const char *attrValue = gCSSTextAttrsMap[mIndex].mAttrValue;
  if (attrValue != kCopyValue)
    AppendASCIItoUTF16(attrValue, aValue);

  return PR_TRUE;
}

