/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "FileSystemPermissionRequest.h"

#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIDocument.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsPIDOMWindow.h"
#include "nsContentPermissionHelper.h"

namespace mozilla {
namespace dom {

namespace {

// This class takes care of the PBackground initialization and, once this step
// is completed, it starts the task.
class PBackgroundInitializer final : public nsIIPCBackgroundChildCreateCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK

  static void
  ScheduleTask(FileSystemTaskChildBase* aTask)
  {
    MOZ_ASSERT(aTask);
    RefPtr<PBackgroundInitializer> pb = new PBackgroundInitializer(aTask);
  }

private:
  explicit PBackgroundInitializer(FileSystemTaskChildBase* aTask)
    : mTask(aTask)
  {
    MOZ_ASSERT(aTask);

    PBackgroundChild* actor =
      mozilla::ipc::BackgroundChild::GetForCurrentThread();
    if (actor) {
      ActorCreated(actor);
    } else {
      if (NS_WARN_IF(
          !mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread(this))) {
        MOZ_CRASH();
      }
    }
  }

  ~PBackgroundInitializer()
  {}

  RefPtr<FileSystemTaskChildBase> mTask;
};

NS_IMPL_ISUPPORTS(PBackgroundInitializer,
                  nsIIPCBackgroundChildCreateCallback)

void
PBackgroundInitializer::ActorFailed()
{
  MOZ_CRASH("Failed to create a PBackgroundChild actor!");
}

void
PBackgroundInitializer::ActorCreated(mozilla::ipc::PBackgroundChild* aActor)
{
  mTask->Start();
}

// This must be a CancelableRunnable because it can be dispatched to a worker
// thread. But we don't care about the Cancel() because in that case, Run() is
// not called and the task is deleted by the DTOR.
class AsyncStartRunnable final : public CancelableRunnable
{
public:
  explicit AsyncStartRunnable(FileSystemTaskChildBase* aTask)
    : mTask(aTask)
  {
    MOZ_ASSERT(aTask);
  }

  NS_IMETHOD
  Run() override
  {
    PBackgroundInitializer::ScheduleTask(mTask);
    return NS_OK;
  }

private:
  RefPtr<FileSystemTaskChildBase> mTask;
};

} // anonymous namespace

NS_IMPL_ISUPPORTS(FileSystemPermissionRequest, nsIRunnable,
                  nsIContentPermissionRequest)

/* static */ void
FileSystemPermissionRequest::RequestForTask(FileSystemTaskChildBase* aTask)
{
  MOZ_ASSERT(aTask);

  RefPtr<FileSystemBase> filesystem = aTask->GetFileSystem();
  if (!filesystem) {
    return;
  }

  if (filesystem->PermissionCheckType() == FileSystemBase::ePermissionCheckNotRequired) {
    // Let's make the scheduling of this task asynchronous.
    RefPtr<AsyncStartRunnable> runnable = new AsyncStartRunnable(aTask);
    NS_DispatchToCurrentThread(runnable);
    return;
  }

  // We don't need any permission check for the FileSystem API. If we are here
  // it's because we are dealing with a DeviceStorage API that is main-thread
  // only.
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<FileSystemPermissionRequest> request =
    new FileSystemPermissionRequest(aTask);
  NS_DispatchToCurrentThread(request);
}

FileSystemPermissionRequest::FileSystemPermissionRequest(FileSystemTaskChildBase* aTask)
  : mTask(aTask)
{
  MOZ_ASSERT(mTask, "aTask should not be null!");
  MOZ_ASSERT(NS_IsMainThread());

  mTask->GetPermissionAccessType(mPermissionAccess);

  RefPtr<FileSystemBase> filesystem = mTask->GetFileSystem();
  if (!filesystem) {
    return;
  }

  mPermissionType = filesystem->GetPermission();

  mWindow = do_QueryInterface(filesystem->GetParentObject());
  if (NS_WARN_IF(!mWindow)) {
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
FileSystemPermissionRequest::GetWindow(mozIDOMWindow** aRequestingWindow)
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
  ScheduleTask();
  return NS_OK;
}

NS_IMETHODIMP
FileSystemPermissionRequest::Allow(JS::HandleValue aChoices)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChoices.isUndefined());
  ScheduleTask();
  return NS_OK;
}

NS_IMETHODIMP
FileSystemPermissionRequest::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<FileSystemBase> filesystem = mTask->GetFileSystem();
  if (!filesystem) {
    Cancel();
    return NS_OK;
  }

  if (filesystem->PermissionCheckType() == FileSystemBase::ePermissionCheckNotRequired) {
    Allow(JS::UndefinedHandleValue);
    return NS_OK;
  }

  if (filesystem->PermissionCheckType() == FileSystemBase::ePermissionCheckByTestingPref &&
      mozilla::Preferences::GetBool("device.storage.prompt.testing", false)) {
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

void
FileSystemPermissionRequest::ScheduleTask()
{
  PBackgroundInitializer::ScheduleTask(mTask);
}

} /* namespace dom */
} /* namespace mozilla */
