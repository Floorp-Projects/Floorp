/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChangeCSSInlineStyleTxn.h"
#include "nsAString.h"                  // for nsAString_internal::Append, etc
#include "nsCRT.h"                      // for nsCRT
#include "nsDebug.h"                    // for NS_ENSURE_SUCCESS, etc
#include "nsError.h"                    // for NS_ERROR_NULL_POINTER, etc
#include "nsGkAtoms.h"                  // for nsGkAtoms, etc
#include "nsIAtom.h"                    // for nsIAtom
#include "nsIDOMCSSStyleDeclaration.h"  // for nsIDOMCSSStyleDeclaration
#include "nsIDOMElement.h"              // for nsIDOMElement
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsISupportsImpl.h"            // for EditTxn::QueryInterface, etc
#include "nsISupportsUtils.h"           // for NS_ADDREF
#include "nsLiteralString.h"            // for NS_LITERAL_STRING, etc
#include "nsReadableUtils.h"            // for ToNewUnicode
#include "nsString.h"                   // for nsAutoString, nsString, etc
#include "nsUnicharUtils.h"
#include "nsXPCOM.h"                    // for NS_Free

class nsIEditor;

#define kNullCh (PRUnichar('\0'))

NS_IMPL_CYCLE_COLLECTION_CLASS(ChangeCSSInlineStyleTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ChangeCSSInlineStyleTxn,
                                                EditTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ChangeCSSInlineStyleTxn,
                                                  EditTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChangeCSSInlineStyleTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

// answers true if aValue is in the string list of white-space separated values aValueList
// a case-sensitive search is performed if aCaseSensitive is true
bool
ChangeCSSInlineStyleTxn::ValueIncludes(const nsAString &aValueList, const nsAString &aValue, bool aCaseSensitive)
{
  nsAutoString  valueList(aValueList);
  bool result = false;

  valueList.Append(kNullCh);  // put an extra null at the end

  PRUnichar *value = ToNewUnicode(aValue);
  PRUnichar *start = valueList.BeginWriting();
  PRUnichar *end   = start;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (false == nsCRT::IsAsciiSpace(*end))) { // look for space or end
      end++;
    }
    *end = kNullCh; // end string here

    if (start < end) {
      if (aCaseSensitive) {
        if (value && !NS_strcmp(value, start)) {
          result = true;
          break;
        }
      }
      else {
        if (nsDependentString(value).Equals(nsDependentString(start),
                                            nsCaseInsensitiveStringComparator())) {
          result = true;
          break;
        }
      }
    }
    start = ++end;
  }
  NS_Free(value);
  return result;
}

// removes the value aRemoveValue from the string list of white-space separated values aValueList
void
ChangeCSSInlineStyleTxn::RemoveValueFromListOfValues(nsAString & aValues, const nsAString  & aRemoveValue)
{
  nsAutoString  classStr(aValues);  // copy to work buffer   nsAutoString  rv(aRemoveValue);
  nsAutoString  outString;
  classStr.Append(kNullCh);  // put an extra null at the end

  PRUnichar *start = classStr.BeginWriting();
  PRUnichar *end   = start;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (false == nsCRT::IsAsciiSpace(*end))) { // look for space or end
      end++;
    }
    *end = kNullCh; // end string here

    if (start < end) {
      if (!aRemoveValue.Equals(start)) {
        outString.Append(start);
        outString.Append(PRUnichar(' '));
      }
    }

    start = ++end;
  }
  aValues.Assign(outString);
}

ChangeCSSInlineStyleTxn::ChangeCSSInlineStyleTxn()
  : EditTxn()
{
}

NS_IMETHODIMP ChangeCSSInlineStyleTxn::Init(nsIEditor      *aEditor,
                                            nsIDOMElement  *aElement,
                                            nsIAtom        *aProperty,
                                            const nsAString& aValue,
                                            bool aRemoveProperty)
{
  NS_ASSERTION(aEditor && aElement, "bad arg");
  if (!aEditor || !aElement) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;
  mElement = do_QueryInterface(aElement);
  mProperty = aProperty;
  NS_ADDREF(mProperty);
  mValue.Assign(aValue);
  mRemoveProperty = aRemoveProperty;
  mUndoAttributeWasSet = false;
  mRedoAttributeWasSet = false;
  mUndoValue.Truncate();
  mRedoValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP ChangeCSSInlineStyleTxn::DoTransaction(void)
{
  NS_ASSERTION(mEditor && mElement, "bad state");
  if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyles = do_QueryInterface(mElement);
  NS_ENSURE_TRUE(inlineStyles, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  nsresult result = inlineStyles->GetStyle(getter_AddRefs(cssDecl));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(cssDecl, NS_ERROR_NULL_POINTER);

  nsAutoString propertyNameString;
  mProperty->ToString(propertyNameString);

  NS_NAMED_LITERAL_STRING(styleAttr, "style");
  result = mElement->HasAttribute(styleAttr, &mUndoAttributeWasSet);
  NS_ENSURE_SUCCESS(result, result);

  nsAutoString values;
  result = cssDecl->GetPropertyValue(propertyNameString, values);
  NS_ENSURE_SUCCESS(result, result);     
  mUndoValue.Assign(values);

  // does this property accept more than 1 value ?
  // we need to know that because of bug 62682
  bool multiple = AcceptsMoreThanOneValue(mProperty);
  
  if (mRemoveProperty) {
    nsAutoString returnString;
    if (multiple) {
      // the property can have more than one value, let's remove only
      // the value we have to remove and not the others

      // the 2 lines below are a workaround because nsDOMCSSDeclaration::GetPropertyCSSValue
      // is not yet implemented (bug 62682)
      RemoveValueFromListOfValues(values, NS_LITERAL_STRING("none"));
      RemoveValueFromListOfValues(values, mValue);
      if (values.IsEmpty()) {
        result = cssDecl->RemoveProperty(propertyNameString, returnString);
        NS_ENSURE_SUCCESS(result, result);     
      }
      else {
        nsAutoString priority;
        result = cssDecl->GetPropertyPriority(propertyNameString, priority);
        NS_ENSURE_SUCCESS(result, result);     
        result = cssDecl->SetProperty(propertyNameString, values,
                                      priority);
        NS_ENSURE_SUCCESS(result, result);     
      }
    }
    else {
      result = cssDecl->RemoveProperty(propertyNameString, returnString);
      NS_ENSURE_SUCCESS(result, result);     
    }
  }
  else {
    nsAutoString priority;
    result = cssDecl->GetPropertyPriority(propertyNameString, priority);
    NS_ENSURE_SUCCESS(result, result);
    if (multiple) {
      // the property can have more than one value, let's add
      // the value we have to add to the others

      // the line below is a workaround because nsDOMCSSDeclaration::GetPropertyCSSValue
      // is not yet implemented (bug 62682)
      AddValueToMultivalueProperty(values, mValue);
    }
    else
      values.Assign(mValue);
    result = cssDecl->SetProperty(propertyNameString, values,
                                  priority);
    NS_ENSURE_SUCCESS(result, result);     
  }

  // let's be sure we don't keep an empty style attribute
  uint32_t length;
  result = cssDecl->GetLength(&length);
  NS_ENSURE_SUCCESS(result, result);     
  if (!length) {
    result = mElement->RemoveAttribute(styleAttr);
    NS_ENSURE_SUCCESS(result, result);     
  }
  else
    mRedoAttributeWasSet = true;

  return cssDecl->GetPropertyValue(propertyNameString, mRedoValue);
}

nsresult ChangeCSSInlineStyleTxn::SetStyle(bool aAttributeWasSet,
                                           nsAString & aValue)
{
  NS_ASSERTION(mEditor && mElement, "bad state");
  if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }

  nsresult result = NS_OK;
  if (aAttributeWasSet) {
    // the style attribute was set and not empty, let's recreate the declaration
    nsAutoString propertyNameString;
    mProperty->ToString(propertyNameString);

    nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyles = do_QueryInterface(mElement);
    NS_ENSURE_TRUE(inlineStyles, NS_ERROR_NULL_POINTER);
    nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
    result = inlineStyles->GetStyle(getter_AddRefs(cssDecl));
    NS_ENSURE_SUCCESS(result, result);
    NS_ENSURE_TRUE(cssDecl, NS_ERROR_NULL_POINTER);

    if (aValue.IsEmpty()) {
      // an empty value means we have to remove the property
      nsAutoString returnString;
      result = cssDecl->RemoveProperty(propertyNameString, returnString);
    }
    else {
      // let's recreate the declaration as it was
      nsAutoString priority;
      result = cssDecl->GetPropertyPriority(propertyNameString, priority);
      NS_ENSURE_SUCCESS(result, result);
      result = cssDecl->SetProperty(propertyNameString, aValue, priority);
    }
  }
  else
    result = mElement->RemoveAttribute(NS_LITERAL_STRING("style"));

  return result;
}

NS_IMETHODIMP ChangeCSSInlineStyleTxn::UndoTransaction(void)
{
  return SetStyle(mUndoAttributeWasSet, mUndoValue);
}

NS_IMETHODIMP ChangeCSSInlineStyleTxn::RedoTransaction(void)
{
  return SetStyle(mRedoAttributeWasSet, mRedoValue);
}

NS_IMETHODIMP ChangeCSSInlineStyleTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("ChangeCSSInlineStyleTxn: [mRemoveProperty == ");

  if (!mRemoveProperty)
    aString.AppendLiteral("false] ");
  else
    aString.AppendLiteral("true] ");
  nsAutoString tempString;
  mProperty->ToString(tempString);
  aString += tempString;
  return NS_OK;
}

// answers true if the CSS property accepts more than one value
bool
ChangeCSSInlineStyleTxn::AcceptsMoreThanOneValue(nsIAtom *aCSSProperty)
{
  return aCSSProperty == nsGkAtoms::text_decoration;
}

// adds the value aNewValue to the list of white-space separated values aValues
NS_IMETHODIMP
ChangeCSSInlineStyleTxn::AddValueToMultivalueProperty(nsAString & aValues, const nsAString & aNewValue)
{
  if (aValues.IsEmpty()
      || aValues.LowerCaseEqualsLiteral("none")) {
    // the list of values is empty of the value is 'none'
    aValues.Assign(aNewValue);
  }
  else if (!ValueIncludes(aValues, aNewValue, false)) {
    // we already have another value but not this one; add it
    aValues.Append(PRUnichar(' '));
    aValues.Append(aNewValue);
  }
  return NS_OK;
}
