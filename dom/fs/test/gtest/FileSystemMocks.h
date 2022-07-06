/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_TEST_GTEST_FILESYSTEMMOCKS_H_
#define DOM_FS_TEST_GTEST_FILESYSTEMMOCKS_H_

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "TestHelpers.h"
#include "fs/FileSystemRequestHandler.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"
#include "nsIGlobalObject.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"

#include "jsapi.h"
#include "js/Promise.h"
#include "js/RootingAPI.h"

#include <memory>  // We don't have a mozilla shared pointer for pod types

namespace mozilla::dom::fs::test {

nsIGlobalObject* GetGlobal();

class MockFileSystemRequestHandler : public FileSystemRequestHandler {
 public:
  MOCK_METHOD1(GetRoot, void(RefPtr<Promise> aPromise));

  MOCK_METHOD4(GetDirectoryHandle,
               void(RefPtr<FileSystemActorHolder>& aActor,
                    const FileSystemChildMetadata& aDirectory, bool aCreate,
                    RefPtr<Promise> aPromise));

  MOCK_METHOD4(GetFileHandle, void(RefPtr<FileSystemActorHolder>& aActor,
                                   const FileSystemChildMetadata& aFile,
                                   bool aCreate, RefPtr<Promise> aPromise));

  MOCK_METHOD3(GetFile, void(RefPtr<FileSystemActorHolder>& aActor,
                             const FileSystemEntryMetadata& aFile,
                             RefPtr<Promise> aPromise));

  MOCK_METHOD5(GetEntries,
               void(RefPtr<FileSystemActorHolder>& aActor,
                    const EntryId& aDirectory, PageNumber aPage,
                    RefPtr<Promise> aPromise, ArrayAppendable& aSink));

  MOCK_METHOD4(RemoveEntry, void(RefPtr<FileSystemActorHolder>& aActor,
                                 const FileSystemChildMetadata& aEntry,
                                 bool aRecursive, RefPtr<Promise> aPromise));
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
  TestPromiseListener() : mIsDone(std::make_shared<bool>(false)), mTimer() {
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
    mozilla::ScopeExit flagAsDone([isDone = mIsDone] { *isDone = true; });

    SuccessHandler{}();
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    mozilla::ScopeExit flagAsDone([isDone = mIsDone] { *isDone = true; });

    if (aValue.isInt32()) {
      ErrorHandler{}(static_cast<nsresult>(aValue.toInt32()));
      return;
    }

    ASSERT_TRUE(aValue.isObject());
    JS::Rooted<JSObject*> exceptionObject(aCx, &aValue.toObject());

    RefPtr<Exception> exception;
    UNWRAP_OBJECT(Exception, exceptionObject, exception);
    if (exception) {
      ErrorHandler{}(static_cast<nsresult>(exception->Result()));
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

 protected:
  virtual ~TestPromiseListener() = default;

  std::shared_ptr<bool> mIsDone;  // We pass this to a callback

  nsCOMPtr<nsITimer> mTimer;
};

template <nsresult Expected>
struct NSErrorMatcher {
  void operator()(nsresult aErr) { ASSERT_NSEQ(Expected, aErr); }
};

struct FailOnCall {
  void operator()() { FAIL(); }
};

}  // namespace mozilla::dom::fs::test

#define MOCK_PROMISE_LISTENER(name, ...) \
  using name = mozilla::dom::fs::test::TestPromiseListener<__VA_ARGS__>;

MOCK_PROMISE_LISTENER(
    ExpectNotImplemented, mozilla::dom::fs::test::FailOnCall,
    mozilla::dom::fs::test::NSErrorMatcher<NS_ERROR_NOT_IMPLEMENTED>);

#endif  // DOM_FS_TEST_GTEST_FILESYSTEMMOCKS_H_
