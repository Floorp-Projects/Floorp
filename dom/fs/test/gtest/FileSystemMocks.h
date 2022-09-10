/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_TEST_GTEST_FILESYSTEMMOCKS_H_
#define DOM_FS_TEST_GTEST_FILESYSTEMMOCKS_H_

#include <memory>  // We don't have a mozilla shared pointer for pod types

#include "TestHelpers.h"
#include "fs/FileSystemChildFactory.h"
#include "fs/FileSystemRequestHandler.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "js/Promise.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/FileSystemManagerChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"

namespace testing::internal {

GTEST_API_ ::testing::AssertionResult CmpHelperSTREQ(const char* s1_expression,
                                                     const char* s2_expression,
                                                     const nsAString& s1,
                                                     const nsAString& s2);

}  // namespace testing::internal

namespace mozilla::dom::fs {

inline std::ostream& operator<<(std::ostream& aOut,
                                const FileSystemEntryMetadata& aMetadata) {
  return aOut;
}

namespace test {

nsIGlobalObject* GetGlobal();

nsresult GetAsString(const RefPtr<Promise>& aPromise, nsAString& aString);

mozilla::ipc::PrincipalInfo GetPrincipalInfo();

class MockFileSystemRequestHandler : public FileSystemRequestHandler {
 public:
  MOCK_METHOD(void, GetRootHandle,
              (RefPtr<FileSystemManager> aManager, RefPtr<Promise> aPromise),
              (override));

  MOCK_METHOD(void, GetDirectoryHandle,
              (RefPtr<FileSystemManager> & aManager,
               const FileSystemChildMetadata& aDirectory, bool aCreate,
               RefPtr<Promise> aPromise),
              (override));

  MOCK_METHOD(void, GetFileHandle,
              (RefPtr<FileSystemManager> & aManager,
               const FileSystemChildMetadata& aFile, bool aCreate,
               RefPtr<Promise> aPromise),
              (override));

  MOCK_METHOD(void, GetFile,
              (RefPtr<FileSystemManager> & aManager,
               const FileSystemEntryMetadata& aFile, RefPtr<Promise> aPromise),
              (override));

  MOCK_METHOD(void, GetEntries,
              (RefPtr<FileSystemManager> & aManager, const EntryId& aDirectory,
               PageNumber aPage, RefPtr<Promise> aPromise,
               RefPtr<FileSystemEntryMetadataArray>& aSink),
              (override));

  MOCK_METHOD(void, RemoveEntry,
              (RefPtr<FileSystemManager> & aManager,
               const FileSystemChildMetadata& aEntry, bool aRecursive,
               RefPtr<Promise> aPromise),
              (override));

  MOCK_METHOD(void, MoveEntry,
              (RefPtr<FileSystemManager> & aManager, FileSystemHandle* aHandle,
               const FileSystemEntryMetadata& aEntry,
               const FileSystemChildMetadata& aNewEntry,
               RefPtr<Promise> aPromise),
              (override));

  MOCK_METHOD(void, RenameEntry,
              (RefPtr<FileSystemManager> & aManager, FileSystemHandle* aHandle,
               const FileSystemEntryMetadata& aEntry, const Name& aName,
               RefPtr<Promise> aPromise),
              (override));

  MOCK_METHOD(void, Resolve,
              (RefPtr<FileSystemManager> & aManager,
               const FileSystemEntryPair& aEndpoints, RefPtr<Promise> aPromise),
              (override));
};

class WaitablePromiseListener {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void ClearDone() = 0;

  virtual bool IsDone() const = 0;

  virtual PromiseNativeHandler* AsHandler() = 0;

 protected:
  virtual ~WaitablePromiseListener() = default;
};

template <class SuccessHandler, class ErrorHandler,
          uint32_t MilliSeconds = 2000u>
class TestPromiseListener : public PromiseNativeHandler,
                            public WaitablePromiseListener {
 public:
  TestPromiseListener()
      : mIsDone(std::make_shared<bool>(false)),
        mTimer(),
        mOnSuccess(),
        mOnError() {
    ClearDone();
  }

  // nsISupports implementation

  NS_IMETHODIMP QueryInterface(REFNSIID aIID, void** aInstancePtr) override {
    nsresult rv = NS_ERROR_UNEXPECTED;
    NS_INTERFACE_TABLE0(TestPromiseListener)

    return rv;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestPromiseListener, override)

  // PromiseNativeHandler implementation

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    mozilla::ScopeExit flagAsDone([isDone = mIsDone, timer = mTimer] {
      timer->Cancel();
      *isDone = true;
    });

    mOnSuccess();
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    mozilla::ScopeExit flagAsDone([isDone = mIsDone, timer = mTimer] {
      timer->Cancel();
      *isDone = true;
    });

    if (aValue.isInt32()) {
      mOnError(static_cast<nsresult>(aValue.toInt32()));
      return;
    }

    ASSERT_TRUE(aValue.isObject());
    JS::Rooted<JSObject*> exceptionObject(aCx, &aValue.toObject());

    RefPtr<Exception> exception;
    UNWRAP_OBJECT(Exception, exceptionObject, exception);
    if (exception) {
      mOnError(static_cast<nsresult>(exception->Result()));
      return;
    }
  }

  // WaitablePromiseListener implementation

  void ClearDone() override {
    *mIsDone = false;
    if (mTimer) {
      mTimer->Cancel();
    }
    auto timerCallback = [isDone = mIsDone](nsITimer* aTimer) {
      *isDone = true;
      FAIL() << "Timed out!";
    };
    const char* timerName = "fs::TestPromiseListener::ClearDone";
    auto res = NS_NewTimerWithCallback(timerCallback, MilliSeconds,
                                       nsITimer::TYPE_ONE_SHOT, timerName);
    if (res.isOk()) {
      mTimer = res.unwrap();
    }
  }

  bool IsDone() const override { return *mIsDone; }

  PromiseNativeHandler* AsHandler() override { return this; }

  SuccessHandler& GetSuccessHandler() { return mOnSuccess; }

  SuccessHandler& GetErrorHandler() { return mOnError; }

 protected:
  virtual ~TestPromiseListener() = default;

  std::shared_ptr<bool> mIsDone;  // We pass this to a callback

  nsCOMPtr<nsITimer> mTimer;

  SuccessHandler mOnSuccess;

  ErrorHandler mOnError;
};

class TestFileSystemManagerChild : public FileSystemManagerChild {
 public:
  MOCK_METHOD(void, SendGetRootHandle,
              (mozilla::ipc::ResolveCallback<FileSystemGetHandleResponse> &&
                   aResolve,
               mozilla::ipc::RejectCallback&& aReject),
              (override));

  MOCK_METHOD(
      void, SendGetDirectoryHandle,
      (const FileSystemGetHandleRequest& request,
       mozilla::ipc::ResolveCallback<FileSystemGetHandleResponse>&& aResolve,
       mozilla::ipc::RejectCallback&& aReject),
      (override));

  MOCK_METHOD(
      void, SendGetFileHandle,
      (const FileSystemGetHandleRequest& request,
       mozilla::ipc::ResolveCallback<FileSystemGetHandleResponse>&& aResolve,
       mozilla::ipc::RejectCallback&& aReject),
      (override));

  MOCK_METHOD(
      void, SendGetAccessHandle,
      (const FileSystemGetAccessHandleRequest& request,
       mozilla::ipc::ResolveCallback<FileSystemGetAccessHandleResponse>&&
           aResolve,
       mozilla::ipc::RejectCallback&& aReject),
      (override));

  MOCK_METHOD(
      void, SendGetFile,
      (const FileSystemGetFileRequest& request,
       mozilla::ipc::ResolveCallback<FileSystemGetFileResponse>&& aResolve,
       mozilla::ipc::RejectCallback&& aReject),
      (override));

  MOCK_METHOD(
      void, SendResolve,
      (const FileSystemResolveRequest& request,
       mozilla::ipc::ResolveCallback<FileSystemResolveResponse>&& aResolve,
       mozilla::ipc::RejectCallback&& aReject),
      (override));

  MOCK_METHOD(
      void, SendGetEntries,
      (const FileSystemGetEntriesRequest& request,
       mozilla::ipc::ResolveCallback<FileSystemGetEntriesResponse>&& aResolve,
       mozilla::ipc::RejectCallback&& aReject),
      (override));

  MOCK_METHOD(
      void, SendRemoveEntry,
      (const FileSystemRemoveEntryRequest& request,
       mozilla::ipc::ResolveCallback<FileSystemRemoveEntryResponse>&& aResolve,
       mozilla::ipc::RejectCallback&& aReject),
      (override));

  MOCK_METHOD(void, Shutdown, (), (override));

 protected:
  virtual ~TestFileSystemManagerChild() = default;
};

class TestFileSystemChildFactory final : public FileSystemChildFactory {
 public:
  explicit TestFileSystemChildFactory(TestFileSystemManagerChild* aChild)
      : mChild(aChild) {}

  already_AddRefed<FileSystemManagerChild> Create() const override {
    return RefPtr<TestFileSystemManagerChild>(mChild).forget();
  }

  ~TestFileSystemChildFactory() = default;

 private:
  TestFileSystemManagerChild* mChild;
};

struct MockExpectMe {
  MOCK_METHOD0(InvokeMe, void());

  template <class... Args>
  void operator()(Args...) {
    InvokeMe();
  }
};

template <nsresult Expected>
struct NSErrorMatcher {
  void operator()(nsresult aErr) { ASSERT_NSEQ(Expected, aErr); }
};

struct FailOnCall {
  template <class... Args>
  void operator()(Args...) {
    FAIL();
  }
};

}  // namespace test
}  // namespace mozilla::dom::fs

#define MOCK_PROMISE_LISTENER(name, ...) \
  using name = mozilla::dom::fs::test::TestPromiseListener<__VA_ARGS__>;

MOCK_PROMISE_LISTENER(
    ExpectNotImplemented, mozilla::dom::fs::test::FailOnCall,
    mozilla::dom::fs::test::NSErrorMatcher<NS_ERROR_NOT_IMPLEMENTED>);

MOCK_PROMISE_LISTENER(ExpectResolveCalled, mozilla::dom::fs::test::MockExpectMe,
                      mozilla::dom::fs::test::FailOnCall);

#endif  // DOM_FS_TEST_GTEST_FILESYSTEMMOCKS_H_
