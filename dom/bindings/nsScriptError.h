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

class nsScriptErrorNote final : public nsIScriptErrorNote {
 public:
  nsScriptErrorNote();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISCRIPTERRORNOTE

  void Init(const nsAString& message, const nsAString& sourceName,
            uint32_t lineNumber, uint32_t columnNumber);

 private:
  virtual ~nsScriptErrorNote();

  nsString mMessage;
  nsString mSourceName;
  nsString mSourceLine;
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

protected:
  virtual ~nsScriptErrorBase();

  void
  InitializeOnMainThread();

  void InitializationHelper(const nsAString& message,
                            const nsAString& sourceLine, uint32_t lineNumber,
                            uint32_t columnNumber, uint32_t flags,
                            const nsACString& category,
                            uint64_t aInnerWindowID);

  nsCOMArray<nsIScriptErrorNote> mNotes;
  nsString mMessage;
  nsString mMessageName;
  nsString mSourceName;
  uint32_t mLineNumber;
  nsString mSourceLine;
  uint32_t mColumnNumber;
  uint32_t mFlags;
  nsCString mCategory;
  // mOuterWindowID is set on the main thread from InitializeOnMainThread().
  uint64_t mOuterWindowID;
  uint64_t mInnerWindowID;
  int64_t mTimeStamp;
  // mInitializedOnMainThread and mIsFromPrivateWindow are set on the main
  // thread from InitializeOnMainThread().
  mozilla::Atomic<bool> mInitializedOnMainThread;
  bool mIsFromPrivateWindow;
};

class nsScriptError final : public nsScriptErrorBase {
public:
  nsScriptError() {}
  NS_DECL_THREADSAFE_ISUPPORTS

private:
  virtual ~nsScriptError() {}
};

class nsScriptErrorWithStack : public nsScriptErrorBase {
public:
  nsScriptErrorWithStack(JS::HandleObject aStack, JS::HandleObject aStackGlobal);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsScriptErrorWithStack)

  NS_IMETHOD GetStack(JS::MutableHandleValue) override;
  NS_IMETHOD ToString(nsACString& aResult) override;

private:
  virtual ~nsScriptErrorWithStack();
  // Complete stackframe where the error happened.
  // Must be a (possibly wrapped) SavedFrame object.
  JS::Heap<JSObject*> mStack;
  // Global object that must be same-compartment with mStack.
  JS::Heap<JSObject*> mStackGlobal;
};

#endif /* mozilla_dom_nsScriptError_h */
