/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "FileSystemPermissionRequest.h"

#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "nsIDocument.h"
#include "nsPIDOMWindow.h"
#include "nsContentPermissionHelper.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(FileSystemPermissionRequest, nsIRunnable, nsIContentPermissionRequest)

// static
void
FileSystemPermissionRequest::RequestForTask(FileSystemTaskBase* aTask)
{
  MOZ_ASSERT(aTask, "aTask should not be null!");
  MOZ_ASSERT(NS_IsMainThread());
  nsRefPtr<FileSystemPermissionRequest> request =
    new FileSystemPermissionRequest(aTask);
  NS_DispatchToCurrentThread(request);
}

FileSystemPermissionRequest::FileSystemPermissionRequest(
  FileSystemTaskBase* aTask)
  : mTask(aTask)
{
  MOZ_ASSERT(mTask, "aTask should not be null!");
  MOZ_ASSERT(NS_IsMainThread());

  mTask->GetPermissionAccessType(mPermissionAccess);

  nsRefPtr<FileSystemBase> filesystem = mTask->GetFileSystem();
  if (!filesystem) {
    return;
  }

  mPermissionType = filesystem->GetPermission();

  mWindow = filesystem->GetWindow();
  if (!mWindow) {
    return;
  }

  nsCOMPtr<nsIDocument> doc = mWindow->GetDoc();
  if (!doc) {
    return;
  }

  mPrincipal = doc->NodePrincipal();
  mRequester = new nsContentPermissionRequester(mWindow);
}

FileSystemPermissionRequest::~FileSystemPermissionRequest()
{
}

NS_IMETHODIMP
FileSystemPermissionRequest::GetTypes(nsIArray** aTypes)
{
  nsTArray<nsString> emptyOptions;
  return nsContentPermissionUtils::CreatePermissionArray(mPermissionType,
                                                         mPermissionAccess,
                                                         emptyOptions,
                                                         aTypes);
}

NS_IMETHODIMP
FileSystemPermissionRequest::GetPrincipal(nsIPrincipal** aRequestingPrincipal)
{
  NS_IF_ADDREF(*aRequestingPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
FileSystemPermissionRequest::GetWindow(nsIDOMWindow** aRequestingWindow)
{
  NS_IF_ADDREF(*aRequestingWindow = mWindow);
  return NS_OK;
}

NS_IMETHODIMP
FileSystemPermissionRequest::GetElement(nsIDOMElement** aRequestingElement)
{
  *aRequestingElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
FileSystemPermissionRequest::Cancel()
{
  MOZ_ASSERT(NS_IsMainThread());
  mTask->SetError(NS_ERROR_DOM_SECURITY_ERR);
  mTask->Start();
  return NS_OK;
}

NS_IMETHODIMP
FileSystemPermissionRequest::Allow(JS::HandleValue aChoices)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChoices.isUndefined());
  mTask->Start();
  return NS_OK;
}

NS_IMETHODIMP
FileSystemPermissionRequest::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<FileSystemBase> filesystem = mTask->GetFileSystem();
  if (!filesystem) {
    Cancel();
    return NS_OK;
  }

  if (!filesystem->RequiresPermissionChecks()) {
    Allow(JS::UndefinedHandleValue);
    return NS_OK;
  }

  if (!mWindow) {
    Cancel();
    return NS_OK;
  }

  nsContentPermissionUtils::AskPermission(this, mWindow);
  return NS_OK;
}

NS_IMETHODIMP
FileSystemPermissionRequest::GetRequester(nsIContentPermissionRequester** aRequester)
{
  NS_ENSURE_ARG_POINTER(aRequester);

  nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
  requester.forget(aRequester);
  return NS_OK;
}

} /* namespace dom */
} /* namespace mozilla */
