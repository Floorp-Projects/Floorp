/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemPermissionRequest_h
#define mozilla_dom_FileSystemPermissionRequest_h

#include "nsIRunnable.h"
#include "nsIContentPermissionPrompt.h"
#include "nsString.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class FileSystemTaskChildBase;

class FileSystemPermissionRequest final
  : public nsIContentPermissionRequest
  , public nsIRunnable
{
public:
  // Request permission for the given task.
  static void
  RequestForTask(FileSystemTaskChildBase* aTask);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_NSIRUNNABLE

private:
  explicit FileSystemPermissionRequest(FileSystemTaskChildBase* aTask);

  ~FileSystemPermissionRequest();

  // Once the permission check has been done, we must run the task using IPC and
  // PBackground. This method checks if the PBackground thread is ready to
  // receive the task and in case waits for ActorCreated() to be called using
  // the PBackgroundInitializer class (see FileSystemPermissionRequest.cpp).
  void
  ScheduleTask();

  nsCString mPermissionType;
  nsCString mPermissionAccess;
  RefPtr<FileSystemTaskChildBase> mTask;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemPermissionRequest_h
