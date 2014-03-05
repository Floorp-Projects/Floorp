/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemPermissionRequest_h
#define mozilla_dom_FileSystemPermissionRequest_h

#include "PCOMContentPermissionRequestChild.h"
#include "nsAutoPtr.h"
#include "nsContentPermissionHelper.h"
#include "nsIRunnable.h"

class nsCString;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class FileSystemTaskBase;

class FileSystemPermissionRequest MOZ_FINAL
  : public nsIContentPermissionRequest
  , public nsIRunnable
  , public PCOMContentPermissionRequestChild
{
public:
  // Request permission for the given task.
  static void
  RequestForTask(FileSystemTaskBase* aTask);

  // Overrides PCOMContentPermissionRequestChild

  virtual void
  IPDLRelease() MOZ_OVERRIDE;

  bool
  Recv__delete__(const bool& aAllow,
    const InfallibleTArray<PermissionChoice>& aChoices) MOZ_OVERRIDE;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_NSIRUNNABLE
private:
  FileSystemPermissionRequest(FileSystemTaskBase* aTask);

  virtual
  ~FileSystemPermissionRequest();

  nsCString mPermissionType;
  nsCString mPermissionAccess;
  nsRefPtr<FileSystemTaskBase> mTask;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemPermissionRequest_h
