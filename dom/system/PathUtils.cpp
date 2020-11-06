/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "PathUtils.h"

#include "mozilla/ErrorNames.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Span.h"
#include "mozilla/dom/DOMParser.h"
#include "mozilla/dom/PathUtilsBinding.h"
#include "nsIFile.h"
#include "nsLocalFile.h"
#include "nsNetUtil.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

static constexpr auto ERROR_EMPTY_PATH =
    "PathUtils does not support empty paths"_ns;
static constexpr auto ERROR_INITIALIZE_PATH = "Could not initialize path"_ns;
static constexpr auto ERROR_GET_PARENT = "Could not get parent path"_ns;

static void ThrowError(ErrorResult& aErr, const nsresult aResult,
                       const nsCString& aMessage) {
  nsAutoCStringN<32> errName;
  GetErrorName(aResult, errName);

  nsAutoCStringN<256> formattedMsg;
  formattedMsg.Append(aMessage);
  formattedMsg.Append(": "_ns);
  formattedMsg.Append(errName);

  switch (aResult) {
    case NS_ERROR_FILE_UNRECOGNIZED_PATH:
      aErr.ThrowOperationError(formattedMsg);
      break;

    case NS_ERROR_FILE_ACCESS_DENIED:
      aErr.ThrowInvalidAccessError(formattedMsg);
      break;

    case NS_ERROR_FAILURE:
    default:
      aErr.ThrowUnknownError(formattedMsg);
      break;
  }
}

/**
 * Return the leaf name, including leading path separators in the case of
 * Windows UNC drive paths.
 *
 * @param aFile The file whose leaf name is to be returned.
 * @param aResult The string to hold the resulting leaf name.
 * @param aParent The pre-computed parent of |aFile|. If not provided, it will
 *                be computed.
 */
static nsresult GetLeafNamePreservingRoot(nsIFile* aFile, nsString& aResult,
                                          nsIFile* aParent = nullptr) {
  MOZ_ASSERT(aFile);

  nsCOMPtr<nsIFile> parent = aParent;
  if (!parent) {
    MOZ_TRY(aFile->GetParent(getter_AddRefs(parent)));
  }

  if (parent) {
    return aFile->GetLeafName(aResult);
  }

  // We have reached the root path. On Windows, the leafname for a UNC path
  // will not have the leading backslashes, so we need to use the entire path
  // here:
  //
  // * for a UNIX root path (/) this will be /;
  // * for a Windows drive path (e.g., C:), this will be the drive path (C:);
  //   and
  // * for a Windows UNC server path (e.g., \\\\server), this will be the full
  //   server path (\\\\server).
  return aFile->GetPath(aResult);
}

void PathUtils::Filename(const GlobalObject&, const nsAString& aPath,
                         nsString& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError("PathUtils does not support empty paths");
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  if (nsresult rv = GetLeafNamePreservingRoot(path, aResult); NS_FAILED(rv)) {
    ThrowError(aErr, rv, "Could not get leaf name of path"_ns);
    return;
  }
}

void PathUtils::Parent(const GlobalObject&, const nsAString& aPath,
                       nsString& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  nsCOMPtr<nsIFile> parent;
  if (nsresult rv = path->GetParent(getter_AddRefs(parent)); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_GET_PARENT);
    return;
  }

  if (parent) {
    MOZ_ALWAYS_SUCCEEDS(parent->GetPath(aResult));
  } else {
    aResult = VoidString();
  }
}

void PathUtils::Join(const GlobalObject&, const Sequence<nsString>& aComponents,
                     nsString& aResult, ErrorResult& aErr) {
  if (aComponents.IsEmpty()) {
    return;
  }
  if (aComponents[0].IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aComponents[0]); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  const auto components = Span<const nsString>(aComponents).Subspan(1);
  for (const auto& component : components) {
    if (nsresult rv = path->Append(component); NS_FAILED(rv)) {
      ThrowError(aErr, rv, "Could not append to path"_ns);
      return;
    }
  }

  MOZ_ALWAYS_SUCCEEDS(path->GetPath(aResult));
}

void PathUtils::Normalize(const GlobalObject&, const nsAString& aPath,
                          nsString& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  if (nsresult rv = path->Normalize(); NS_FAILED(rv)) {
    ThrowError(aErr, rv, "Could not normalize path"_ns);
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(path->GetPath(aResult));
}

void PathUtils::Split(const GlobalObject&, const nsAString& aPath,
                      nsTArray<nsString>& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  while (path) {
    auto* component = aResult.EmplaceBack(fallible);
    if (!component) {
      aErr.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    nsCOMPtr<nsIFile> parent;
    if (nsresult rv = path->GetParent(getter_AddRefs(parent)); NS_FAILED(rv)) {
      ThrowError(aErr, rv, ERROR_GET_PARENT);
      return;
    }

    // GetLeafPreservingRoot cannot fail if we pass it a parent path.
    MOZ_ALWAYS_SUCCEEDS(GetLeafNamePreservingRoot(path, *component, parent));

    path = parent;
  }

  aResult.Reverse();
}

void PathUtils::ToFileURI(const GlobalObject&, const nsAString& aPath,
                          nsCString& aResult, ErrorResult& aErr) {
  if (aPath.IsEmpty()) {
    aErr.ThrowNotAllowedError(ERROR_EMPTY_PATH);
    return;
  }

  nsCOMPtr<nsIFile> path = new nsLocalFile();
  if (nsresult rv = path->InitWithPath(aPath); NS_FAILED(rv)) {
    ThrowError(aErr, rv, ERROR_INITIALIZE_PATH);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  if (nsresult rv = NS_NewFileURI(getter_AddRefs(uri), path); NS_FAILED(rv)) {
    ThrowError(aErr, rv, "Could not initialize File URI"_ns);
    return;
  }

  if (nsresult rv = uri->GetSpec(aResult); NS_FAILED(rv)) {
    ThrowError(aErr, rv, "Could not retrieve URI spec"_ns);
    return;
  }
}

}  // namespace dom
}  // namespace mozilla
