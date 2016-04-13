/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemTaskBase_h
#define mozilla_dom_FileSystemTaskBase_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemRequestParent.h"
#include "mozilla/dom/PFileSystemRequestChild.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

class BlobImpl;
class FileSystemBase;
class FileSystemParams;
class PBlobParent;

#define DIRECTORY_READ_PERMISSION "read"
#define DIRECTORY_WRITE_PERMISSION "write"
#define DIRECTORY_CREATE_PERMISSION "create"

/*
 * The base class to implement a Task class.
 * The file system operations can only be performed in the parent process. In
 * order to avoid duplicated code, we used PBackground for child-parent and
 * parent-parent communications.
 *
 * The following diagram illustrates the how a API call from the content page
 * starts a task and gets call back results.
 *
 * The left block is the call sequence inside any process loading content, while
 * the right block is the call sequence only inside the parent process.
 *
 *       Page
 *        |
 *        | (1)
 *  ______|_______________________     |     __________________________________
 * |      |                       |    |    |                                  |
 * |      |                          |   |   |                                 |
 * |      V                          |  IPC  | PBackground thread on           |
 * | [new FileSystemTaskChildBase()] |   |   | the parent process              |
 * |       |                         |   |   |                                 |
 * |       | (2)                     |   |   |                                 |
 * |       V                         |   |   |                                 |
 * | [FileSystemPermissionRequest------------------\                           |
 * |  ::RequestForTask()] <------------------------/                           |
 * |         |                      |    |    |                                |
 * |         | (3)                  |         |                                |
 * |         V                      |   (4)   |                                |
 * |    [GetRequestParams]------------------->[new FileSystemTaskParentBase()] |
 * |                                 |       |          |                      |
 * |                                 |   |   |          | (5)   _____________  |
 * |                                 |   |   |          |      |             | |
 * |                                 |   |   |          |      | I/O Thread  | |
 * |                                 |   |   |          |      |             | |
 * |                                 |   |   |          ---------> [IOWork]  | |
 * |                                 |  IPC  |                 |     |       | |
 * |                                 |   |   |                 |     | (6)   | |
 * |                                 |   |   |          --------------       | |
 * |                                 |   |   |          |      |_____________| |
 * |                                 |   |   |          |                      |
 * |                                 |   |   |          V                      |
 * |                                 |   |   |     [HandleResult]              |
 * |                                 |   |   |          |                      |
 * |                                 |       |          | (7)                  |
 * |                                 |  (8)  |          V                      |
 * |   [SetRequestResult]<---------------------[GetRequestResult]              |
 * |       |                         |       |                                 |
 * |       | (9)                     |   |   |                                 |
 * |       V                         |   |   |                                 |
 * |[HandlerCallback]                |  IPC  |                                 |
 * |_______|_________________________|   |   |_________________________________|
 *         |                             |
 *         V
 *        Page
 *
 * 1. From the process that is handling the request
 * Child/Parent (it can be in any process):
 *   (1) Call FileSystem API from content page with JS. Create a task and run.
 *   The base constructor [FileSystemTaskChildBase()] of the task should be
 *   called.
 *   (2) The FileSystemTaskChildBase object is given to
 *   [FileSystemPermissionRequest::RequestForTask()] that will perform a
 *   permission check step if needed (See ePermissionCheckType enum). The real
 *   operation is done on the parent process but it's hidden by
 *   [nsContentPermissionUtils::AskPermission()]. If the permission check is not
 *   needed or if the page has the right permission, the
 *   FileSystemPermissionRequest will start the task (only once PBackground
 *   actor is fully initialized).
 *   (3) Forward the task to the parent process through the IPC and call
 *   [GetRequestParams] to prepare the parameters of the IPC.
 * Parent:
 *   (4) The parent process receives IPC and handle it in
 *   FileystemRequestParent. Get the IPC parameters and create a task to run the
 *   IPC task. The base constructor [FileSystemTaskParentBase(aParam, aParent)]
 *   For security reasons, we do an additional permission check if needed. In
 *   the check fails, the child process will be killed.
 *   of the task should be called to set the task as an IPC task.
 *   (5) The task operation will be performed in the member function of [IOWork].
 *   A I/O  thread will be created to run that function. If error occurs
 *   during the operation, call [SetError] to record the error and then abort.
 *   (6) After finishing the task operation, call [HandleResult] to send the
 *   result back to the child process though the IPC.
 *   (7) Call [GetRequestResult] request result to prepare the parameters of the
 *   IPC. Because the formats of the error result for different task are the
 *   same, FileSystemTaskChildBase can handle the error message without
 *   interfering.
 *   Each task only needs to implement its specific success result preparation
 *   function -[GetSuccessRequestResult].
 * Child/Parent:
 *   (8) The process receives IPC and calls [SetRequestResult] to get the
 *   task result. Each task needs to implement its specific success result
 *   parsing function [SetSuccessRequestResult] to get the success result.
 *   (9) Call [HandlerCallback] to send the task result to the content page.
 */
class FileSystemTaskChildBase : public PFileSystemRequestChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(FileSystemTaskChildBase)

  /*
   * Start the task. It will dispatch all the information to the parent process,
   * PBackground thread. This method must be called from the owning thread.
   */
  void
  Start();

  /*
   * The error codes are defined in xpcom/base/ErrorList.h and their
   * corresponding error name and message are defined in dom/base/domerr.msg.
   */
  void
  SetError(const nsresult& aErrorCode);

  FileSystemBase*
  GetFileSystem() const;

  /*
   * Get the type of permission access required to perform this task.
   */
  virtual void
  GetPermissionAccessType(nsCString& aAccess) const = 0;

  /*
   * After the task is completed, this function will be called to pass the task
   * result to the content page. This method is called in the owning thread.
   * Override this function to handle the call back to the content page.
   */
  virtual void
  HandlerCallback() = 0;

  bool
  HasError() const { return NS_FAILED(mErrorValue); }

protected:
  /*
   * To create a task to handle the page content request.
   */
  explicit FileSystemTaskChildBase(FileSystemBase* aFileSystem);

  virtual
  ~FileSystemTaskChildBase();

  /*
   * Wrap the task parameter to FileSystemParams for sending it through IPC.
   * It will be called when we need to forward a task from the child process to
   * the parent process. This method runs in the owning thread.
   * @param filesystem The string representation of the file system.
   */
  virtual FileSystemParams
  GetRequestParams(const nsString& aSerializedDOMPath,
                   ErrorResult& aRv) const = 0;

  /*
   * Unwrap the IPC message to get the task success result.
   * It will be called when the task is completed successfully and an IPC
   * message is received in the child process and we want to get the task
   * success result. This method runs in the owning thread.
   */
  virtual void
  SetSuccessRequestResult(const FileSystemResponseValue& aValue,
                          ErrorResult& aRv) = 0;

  // Overrides PFileSystemRequestChild
  virtual bool
  Recv__delete__(const FileSystemResponseValue& value) override;

  nsresult mErrorValue;
  RefPtr<FileSystemBase> mFileSystem;

private:

  /*
   * Unwrap the IPC message to get the task result.
   * It will be called when the task is completed and an IPC message is received
   * in the content process and we want to get the task result. This runs on the
   * owning thread.
   */
  void
  SetRequestResult(const FileSystemResponseValue& aValue);
};

// This class is the 'alter ego' of FileSystemTaskChildBase in the PBackground
// world.
class FileSystemTaskParentBase : public nsRunnable
{
public:
  /*
   * Start the task. This must be called from the PBackground thread only.
   */
  void
  Start();

  /*
   * The error codes are defined in xpcom/base/ErrorList.h and their
   * corresponding error name and message are defined in dom/base/domerr.msg.
   */
  void
  SetError(const nsresult& aErrorCode);

  /*
   * The function to perform task operation. It will be run on the I/O
   * thread of the parent process.
   * Overrides this function to define the task operation for individual task.
   */
  virtual nsresult
  IOWork() = 0;

  /*
   * Wrap the task success result to FileSystemResponseValue for sending it
   * through IPC. This method runs in the PBackground thread.
   * It will be called when the task is completed successfully and we need to
   * send the task success result back to the child process.
   */
  virtual FileSystemResponseValue
  GetSuccessRequestResult(ErrorResult& aRv) const = 0;

  /*
   * After finishing the task operation, handle the task result.
   * If it is an IPC task, send back the IPC result. It runs on the PBackground
   * thread.
   */
  void
  HandleResult();

  // If this task must do something on the main-thread before IOWork(), it must
  // overwrite this method. Otherwise it returns true if the FileSystem must be
  // initialized on the main-thread. It's called from the Background thread.
  virtual bool
  NeedToGoToMainThread() const;

  // This method is called only if NeedToGoToMainThread() returns true.
  // Of course, it runs on the main-thread.
  virtual nsresult
  MainThreadWork();

  /*
   * Get the type of permission access required to perform this task.
   */
  virtual void
  GetPermissionAccessType(nsCString& aAccess) const = 0;

  bool
  HasError() const { return NS_FAILED(mErrorValue); }

  NS_IMETHOD
  Run() override;

private:
  /*
   * Wrap the task result to FileSystemResponseValue for sending it through IPC.
   * It will be called when the task is completed and we need to
   * send the task result back to the content. This runs on the PBackground
   * thread.
   */
  FileSystemResponseValue
  GetRequestResult() const;

protected:
  /*
   * To create a parent process task delivered from the child process through
   * IPC.
   */
  FileSystemTaskParentBase(FileSystemBase* aFileSystem,
                           const FileSystemParams& aParam,
                           FileSystemRequestParent* aParent);

  virtual
  ~FileSystemTaskParentBase();

  nsresult mErrorValue;
  RefPtr<FileSystemBase> mFileSystem;
  RefPtr<FileSystemRequestParent> mRequestParent;
  nsCOMPtr<nsIEventTarget> mBackgroundEventTarget;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemTaskBase_h
