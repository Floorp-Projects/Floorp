/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChangeStyleTxn.h"

#include "mozilla/dom/Element.h"        // for Element
#include "nsAString.h"                  // for nsAString_internal::Append, etc
#include "nsCRT.h"                      // for nsCRT::IsAsciiSpace
#include "nsDebug.h"                    // for NS_ENSURE_SUCCESS, etc
#include "nsError.h"                    // for NS_ERROR_NULL_POINTER, etc
#include "nsGkAtoms.h"                  // for nsGkAtoms, etc
#include "nsIDOMCSSStyleDeclaration.h"  // for nsIDOMCSSStyleDeclaration
#include "nsIDOMElementCSSInlineStyle.h" // for nsIDOMElementCSSInlineStyle
#include "nsLiteralString.h"            // for NS_LITERAL_STRING, etc
#include "nsReadableUtils.h"            // for ToNewUnicode
#include "nsString.h"                   // for nsAutoString, nsString, etc
#include "nsUnicharUtils.h"             // for nsCaseInsensitiveStringComparator
#include "nsXPCOM.h"                    // for NS_Free

using namespace mozilla;
using namespace mozilla::dom;

#define kNullCh (char16_t('\0'))

NS_IMPL_CYCLE_COLLECTION_INHERITED(ChangeStyleTxn, EditTxn, mElement)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChangeStyleTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMPL_ADDREF_INHERITED(ChangeStyleTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(ChangeStyleTxn, EditTxn)

ChangeStyleTxn::~ChangeStyleTxn()
{
}

// Answers true if aValue is in the string list of white-space separated values
// aValueList.
bool
ChangeStyleTxn::ValueIncludes(const nsAString &aValueList,
                              const nsAString &aValue)
{
  nsAutoString valueList(aValueList);
  bool result = false;

  // put an extra null at the end
  valueList.Append(kNullCh);

  char16_t* value = ToNewUnicode(aValue);
  char16_t* start = valueList.BeginWriting();
  char16_t* end = start;

  while (kNullCh != *start) {
    while (kNullCh != *start && nsCRT::IsAsciiSpace(*start)) {
      // skip leading space
      start++;
    }
    end = start;

    while (kNullCh != *end && !nsCRT::IsAsciiSpace(*end)) {
      // look for space or end
      end++;
    }
    // end string here
    *end = kNullCh;

    if (start < end) {
      if (nsDependentString(value).Equals(nsDependentString(start),
            nsCaseInsensitiveStringComparator())) {
        result = true;
        break;
      }
    }
    start = ++end;
  }
  NS_Free(value);
  return result;
}

// Removes the value aRemoveValue from the string list of white-space separated
// values aValueList
void
ChangeStyleTxn::RemoveValueFromListOfValues(nsAString& aValues,
                                            const nsAString& aRemoveValue)
{
  nsAutoString classStr(aValues);
  nsAutoString outString;
  // put an extra null at the end
  classStr.Append(kNullCh);

  char16_t* start = classStr.BeginWriting();
  char16_t* end = start;

  while (kNullCh != *start) {
    while (kNullCh != *start && nsCRT::IsAsciiSpace(*start)) {
      // skip leading space
      start++;
    }
    end = start;

    while (kNullCh != *end && !nsCRT::IsAsciiSpace(*end)) {
      // look for space or end
      end++;
    }
    // end string here
    *end = kNullCh;

    if (start < end && !aRemoveValue.Equals(start)) {
      outString.Append(start);
      outString.Append(char16_t(' '));
    }

    start = ++end;
  }
  aValues.Assign(outString);
}

ChangeStyleTxn::ChangeStyleTxn(Element& aElement, nsIAtom& aProperty,
                               const nsAString& aValue,
                               EChangeType aChangeType)
  : EditTxn()
  , mElement(&aElement)
  , mProperty(&aProperty)
  , mValue(aValue)
  , mRemoveProperty(aChangeType == eRemove)
  , mUndoValue()
  , mRedoValue()
  , mUndoAttributeWasSet(false)
  , mRedoAttributeWasSet(false)
{
}

NS_IMETHODIMP
ChangeStyleTxn::DoTransaction()
{
  nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyles =
    do_QueryInterface(mElement);
  NS_ENSURE_TRUE(inlineStyles, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  nsresult result = inlineStyles->GetStyle(getter_AddRefs(cssDecl));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(cssDecl, NS_ERROR_NULL_POINTER);

  nsAutoString propertyNameString;
  mProperty->ToString(propertyNameString);

  mUndoAttributeWasSet = mElement->HasAttr(kNameSpaceID_None,
                                           nsGkAtoms::style);

  nsAutoString values;
  result = cssDecl->GetPropertyValue(propertyNameString, values);
  NS_ENSURE_SUCCESS(result, result);
  mUndoValue.Assign(values);

  // Does this property accept more than one value? (bug 62682)
  bool multiple = AcceptsMoreThanOneValue(*mProperty);

  if (mRemoveProperty) {
    nsAutoString returnString;
    if (multiple) {
      // Let's remove only the value we have to remove and not the others

      // The two lines below are a workaround because
      // nsDOMCSSDeclaration::GetPropertyCSSValue is not yet implemented (bug
      // 62682)
      RemoveValueFromListOfValues(values, NS_LITERAL_STRING("none"));
      RemoveValueFromListOfValues(values, mValue);
      if (values.IsEmpty()) {
        result = cssDecl->RemoveProperty(propertyNameString, returnString);
        NS_ENSURE_SUCCESS(result, result);
      } else {
        nsAutoString priority;
        result = cssDecl->GetPropertyPriority(propertyNameString, priority);
        NS_ENSURE_SUCCESS(result, result);
        result = cssDecl->SetProperty(propertyNameString, values,
                                      priority);
        NS_ENSURE_SUCCESS(result, result);
      }
    } else {
      result = cssDecl->RemoveProperty(propertyNameString, returnString);
      NS_ENSURE_SUCCESS(result, result);
    }
  } else {
    nsAutoString priority;
    result = cssDecl->GetPropertyPriority(propertyNameString, priority);
    NS_ENSURE_SUCCESS(result, result);
    if (multiple) {
      // Let's add the value we have to add to the others

      // The line below is a workaround because
      // nsDOMCSSDeclaration::GetPropertyCSSValue is not yet implemented (bug
      // 62682)
      AddValueToMultivalueProperty(values, mValue);
    } else {
      values.Assign(mValue);
    }
    result = cssDecl->SetProperty(propertyNameString, values,
                                  priority);
    NS_ENSURE_SUCCESS(result, result);
  }

  // Let's be sure we don't keep an empty style attribute
  uint32_t length;
  result = cssDecl->GetLength(&length);
  NS_ENSURE_SUCCESS(result, result);
  if (!length) {
    result = mElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::style, true);
    NS_ENSURE_SUCCESS(result, result);
  } else {
    mRedoAttributeWasSet = true;
  }

  return cssDecl->GetPropertyValue(propertyNameString, mRedoValue);
}

nsresult
ChangeStyleTxn::SetStyle(bool aAttributeWasSet, nsAString& aValue)
{
  nsresult result = NS_OK;
  if (aAttributeWasSet) {
    // The style attribute was not empty, let's recreate the declaration
    nsAutoString propertyNameString;
    mProperty->ToString(propertyNameString);

    nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyles =
      do_QueryInterface(mElement);
    NS_ENSURE_TRUE(inlineStyles, NS_ERROR_NULL_POINTER);
    nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
    result = inlineStyles->GetStyle(getter_AddRefs(cssDecl));
    NS_ENSURE_SUCCESS(result, result);
    NS_ENSURE_TRUE(cssDecl, NS_ERROR_NULL_POINTER);

    if (aValue.IsEmpty()) {
      // An empty value means we have to remove the property
      nsAutoString returnString;
      result = cssDecl->RemoveProperty(propertyNameString, returnString);
    } else {
      // Let's recreate the declaration as it was
      nsAutoString priority;
      result = cssDecl->GetPropertyPriority(propertyNameString, priority);
      NS_ENSURE_SUCCESS(result, result);
      result = cssDecl->SetProperty(propertyNameString, aValue, priority);
    }
  } else {
    result = mElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::style, true);
  }

  return result;
}

NS_IMETHODIMP
ChangeStyleTxn::UndoTransaction()
{
  return SetStyle(mUndoAttributeWasSet, mUndoValue);
}

NS_IMETHODIMP
ChangeStyleTxn::RedoTransaction()
{
  return SetStyle(mRedoAttributeWasSet, mRedoValue);
}

NS_IMETHODIMP
ChangeStyleTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("ChangeStyleTxn: [mRemoveProperty == ");

  if (mRemoveProperty) {
    aString.AppendLiteral("true] ");
  } else {
    aString.AppendLiteral("false] ");
  }
  aString += nsDependentAtomString(mProperty);
  return NS_OK;
}

// True if the CSS property accepts more than one value
bool
ChangeStyleTxn::AcceptsMoreThanOneValue(nsIAtom& aCSSProperty)
{
  return &aCSSProperty == nsGkAtoms::text_decoration;
}

// Adds the value aNewValue to the list of white-space separated values aValues
void
ChangeStyleTxn::AddValueToMultivalueProperty(nsAString& aValues,
                                             const nsAString& aNewValue)
{
  if (aValues.IsEmpty() || aValues.LowerCaseEqualsLiteral("none")) {
    aValues.Assign(aNewValue);
  } else if (!ValueIncludes(aValues, aNewValue)) {
    // We already have another value but not this one; add it
    aValues.Append(char16_t(' '));
    aValues.Append(aNewValue);
  }
}
