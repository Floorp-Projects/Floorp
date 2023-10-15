/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChangeStyleTransaction.h"

#include "HTMLEditUtils.h"
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

  mUndoAttributeWasSet = mStyledElement->HasAttr(nsGkAtoms::style);

  nsAutoCString values;
  cssDecl->GetPropertyValue(propertyNameString, values);
  mUndoValue.Assign(values);

  if (mRemoveProperty) {
    nsAutoCString returnString;
    if (mProperty == nsGkAtoms::text_decoration) {
      BuildTextDecorationValueToRemove(values, mValue, values);
      if (values.IsEmpty()) {
        ErrorResult error;
        cssDecl->RemoveProperty(propertyNameString, returnString, error);
        if (MOZ_UNLIKELY(error.Failed())) {
          NS_WARNING("nsICSSDeclaration::RemoveProperty() failed");
          return error.StealNSResult();
        }
      } else {
        ErrorResult error;
        nsAutoCString priority;
        cssDecl->GetPropertyPriority(propertyNameString, priority);
        cssDecl->SetProperty(propertyNameString, values, priority, error);
        if (MOZ_UNLIKELY(error.Failed())) {
          NS_WARNING("nsICSSDeclaration::SetProperty() failed");
          return error.StealNSResult();
        }
      }
    } else {
      ErrorResult error;
      cssDecl->RemoveProperty(propertyNameString, returnString, error);
      if (MOZ_UNLIKELY(error.Failed())) {
        NS_WARNING("nsICSSDeclaration::RemoveProperty() failed");
        return error.StealNSResult();
      }
    }
  } else {
    nsAutoCString priority;
    cssDecl->GetPropertyPriority(propertyNameString, priority);
    if (mProperty == nsGkAtoms::text_decoration) {
      BuildTextDecorationValueToSet(values, mValue, values);
    } else {
      values.Assign(mValue);
    }
    ErrorResult error;
    cssDecl->SetProperty(propertyNameString, values, priority, error);
    if (MOZ_UNLIKELY(error.Failed())) {
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

  cssDecl->GetPropertyValue(propertyNameString, mRedoValue);
  return NS_OK;
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
      if (MOZ_UNLIKELY(error.Failed())) {
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

// static
void ChangeStyleTransaction::BuildTextDecorationValueToSet(
    const nsACString& aCurrentValues, const nsACString& aAddingValues,
    nsACString& aOutValues) {
  const bool underline = ValueIncludes(aCurrentValues, "underline"_ns) ||
                         ValueIncludes(aAddingValues, "underline"_ns);
  const bool overline = ValueIncludes(aCurrentValues, "overline"_ns) ||
                        ValueIncludes(aAddingValues, "overline"_ns);
  const bool lineThrough = ValueIncludes(aCurrentValues, "line-through"_ns) ||
                           ValueIncludes(aAddingValues, "line-through"_ns);
  // FYI: Don't refer aCurrentValues which may refer same instance as
  // aOutValues.
  BuildTextDecorationValue(underline, overline, lineThrough, aOutValues);
}

// static
void ChangeStyleTransaction::BuildTextDecorationValueToRemove(
    const nsACString& aCurrentValues, const nsACString& aRemovingValues,
    nsACString& aOutValues) {
  const bool underline = ValueIncludes(aCurrentValues, "underline"_ns) &&
                         !ValueIncludes(aRemovingValues, "underline"_ns);
  const bool overline = ValueIncludes(aCurrentValues, "overline"_ns) &&
                        !ValueIncludes(aRemovingValues, "overline"_ns);
  const bool lineThrough = ValueIncludes(aCurrentValues, "line-through"_ns) &&
                           !ValueIncludes(aRemovingValues, "line-through"_ns);
  // FYI: Don't refer aCurrentValues which may refer same instance as
  // aOutValues.
  BuildTextDecorationValue(underline, overline, lineThrough, aOutValues);
}

void ChangeStyleTransaction::BuildTextDecorationValue(bool aUnderline,
                                                      bool aOverline,
                                                      bool aLineThrough,
                                                      nsACString& aOutValues) {
  // We should build text-decoration(-line) value as same as Blink for
  // compatibility.  Blink sets text-decoration-line to the values in the
  // following order.  Blink drops `blink` and other styles like color and
  // style.  For keeping the code simple, let's use the lossy behavior.
  aOutValues.Truncate();
  if (aUnderline) {
    aOutValues.AssignLiteral("underline");
  }
  if (aOverline) {
    if (!aOutValues.IsEmpty()) {
      aOutValues.Append(HTMLEditUtils::kSpace);
    }
    aOutValues.AppendLiteral("overline");
  }
  if (aLineThrough) {
    if (!aOutValues.IsEmpty()) {
      aOutValues.Append(HTMLEditUtils::kSpace);
    }
    aOutValues.AppendLiteral("line-through");
  }
}

}  // namespace mozilla
