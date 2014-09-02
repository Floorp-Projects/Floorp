/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemTaskBase_h
#define mozilla_dom_FileSystemTaskBase_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemRequestParent.h"
#include "mozilla/dom/PFileSystemRequestChild.h"
#include "mozilla/dom/ipc/Blob.h"

class nsIDOMFile;

namespace mozilla {
namespace dom {

class FileSystemBase;
class FileSystemParams;
class Promise;

/*
 * The base class to implement a Task class.
 * The task is used to handle the OOP (out of process) operations.
 * The file system operations can only be performed in the parent process. When
 * performing such a parent-process-only operation, a task will delivered the
 * operation to the parent process if needed.
 *
 * The following diagram illustrates the how a API call from the content page
 * starts a task and gets call back results.
 *
 * The left block is the call sequence inside the child process, while the
 * right block is the call sequence inside the parent process.
 *
 * There are two types of API call. One is from the content page of the child
 * process and we mark the steps as (1) to (8). The other is from the content
 * page of the parent process and we mark the steps as (1') to (4').
 *
 *       Page                                             Page
 *        |                                                |
 *        | (1)                                            |  (1')
 *  ______|________________     |     _____________________|_____________
 * |      |                |    |    |                     |             |
 * |      |  Task in       |    |    |  Task in            |             |
 * |      |  Child Process |    |    |  Parent Process     |             |
 * |      V                |   IPC   |                     V             |
 * [new FileSystemTaskBase()]   |    |     [new FileSystemTaskBase()]    |
 * |         |             |    |    |                         |         |
 * |         | (2)         |         |                         | (2')    |
 * |         V             |   (3)   |                         |         |
 * |    [GetRequestParams]------------->[new FileSystemTaskBase(...)]    |
 * |                       |         |          |              |         |
 * |                       |    |    |          | (4)          |         |
 * |                       |    |    |          |              V         |
 * |                       |    |    |          -----------> [Work]      |
 * |                       |   IPC   |                         |         |
 * |                       |    |    |                     (5) | (3')    |
 * |                       |    |    |                         V         |
 * |                       |    |    |          --------[HandleResult]   |
 * |                       |    |    |          |              |         |
 * |                       |         |          | (6)          |         |
 * |                       |   (7)   |          V              |         |
 * |   [SetRequestResult]<-------------[GetRequestResult]      |         |
 * |       |               |         |                         | (4')    |
 * |       | (8)           |    |    |                         |         |
 * |       V               |    |    |                         V         |
 * |[HandlerCallback]      |   IPC   |               [HandlerCallback]   |
 * |_______|_______________|    |    |_________________________|_________|
 *         |                    |                              |
 *         V                                                   V
 *        Page                                                Page
 *
 * 1. From child process page
 * Child:
 *   (1) Call FileSystem API from content page with JS. Create a task and run.
 *   The base constructor [FileSystemTaskBase()] of the task should be called.
 *   (2) Forward the task to the parent process through the IPC and call
 *   [GetRequestParams] to prepare the parameters of the IPC.
 * Parent:
 *   (3) The parent process receives IPC and handle it in
 *   FileystemRequestParent.
 *   Get the IPC parameters and create a task to run the IPC task. The base
 *   constructor [FileSystemTaskBase(aParam, aParent)] of the task should be
 *   called to set the task as an IPC task.
 *   (4) The task operation will be performed in the member function of [Work].
 *   A worker thread will be created to run that function. If error occurs
 *   during the operation, call [SetError] to record the error and then abort.
 *   (5) After finishing the task operation, call [HandleResult] to send the
 *   result back to the child process though the IPC.
 *   (6) Call [GetRequestResult] request result to prepare the parameters of the
 *   IPC. Because the formats of the error result for different task are the
 *   same, FileSystemTaskBase can handle the error message without interfering.
 *   Each task only needs to implement its specific success result preparation
 *   function -[GetSuccessRequestResult].
 * Child:
 *   (7) The child process receives IPC and calls [SetRequestResult] to get the
 *   task result. Each task needs to implement its specific success result
 *   parsing function [SetSuccessRequestResult] to get the success result.
 *   (8) Call [HandlerCallback] to send the task result to the content page.
 * 2. From parent process page
 * We don't need to send the task parameters and result to other process. So
 * there are less steps, but their functions are the same. The correspondence
 * between the two types of steps is:
 *   (1') = (1),
 *   (2') = (4),
 *   (3') = (5),
 *   (4') = (8).
 */
class FileSystemTaskBase
  : public nsRunnable
  , public PFileSystemRequestChild
{
public:
  /*
   * Start the task. If the task is running the child process, it will be
   * forwarded to parent process by IPC, or else, creates a worker thread to
   * do the task work.
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

  NS_DECL_NSIRUNNABLE
protected:
  /*
   * To create a task to handle the page content request.
   */
  explicit FileSystemTaskBase(FileSystemBase* aFileSystem);

  /*
   * To create a parent process task delivered from the child process through
   * IPC.
   */
  FileSystemTaskBase(FileSystemBase* aFileSystem,
                     const FileSystemParams& aParam,
                     FileSystemRequestParent* aParent);

  virtual
  ~FileSystemTaskBase();

  /*
   * The function to perform task operation. It will be run on the worker
   * thread of the parent process.
   * Overrides this function to define the task operation for individual task.
   */
  virtual nsresult
  Work() = 0;

  /*
   * After the task is completed, this function will be called to pass the task
   * result to the content page.
   * Override this function to handle the call back to the content page.
   */
  virtual void
  HandlerCallback() = 0;

  /*
   * Wrap the task parameter to FileSystemParams for sending it through IPC.
   * It will be called when we need to forward a task from the child process to
   * the prarent process.
   * @param filesystem The string representation of the file system.
   */
  virtual FileSystemParams
  GetRequestParams(const nsString& aFileSystem) const = 0;

  /*
   * Wrap the task success result to FileSystemResponseValue for sending it
   * through IPC.
   * It will be called when the task is completed successfully and we need to
   * send the task success result back to the child process.
   */
  virtual FileSystemResponseValue
  GetSuccessRequestResult() const = 0;

  /*
   * Unwrap the IPC message to get the task success result.
   * It will be called when the task is completed successfully and an IPC
   * message is received in the child process and we want to get the task
   * success result.
   */
  virtual void
  SetSuccessRequestResult(const FileSystemResponseValue& aValue) = 0;

  bool
  HasError() const { return mErrorValue != NS_OK; }

  // Overrides PFileSystemRequestChild
  virtual bool
  Recv__delete__(const FileSystemResponseValue& value) MOZ_OVERRIDE;

  BlobParent*
  GetBlobParent(nsIDOMFile* aFile) const;

  nsresult mErrorValue;

  nsRefPtr<FileSystemBase> mFileSystem;
  nsRefPtr<FileSystemRequestParent> mRequestParent;
private:
  /*
   * After finishing the task operation, handle the task result.
   * If it is an IPC task, send back the IPC result. Or else, send the result
   * to the content page.
   */
  void
  HandleResult();

  /*
   * Wrap the task result to FileSystemResponseValue for sending it through IPC.
   * It will be called when the task is completed and we need to
   * send the task result back to the child process.
   */
  FileSystemResponseValue
  GetRequestResult() const;

  /*
   * Unwrap the IPC message to get the task result.
   * It will be called when the task is completed and an IPC message is received
   * in the child process and we want to get the task result.
   */
  void
  SetRequestResult(const FileSystemResponseValue& aValue);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemTaskBase_h
