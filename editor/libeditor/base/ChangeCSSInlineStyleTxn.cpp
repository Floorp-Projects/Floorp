/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: Daniel Glazman <glazman@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "ChangeCSSInlineStyleTxn.h"
#include "nsIDOMElement.h"
#include "nsEditor.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIContent.h"
#include "nsIEditProperty.h"
#include "nsIParser.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"

void
ChangeCSSInlineStyleTxn::AppendDeclaration(nsAString & aOutputString,
                                           const nsAString & aProperty,
                                           const nsAString & aValues)
{
  aOutputString.Append(aProperty
                       + NS_LITERAL_STRING(": ")
                       + aValues
                       + NS_LITERAL_STRING("; "));
}

// answers true if aValue is in the string list of white-space separated values aValueList
// a case-sensitive search is performed if aCaseSensitive is true
PRBool
ChangeCSSInlineStyleTxn::ValueIncludes(const nsAString &aValueList, const nsAString &aValue, PRBool aCaseSensitive)
{
  nsAutoString  valueList(aValueList);
  PRBool result = PR_FALSE;

  valueList.Append(kNullCh);  // put an extra null at the end

  PRUnichar *value = ToNewUnicode(aValue);
  PRUnichar *start = (PRUnichar*)(const PRUnichar*)valueList.get();
  PRUnichar *end   = start;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (PR_FALSE == nsCRT::IsAsciiSpace(*end))) { // look for space or end
      end++;
    }
    *end = kNullCh; // end string here

    if (start < end) {
      if (aCaseSensitive) {
        if (!nsCRT::strcmp(value, start)) {
          result = PR_TRUE;
          break;
        }
      }
      else {
        if (nsDependentString(value).Equals(nsDependentString(start),
                                            nsCaseInsensitiveStringComparator())) {
          result = PR_TRUE;
          break;
        }
      }
    }
    start = ++end;
  }
  nsCRT::free(value);
  return result;
}

// removes the value aRemoveValue from the string list of white-space separated values aValueList
void
ChangeCSSInlineStyleTxn::RemoveValueFromListOfValues(nsAString & aValues, const nsAString  & aRemoveValue)
{
  nsAutoString  classStr(aValues);  // copy to work buffer   nsAutoString  rv(aRemoveValue);
  nsAutoString  outString;
  classStr.Append(kNullCh);  // put an extra null at the end

  PRUnichar *start = (PRUnichar*)(const PRUnichar*)classStr.get();
  PRUnichar *end   = start;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (PR_FALSE == nsCRT::IsAsciiSpace(*end))) { // look for space or end
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

ChangeCSSInlineStyleTxn::~ChangeCSSInlineStyleTxn()
{
}

NS_IMETHODIMP ChangeCSSInlineStyleTxn::Init(nsIEditor      *aEditor,
                                            nsIDOMElement  *aElement,
                                            nsIAtom        *aProperty,
                                            const nsAString& aValue,
                                            PRBool aRemoveProperty)
{
  NS_ASSERTION(aEditor && aElement, "bad arg");
  if (!aEditor || !aElement) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;
  mElement = do_QueryInterface(aElement);
  mProperty = aProperty;
  NS_ADDREF(mProperty);
  mValue.Assign(aValue);
  mRemoveProperty = aRemoveProperty;
  mPropertyWasSet = PR_FALSE;
  mUndoAttributeWasSet = PR_FALSE;
  mRedoAttributeWasSet = PR_FALSE;
  mUndoValue.SetLength(0);
  return NS_OK;
}

NS_IMETHODIMP ChangeCSSInlineStyleTxn::DoTransaction(void)
{
  NS_ASSERTION(mEditor && mElement, "bad state");
  if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }

  nsresult result=NS_OK;

  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  PRUint32 length = 0;
  nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyles = do_QueryInterface(mElement);
  if (!inlineStyles) return NS_ERROR_NULL_POINTER;
  result = inlineStyles->GetStyle(getter_AddRefs(cssDecl));
  if (NS_FAILED(result)) return result;
  if (!cssDecl) return NS_ERROR_NULL_POINTER;
  result = cssDecl->GetLength(&length);
  if (NS_FAILED(result)) return result;

  nsAutoString newDeclString, propertyNameString, undoString, redoString;
  mProperty->ToString(propertyNameString);

  mPropertyWasSet = PR_FALSE;
  // does this property accept more than 1 value ?
  PRBool multiple = AcceptsMoreThanOneValue(mProperty);
  
  nsAutoString styleAttr(NS_LITERAL_STRING("style"));

  result = mEditor->GetAttributeValue(mElement, styleAttr, mUndoValue, &mUndoAttributeWasSet);
  if (NS_FAILED(result)) return result;

  if (0 == length && !mRemoveProperty && mUndoAttributeWasSet) {
    // dirty case, the style engine did not have the time to parse the style attribute
    newDeclString.Append(mUndoValue + NS_LITERAL_STRING(" "));
    AppendDeclaration(newDeclString, propertyNameString, mValue);
    result = mElement->SetAttribute(styleAttr, newDeclString);
  }
  else if (0 == length) {
    if (mRemoveProperty) {
      // there is no style attribute or it is empty and we want to remove something
      // so it is an early way out
      result = mElement->RemoveAttribute(styleAttr);
    }
    else {
      // the style attribute is empty or absent and we want to add a property
      // let's do it...
      AppendDeclaration(newDeclString, propertyNameString, mValue);
      result = mElement->SetAttribute(styleAttr, newDeclString);
    }
  }
  else if (mRemoveProperty && (1 == length)) {
    // let's deal with a special case : we want to remove a property from the
    // style attribute and it contains only one declaration...
    // if it is the one we look for, let's remove the attribute ! Otherwise,
    // do nothing.
    nsAutoString itemPropertyNameString;
    cssDecl->Item(0, itemPropertyNameString);
    PRBool thisOne = propertyNameString.Equals(itemPropertyNameString, nsCaseInsensitiveStringComparator());
    if (thisOne) {
      mPropertyWasSet = PR_TRUE;
      if (multiple) {
        // the property accepts more than one value...
        // cssDecl->GetPropertyValue(propertyNameString, mUndoValue);
        nsAutoString values;
        cssDecl->GetPropertyValue(propertyNameString, values);
        RemoveValueFromListOfValues(values, NS_LITERAL_STRING("none"));
        RemoveValueFromListOfValues(values, mValue);
        if (0 != values.Length()) {
          AppendDeclaration(newDeclString, propertyNameString, values);
        }
        result = mElement->SetAttribute(styleAttr, newDeclString);
      }
      else {
        result = mElement->RemoveAttribute(styleAttr);
      }
    }
  }
  else {
    // general case, we are going to browse all the CSS declarations in the
    // style attribute, and, if needed, remove or rewrite the one we want
    PRUint32 index;
    nsAutoString itemPropertyNameString;
    for (index = 0 ; index < length; index++) {
      cssDecl->Item(index, itemPropertyNameString);
      PRBool thisOne = propertyNameString.Equals(itemPropertyNameString, nsCaseInsensitiveStringComparator());
      if (thisOne) {
        // we have found the property we are looking for...
        // if we have to remove it, do nothing or remove only the corresponding value
        // if we have to change it, do it below
        if (mRemoveProperty) {
          if (multiple) {
            nsAutoString values;
            cssDecl->GetPropertyValue(propertyNameString, values);
            RemoveValueFromListOfValues(values, NS_LITERAL_STRING("none"));
            RemoveValueFromListOfValues(values, mValue);
            if (0 != values.Length()) {
              AppendDeclaration(newDeclString, propertyNameString, values);
            }
          }
        }
        else {
          newDeclString.Append(propertyNameString + NS_LITERAL_STRING(": "));
          if (multiple) {
            nsAutoString values;
            cssDecl->GetPropertyValue(propertyNameString, values);
            AddValueToMultivalueProperty(values, mValue);
            newDeclString.Append(values);
          }
          else {
            newDeclString.Append(mValue);
          }
          newDeclString.Append(NS_LITERAL_STRING("; "));
        }
        mPropertyWasSet = PR_TRUE;
      }
      else {
        // this is not the property we are looking for, let's recreate the declaration
        nsAutoString stringValue;
        cssDecl->GetPropertyValue(itemPropertyNameString, stringValue);
        AppendDeclaration(newDeclString, itemPropertyNameString, stringValue);
      }
    }
    // if we DON'T have to remove the property, ie if we have to change or set it,
    // did we find it earlier ? If not, let's set it now...
    if (!mRemoveProperty && !mPropertyWasSet) {
      AppendDeclaration(newDeclString, propertyNameString, mValue);
    }
    result = mElement->SetAttribute(styleAttr, newDeclString);
  }
  result = mEditor->GetAttributeValue(mElement, styleAttr, mRedoValue, &mRedoAttributeWasSet);
  return result;
}

// to undo a transaction, just reset the style attribute to its former value...
NS_IMETHODIMP ChangeCSSInlineStyleTxn::UndoTransaction(void)
{
  NS_ASSERTION(mEditor && mElement, "bad state");
  if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }

  nsresult result=NS_OK;
  if (PR_TRUE == mUndoAttributeWasSet)
    result = mElement->SetAttribute(NS_LITERAL_STRING("style"), mUndoValue);
  else
    result = mElement->RemoveAttribute(NS_LITERAL_STRING("style"));

  return result;
}

// to redo a transaction, just set again the style attribute
NS_IMETHODIMP ChangeCSSInlineStyleTxn::RedoTransaction(void)
{
  NS_ASSERTION(mEditor && mElement, "bad state");
  if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }

  nsresult result=NS_OK;
  if (PR_TRUE == mRedoAttributeWasSet)
    result = mElement->SetAttribute(NS_LITERAL_STRING("style"), mRedoValue);
  else
    result = mElement->RemoveAttribute(NS_LITERAL_STRING("style"));

  return result;
}

NS_IMETHODIMP ChangeCSSInlineStyleTxn::Merge(nsITransaction *aTransaction, PRBool *aDidMerge)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP ChangeCSSInlineStyleTxn::GetTxnDescription(nsAString& aString)
{
  aString.Assign(NS_LITERAL_STRING("ChangeCSSInlineStyleTxn: "));

  if (PR_FALSE==mRemoveProperty)
    aString += NS_LITERAL_STRING("[mRemoveProperty == false] ");
  else
    aString += NS_LITERAL_STRING("[mRemoveProperty == true] ");
  nsAutoString tempString;
  mProperty->ToString(tempString);
  aString += tempString;
  return NS_OK;
}

// answers true if the CSS property accepts more than one value
PRBool
ChangeCSSInlineStyleTxn::AcceptsMoreThanOneValue(nsIAtom *aCSSProperty)
{
  PRBool res = PR_FALSE;
  nsIAtom * textDecorationAtom = NS_NewAtom("text-decoration");
  if (textDecorationAtom == aCSSProperty) {
    res = PR_TRUE;
  }
  NS_IF_RELEASE(textDecorationAtom);
  return res;
}

// adds the value aNewValue to the list of white-space separated values aValues
NS_IMETHODIMP
ChangeCSSInlineStyleTxn::AddValueToMultivalueProperty(nsAString & aValues, const nsAString & aNewValue)
{
  if (aValues.IsEmpty()
      || aValues.Equals(NS_LITERAL_STRING("none"),
                        nsCaseInsensitiveStringComparator())) {
    // the list of values is empty of the value is 'none'
    aValues.Assign(aNewValue);
  }
  else if (!ValueIncludes(aValues, aNewValue, PR_FALSE)) {
    // we already have another value but not this one; add it
    aValues.Append(PRUnichar(' '));
    aValues.Append(aNewValue);
  }
  return NS_OK;
}

