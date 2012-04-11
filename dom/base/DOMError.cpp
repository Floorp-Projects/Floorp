/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMError.h"

#include "mozilla/Util.h"
#include "nsDOMClassInfo.h"
#include "nsDOMException.h"

using mozilla::ArrayLength;
using mozilla::dom::DOMError;

namespace {

struct NameMap
{
  PRUint16 code;
  const char* name;
};

} // anonymous namespace

// static
already_AddRefed<nsIDOMDOMError>
DOMError::CreateForNSResult(nsresult aRv)
{
  const char* name;
  const char* message;
  aRv = NS_GetNameAndMessageForDOMNSResult(aRv, &name, &message);
  if (NS_FAILED(aRv) || !name) {
    return nsnull;
  }
  return CreateWithName(NS_ConvertASCIItoUTF16(name));
}

// static
already_AddRefed<nsIDOMDOMError>
DOMError::CreateForDOMExceptionCode(PRUint16 aDOMExceptionCode)
{
  // All of these codes (and yes, some are skipped) come from the spec.
  static const NameMap kNames[] = {
    {  1, "IndexSizeError" },
    {  3, "HierarchyRequestError" },
    {  4, "WrongDocumentError" },
    {  5, "InvalidCharacterError" },
    {  7, "NoModificationAllowedError" },
    {  8, "NotFoundError" },
    {  9, "NotSupportedError" },
    { 11, "InvalidStateError" },
    { 12, "SyntaxError" },
    { 13, "InvalidModificationError" },
    { 14, "NamespaceError" },
    { 15, "InvalidAccessError" },
    { 17, "TypeMismatchError" },
    { 18, "SecurityError" },
    { 19, "NetworkError" },
    { 20, "AbortError" },
    { 21, "URLMismatchError" },
    { 22, "QuotaExceededError" },
    { 23, "TimeoutError" },
    { 24, "InvalidNodeTypeError" },
    { 25, "DataCloneError" }
  };

  for (size_t index = 0; index < ArrayLength(kNames); index++) {
    if (kNames[index].code == aDOMExceptionCode) {
      nsString name;
      name.AssignASCII(kNames[index].name);
      return CreateWithName(name);
    }
  }

  NS_NOTREACHED("Unknown DOMException code!");
  return nsnull;
}

NS_IMPL_ADDREF(DOMError)
NS_IMPL_RELEASE(DOMError)

NS_INTERFACE_MAP_BEGIN(DOMError)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMError)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMError)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMCI_DATA(DOMError, DOMError)

NS_IMETHODIMP
DOMError::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}
