/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChangeStyleTransaction.h"

#include "mozilla/Logging.h"
#include "mozilla/ToString.h"
#include "mozilla/dom/Element.h"  // for Element
#include "nsAString.h"            // for nsAString::Append, etc.
#include "nsCRT.h"                // for nsCRT::IsAsciiSpace
#include "nsDebug.h"              // for NS_WARNING, etc.
#include "nsError.h"              // for NS_ERROR_NULL_POINTER, etc.
#include "nsGkAtoms.h"            // for nsGkAtoms, etc.
#include "nsICSSDeclaration.h"    // for nsICSSDeclaration.
#include "nsLiteralString.h"      // for NS_LITERAL_STRING, etc.
#include "nsReadableUtils.h"      // for ToNewUnicode
#include "nsString.h"             // for nsAutoString, nsString, etc.
#include "nsStyledElement.h"      // for nsStyledElement.
#include "nsUnicharUtils.h"       // for nsCaseInsensitiveStringComparator

namespace mozilla {

using namespace dom;

// static
already_AddRefed<ChangeStyleTransaction> ChangeStyleTransaction::Create(
    nsStyledElement& aStyledElement, nsAtom& aProperty,
    const nsAString& aValue) {
  RefPtr<ChangeStyleTransaction> transaction =
      new ChangeStyleTransaction(aStyledElement, aProperty, aValue, false);
  return transaction.forget();
}

// static
already_AddRefed<ChangeStyleTransaction> ChangeStyleTransaction::CreateToRemove(
    nsStyledElement& aStyledElement, nsAtom& aProperty,
    const nsAString& aValue) {
  RefPtr<ChangeStyleTransaction> transaction =
      new ChangeStyleTransaction(aStyledElement, aProperty, aValue, true);
  return transaction.forget();
}

ChangeStyleTransaction::ChangeStyleTransaction(nsStyledElement& aStyledElement,
                                               nsAtom& aProperty,
                                               const nsAString& aValue,
                                               bool aRemove)
    : EditTransactionBase(),
      mStyledElement(&aStyledElement),
      mProperty(&aProperty),
      mUndoValue(),
      mRedoValue(),
      mRemoveProperty(aRemove),
      mUndoAttributeWasSet(false),
      mRedoAttributeWasSet(false) {
  CopyUTF16toUTF8(aValue, mValue);
}

std::ostream& operator<<(std::ostream& aStream,
                         const ChangeStyleTransaction& aTransaction) {
  aStream << "{ mStyledElement=" << aTransaction.mStyledElement.get();
  if (aTransaction.mStyledElement) {
    aStream << " (" << *aTransaction.mStyledElement << ")";
  }
  aStream << ", mProperty=" << nsAtomCString(aTransaction.mProperty).get()
          << ", mValue=\"" << aTransaction.mValue.get() << "\", mUndoValue=\""
          << aTransaction.mUndoValue.get()
          << "\", mRedoValue=" << aTransaction.mRedoValue.get()
          << ", mRemoveProperty="
          << (aTransaction.mRemoveProperty ? "true" : "false")
          << ", mUndoAttributeWasSet="
          << (aTransaction.mUndoAttributeWasSet ? "true" : "false")
          << ", mRedoAttributeWasSet="
          << (aTransaction.mRedoAttributeWasSet ? "true" : "false") << " }";
  return aStream;
}

#define kNullCh ('\0')

NS_IMPL_CYCLE_COLLECTION_INHERITED(ChangeStyleTransaction, EditTransactionBase,
                                   mStyledElement)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChangeStyleTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMPL_ADDREF_INHERITED(ChangeStyleTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(ChangeStyleTransaction, EditTransactionBase)

// Answers true if aValue is in the string list of white-space separated values
// aValueList.
bool ChangeStyleTransaction::ValueIncludes(const nsACString& aValueList,
                                           const nsACString& aValue) {
  nsAutoCString valueList(aValueList);
  bool result = false;

  // put an extra null at the end
  valueList.Append(kNullCh);

  char* start = valueList.BeginWriting();
  char* end = start;

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
      if (aValue.Equals(nsDependentCString(start),
                        nsCaseInsensitiveCStringComparator)) {
        result = true;
        break;
      }
    }
    start = ++end;
  }
  return result;
}

// Removes the value aRemoveValue from the string list of white-space separated
// values aValueList
void ChangeStyleTransaction::RemoveValueFromListOfValues(
    nsACString& aValues, const nsACString& aRemoveValue) {
  nsAutoCString classStr(aValues);
  nsAutoCString outString;
  // put an extra null at the end
  classStr.Append(kNullCh);

  char* start = classStr.BeginWriting();
  char* end = start;

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
      outString.Append(' ');
    }

    start = ++end;
  }
  aValues.Assign(outString);
}

NS_IMETHODIMP ChangeStyleTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ChangeStyleTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mStyledElement)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  OwningNonNull<nsStyledElement> styledElement = *mStyledElement;
  nsCOMPtr<nsICSSDeclaration> cssDecl = styledElement->Style();

  // FIXME(bug 1606994): Using atoms forces a string copy here which is not
  // great.
  nsAutoCString propertyNameString;
  mProperty->ToUTF8String(propertyNameString);

  mUndoAttributeWasSet =
      mStyledElement->HasAttr(kNameSpaceID_None, nsGkAtoms::style);

  nsAutoCString values;
  nsresult rv = cssDecl->GetPropertyValue(propertyNameString, values);
  if (NS_FAILED(rv)) {
    NS_WARNING("nsICSSDeclaration::GetPropertyPriorityValue() failed");
    return rv;
  }
  mUndoValue.Assign(values);

  // Does this property accept more than one value? (bug 62682)
  bool multiple = AcceptsMoreThanOneValue(*mProperty);

  if (mRemoveProperty) {
    nsAutoCString returnString;
    if (multiple) {
      // Let's remove only the value we have to remove and not the others
      RemoveValueFromListOfValues(values, "none"_ns);
      RemoveValueFromListOfValues(values, mValue);
      if (values.IsEmpty()) {
        ErrorResult error;
        cssDecl->RemoveProperty(propertyNameString, returnString, error);
        if (error.Failed()) {
          NS_WARNING("nsICSSDeclaration::RemoveProperty() failed");
          return error.StealNSResult();
        }
      } else {
        ErrorResult error;
        nsAutoCString priority;
        cssDecl->GetPropertyPriority(propertyNameString, priority);
        cssDecl->SetProperty(propertyNameString, values, priority, error);
        if (error.Failed()) {
          NS_WARNING("nsICSSDeclaration::SetProperty() failed");
          return error.StealNSResult();
        }
      }
    } else {
      ErrorResult error;
      cssDecl->RemoveProperty(propertyNameString, returnString, error);
      if (error.Failed()) {
        NS_WARNING("nsICSSDeclaration::RemoveProperty() failed");
        return error.StealNSResult();
      }
    }
  } else {
    nsAutoCString priority;
    cssDecl->GetPropertyPriority(propertyNameString, priority);
    if (multiple) {
      // Let's add the value we have to add to the others
      AddValueToMultivalueProperty(values, mValue);
    } else {
      values.Assign(mValue);
    }
    ErrorResult error;
    cssDecl->SetProperty(propertyNameString, values, priority, error);
    if (error.Failed()) {
      NS_WARNING("nsICSSDeclaration::SetProperty() failed");
      return error.StealNSResult();
    }
  }

  // Let's be sure we don't keep an empty style attribute
  uint32_t length = cssDecl->Length();
  if (!length) {
    nsresult rv =
        styledElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::style, true);
    if (NS_FAILED(rv)) {
      NS_WARNING("Element::UnsetAttr(nsGkAtoms::style) failed");
      return rv;
    }
  } else {
    mRedoAttributeWasSet = true;
  }

  rv = cssDecl->GetPropertyValue(propertyNameString, mRedoValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsICSSDeclaration::GetPropertyValue() failed");
  return rv;
}

nsresult ChangeStyleTransaction::SetStyle(bool aAttributeWasSet,
                                          nsACString& aValue) {
  if (NS_WARN_IF(!mStyledElement)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aAttributeWasSet) {
    OwningNonNull<nsStyledElement> styledElement = *mStyledElement;

    // The style attribute was not empty, let's recreate the declaration
    nsAutoCString propertyNameString;
    mProperty->ToUTF8String(propertyNameString);

    nsCOMPtr<nsICSSDeclaration> cssDecl = styledElement->Style();

    ErrorResult error;
    if (aValue.IsEmpty()) {
      // An empty value means we have to remove the property
      nsAutoCString returnString;
      cssDecl->RemoveProperty(propertyNameString, returnString, error);
      if (error.Failed()) {
        NS_WARNING("nsICSSDeclaration::RemoveProperty() failed");
        return error.StealNSResult();
      }
    }
    // Let's recreate the declaration as it was
    nsAutoCString priority;
    cssDecl->GetPropertyPriority(propertyNameString, priority);
    cssDecl->SetProperty(propertyNameString, aValue, priority, error);
    NS_WARNING_ASSERTION(!error.Failed(),
                         "nsICSSDeclaration::SetProperty() failed");
    return error.StealNSResult();
  }

  OwningNonNull<nsStyledElement> styledElement = *mStyledElement;
  nsresult rv =
      styledElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::style, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Element::UnsetAttr(nsGkAtoms::style) failed");
  return rv;
}

NS_IMETHODIMP ChangeStyleTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ChangeStyleTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  nsresult rv = SetStyle(mUndoAttributeWasSet, mUndoValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "ChangeStyleTransaction::SetStyle() failed");
  return rv;
}

NS_IMETHODIMP ChangeStyleTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ChangeStyleTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  nsresult rv = SetStyle(mRedoAttributeWasSet, mRedoValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "ChangeStyleTransaction::SetStyle() failed");
  return rv;
}

// True if the CSS property accepts more than one value
bool ChangeStyleTransaction::AcceptsMoreThanOneValue(nsAtom& aCSSProperty) {
  return &aCSSProperty == nsGkAtoms::text_decoration;
}

// Adds the value aNewValue to the list of white-space separated values aValues
void ChangeStyleTransaction::AddValueToMultivalueProperty(
    nsACString& aValues, const nsACString& aNewValue) {
  if (aValues.IsEmpty() || aValues.LowerCaseEqualsLiteral("none")) {
    aValues.Assign(aNewValue);
  } else if (!ValueIncludes(aValues, aNewValue)) {
    // We already have another value but not this one; add it
    aValues.Append(char16_t(' '));
    aValues.Append(aNewValue);
  }
}

}  // namespace mozilla
