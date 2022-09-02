/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsScriptError_h
#define mozilla_dom_nsScriptError_h

#include "mozilla/Atomics.h"

#include <stdint.h>

#include "jsapi.h"
#include "js/RootingAPI.h"

#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIScriptError.h"
#include "nsString.h"

class nsGlobalWindowInner;

class nsScriptErrorNote final : public nsIScriptErrorNote {
 public:
  nsScriptErrorNote();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISCRIPTERRORNOTE

  void Init(const nsAString& message, const nsAString& sourceName,
            uint32_t sourceId, uint32_t lineNumber, uint32_t columnNumber);

 private:
  virtual ~nsScriptErrorNote();

  nsString mMessage;
  nsString mSourceName;
  nsString mCssSelectors;
  nsString mSourceLine;
  uint32_t mSourceId;
  uint32_t mLineNumber;
  uint32_t mColumnNumber;
};

// Definition of nsScriptError..
class nsScriptErrorBase : public nsIScriptError {
 public:
  nsScriptErrorBase();

  NS_DECL_NSICONSOLEMESSAGE
  NS_DECL_NSISCRIPTERROR

  void AddNote(nsIScriptErrorNote* note);

  static bool ComputeIsFromPrivateWindow(nsGlobalWindowInner* aWindow);

  static bool ComputeIsFromChromeContext(nsGlobalWindowInner* aWindow);

 protected:
  virtual ~nsScriptErrorBase();

  void InitializeOnMainThread();

  void InitializationHelper(const nsAString& message,
                            const nsAString& sourceLine, uint32_t lineNumber,
                            uint32_t columnNumber, uint32_t flags,
                            const nsACString& category, uint64_t aInnerWindowID,
                            bool aFromChromeContext);

  nsCOMArray<nsIScriptErrorNote> mNotes;
  nsString mMessage;
  nsString mMessageName;
  nsString mSourceName;
  nsString mCssSelectors;
  uint32_t mSourceId;
  uint32_t mLineNumber;
  nsString mSourceLine;
  uint32_t mColumnNumber;
  uint32_t mFlags;
  nsCString mCategory;
  // mOuterWindowID is set on the main thread from InitializeOnMainThread().
  uint64_t mOuterWindowID;
  uint64_t mInnerWindowID;
  int64_t mMicroSecondTimeStamp;
  // mInitializedOnMainThread, mIsFromPrivateWindow and mIsFromChromeContext are
  // set on the main thread from InitializeOnMainThread().
  mozilla::Atomic<bool> mInitializedOnMainThread;
  bool mIsFromPrivateWindow;
  bool mIsFromChromeContext;
  bool mIsPromiseRejection;
  bool mIsForwardedFromContentProcess;
};

class nsScriptError final : public nsScriptErrorBase {
 public:
  nsScriptError() = default;
  NS_DECL_THREADSAFE_ISUPPORTS

 private:
  virtual ~nsScriptError() = default;
};

class nsScriptErrorWithStack : public nsScriptErrorBase {
 public:
  nsScriptErrorWithStack(JS::Handle<mozilla::Maybe<JS::Value>> aException,
                         JS::Handle<JSObject*> aStack,
                         JS::Handle<JSObject*> aStackGlobal);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsScriptErrorWithStack)

  NS_IMETHOD GetHasException(bool*) override;
  NS_IMETHOD GetException(JS::MutableHandle<JS::Value>) override;

  NS_IMETHOD GetStack(JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD GetStackGlobal(JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD ToString(nsACString& aResult) override;

 private:
  virtual ~nsScriptErrorWithStack();

  // The "exception" value.
  JS::Heap<JS::Value> mException;
  bool mHasException;

  // Complete stackframe where the error happened.
  // Must be a (possibly wrapped) SavedFrame object.
  JS::Heap<JSObject*> mStack;
  // Global object that must be same-compartment with mStack.
  JS::Heap<JSObject*> mStackGlobal;
};

// Creates either nsScriptErrorWithStack or nsScriptError,
// depending on whether |aStack| or |aException| is passed.
// Additionally when the first (optional) |win| argument is
// provided this function makes sure that the GlobalWindow
// isn't already dying to prevent leaks.
already_AddRefed<nsScriptErrorBase> CreateScriptError(
    nsGlobalWindowInner* win, JS::Handle<mozilla::Maybe<JS::Value>> aException,
    JS::Handle<JSObject*> aStack, JS::Handle<JSObject*> aStackGlobal);

#endif /* mozilla_dom_nsScriptError_h */
