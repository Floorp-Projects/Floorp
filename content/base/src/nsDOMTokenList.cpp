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
 * Sylvain Pasche <sylvain.pasche@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * Implementation of nsIDOMDOMTokenList specified by HTML5.
 */

#include "nsDOMTokenList.h"

#include "nsAttrValue.h"
#include "nsContentUtils.h"
#include "nsDOMError.h"
#include "nsGenericElement.h"
#include "dombindings.h"


nsDOMTokenList::nsDOMTokenList(nsGenericElement *aElement, nsIAtom* aAttrAtom)
  : mElement(aElement),
    mAttrAtom(aAttrAtom)
{
  // We don't add a reference to our element. If it goes away,
  // we'll be told to drop our reference
  SetIsDOMBinding();
}

nsDOMTokenList::~nsDOMTokenList() { }

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMTokenList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMTokenList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMTokenList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMTokenList)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

DOMCI_DATA(DOMTokenList, nsDOMTokenList)

NS_INTERFACE_TABLE_HEAD(nsDOMTokenList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE1(nsDOMTokenList,
                      nsIDOMDOMTokenList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsDOMTokenList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMTokenList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMTokenList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMTokenList)

void
nsDOMTokenList::DropReference()
{
  mElement = nsnull;
}

NS_IMETHODIMP
nsDOMTokenList::GetLength(PRUint32 *aLength)
{
  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    *aLength = 0;
    return NS_OK;
  }

  *aLength = attr->GetAtomCount();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMTokenList::Item(PRUint32 aIndex, nsAString& aResult)
{
  const nsAttrValue* attr = GetParsedAttr();

  if (!attr || aIndex >= static_cast<PRUint32>(attr->GetAtomCount())) {
    SetDOMStringToNull(aResult);
    return NS_OK;
  }
  attr->AtomAt(aIndex)->ToString(aResult);

  return NS_OK;
}

nsresult
nsDOMTokenList::CheckToken(const nsAString& aStr)
{
  if (aStr.IsEmpty()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  nsAString::const_iterator iter, end;
  aStr.BeginReading(iter);
  aStr.EndReading(end);

  while (iter != end) {
    if (nsContentUtils::IsHTMLWhitespace(*iter))
      return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
    ++iter;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMTokenList::Contains(const nsAString& aToken, bool* aResult)
{
  nsresult rv = CheckToken(aToken);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    *aResult = false;
    return NS_OK;
  }

  *aResult = attr->Contains(aToken);

  return NS_OK;
}

void
nsDOMTokenList::AddInternal(const nsAttrValue* aAttr,
                            const nsAString& aToken)
{
  if (!mElement) {
    return;
  }

  nsAutoString resultStr;

  if (aAttr) {
    aAttr->ToString(resultStr);
  }

  if (!resultStr.IsEmpty() &&
      !nsContentUtils::IsHTMLWhitespace(
          resultStr.CharAt(resultStr.Length() - 1))) {
    resultStr.Append(NS_LITERAL_STRING(" ") + aToken);
  } else {
    resultStr.Append(aToken);
  }
  mElement->SetAttr(kNameSpaceID_None, mAttrAtom, resultStr, true);
}

NS_IMETHODIMP
nsDOMTokenList::Add(const nsAString& aToken)
{
  nsresult rv = CheckToken(aToken);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsAttrValue* attr = GetParsedAttr();

  if (attr && attr->Contains(aToken)) {
    return NS_OK;
  }

  AddInternal(attr, aToken);

  return NS_OK;
}

void
nsDOMTokenList::RemoveInternal(const nsAttrValue* aAttr,
                               const nsAString& aToken)
{
  NS_ABORT_IF_FALSE(aAttr, "Need an attribute");

  nsAutoString input;
  aAttr->ToString(input);

  nsAString::const_iterator copyStart, tokenStart, iter, end;
  input.BeginReading(iter);
  input.EndReading(end);
  copyStart = iter;

  nsAutoString output;
  bool lastTokenRemoved = false;

  while (iter != end) {
    // skip whitespace.
    while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
      ++iter;
    }

    if (iter == end) {
      // At this point we're sure the last seen token (if any) wasn't to be
      // removed. So the trailing spaces will need to be kept.
      NS_ABORT_IF_FALSE(!lastTokenRemoved, "How did this happen?");

      output.Append(Substring(copyStart, end));
      break;
    }

    tokenStart = iter;
    do {
      ++iter;
    } while (iter != end && !nsContentUtils::IsHTMLWhitespace(*iter));

    if (Substring(tokenStart, iter).Equals(aToken)) {

      // Skip whitespace after the token, it will be collapsed.
      while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
        ++iter;
      }
      copyStart = iter;
      lastTokenRemoved = true;

    } else {

      if (lastTokenRemoved && !output.IsEmpty()) {
        NS_ABORT_IF_FALSE(!nsContentUtils::IsHTMLWhitespace(
          output.CharAt(output.Length() - 1)), "Invalid last output token");
        output.Append(PRUnichar(' '));
      }
      lastTokenRemoved = false;
      output.Append(Substring(copyStart, iter));
      copyStart = iter;
    }
  }

  mElement->SetAttr(kNameSpaceID_None, mAttrAtom, output, true);
}

NS_IMETHODIMP
nsDOMTokenList::Remove(const nsAString& aToken)
{
  nsresult rv = CheckToken(aToken);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsAttrValue* attr = GetParsedAttr();
  if (!attr) {
    return NS_OK;
  }

  if (!attr->Contains(aToken)) {
    return NS_OK;
  }

  RemoveInternal(attr, aToken);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMTokenList::Toggle(const nsAString& aToken, bool* aResult)
{
  nsresult rv = CheckToken(aToken);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsAttrValue* attr = GetParsedAttr();

  if (attr && attr->Contains(aToken)) {
    RemoveInternal(attr, aToken);
    *aResult = false;
  } else {
    AddInternal(attr, aToken);
    *aResult = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMTokenList::ToString(nsAString& aResult)
{
  if (!mElement) {
    aResult.Truncate();
    return NS_OK;
  }

  mElement->GetAttr(kNameSpaceID_None, mAttrAtom, aResult);

  return NS_OK;
}

JSObject*
nsDOMTokenList::WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap)
{
  return mozilla::dom::binding::DOMTokenList::create(cx, scope, this,
                                                     triedToWrap);
}

