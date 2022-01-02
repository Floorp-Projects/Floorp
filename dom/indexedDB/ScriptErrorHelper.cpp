/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptErrorHelper.h"

#include "MainThreadUtils.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#include "mozilla/SchedulerGroup.h"

namespace {

class ScriptErrorRunnable final : public mozilla::Runnable {
  nsString mMessage;
  nsCString mMessageName;
  nsString mFilename;
  uint32_t mLineNumber;
  uint32_t mColumnNumber;
  uint32_t mSeverityFlag;
  uint64_t mInnerWindowID;
  bool mIsChrome;

 public:
  ScriptErrorRunnable(const nsAString& aMessage, const nsAString& aFilename,
                      uint32_t aLineNumber, uint32_t aColumnNumber,
                      uint32_t aSeverityFlag, bool aIsChrome,
                      uint64_t aInnerWindowID)
      : mozilla::Runnable("ScriptErrorRunnable"),
        mMessage(aMessage),
        mFilename(aFilename),
        mLineNumber(aLineNumber),
        mColumnNumber(aColumnNumber),
        mSeverityFlag(aSeverityFlag),
        mInnerWindowID(aInnerWindowID),
        mIsChrome(aIsChrome) {
    MOZ_ASSERT(!NS_IsMainThread());
    mMessageName.SetIsVoid(true);
  }

  ScriptErrorRunnable(const nsACString& aMessageName,
                      const nsAString& aFilename, uint32_t aLineNumber,
                      uint32_t aColumnNumber, uint32_t aSeverityFlag,
                      bool aIsChrome, uint64_t aInnerWindowID)
      : mozilla::Runnable("ScriptErrorRunnable"),
        mMessageName(aMessageName),
        mFilename(aFilename),
        mLineNumber(aLineNumber),
        mColumnNumber(aColumnNumber),
        mSeverityFlag(aSeverityFlag),
        mInnerWindowID(aInnerWindowID),
        mIsChrome(aIsChrome) {
    MOZ_ASSERT(!NS_IsMainThread());
    mMessage.SetIsVoid(true);
  }

  static void DumpLocalizedMessage(const nsACString& aMessageName,
                                   const nsAString& aFilename,
                                   uint32_t aLineNumber, uint32_t aColumnNumber,
                                   uint32_t aSeverityFlag, bool aIsChrome,
                                   uint64_t aInnerWindowID) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!aMessageName.IsEmpty());

    nsAutoString localizedMessage;
    if (NS_WARN_IF(NS_FAILED(nsContentUtils::GetLocalizedString(
            nsContentUtils::eDOM_PROPERTIES, aMessageName.BeginReading(),
            localizedMessage)))) {
      return;
    }

    Dump(localizedMessage, aFilename, aLineNumber, aColumnNumber, aSeverityFlag,
         aIsChrome, aInnerWindowID);
  }

  static void Dump(const nsAString& aMessage, const nsAString& aFilename,
                   uint32_t aLineNumber, uint32_t aColumnNumber,
                   uint32_t aSeverityFlag, bool aIsChrome,
                   uint64_t aInnerWindowID) {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoCString category;
    if (aIsChrome) {
      category.AssignLiteral("chrome ");
    } else {
      category.AssignLiteral("content ");
    }
    category.AppendLiteral("javascript");

    nsCOMPtr<nsIConsoleService> consoleService =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);

    nsCOMPtr<nsIScriptError> scriptError =
        do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
    // We may not be able to create the script error object when we're shutting
    // down.
    if (!scriptError) {
      return;
    }

    if (aInnerWindowID) {
      MOZ_ALWAYS_SUCCEEDS(scriptError->InitWithWindowID(
          aMessage, aFilename,
          /* aSourceLine */ u""_ns, aLineNumber, aColumnNumber, aSeverityFlag,
          category, aInnerWindowID));
    } else {
      MOZ_ALWAYS_SUCCEEDS(scriptError->Init(
          aMessage, aFilename,
          /* aSourceLine */ u""_ns, aLineNumber, aColumnNumber, aSeverityFlag,
          category.get(),
          /* IDB doesn't run on Private browsing mode */ false,
          /* from chrome context */ aIsChrome));
    }

    // We may not be able to obtain the console service when we're shutting
    // down.
    if (consoleService) {
      MOZ_ALWAYS_SUCCEEDS(consoleService->LogMessage(scriptError));
    }
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mMessage.IsVoid() != mMessageName.IsVoid());

    if (!mMessage.IsVoid()) {
      Dump(mMessage, mFilename, mLineNumber, mColumnNumber, mSeverityFlag,
           mIsChrome, mInnerWindowID);
      return NS_OK;
    }

    DumpLocalizedMessage(mMessageName, mFilename, mLineNumber, mColumnNumber,
                         mSeverityFlag, mIsChrome, mInnerWindowID);

    return NS_OK;
  }

 private:
  virtual ~ScriptErrorRunnable() = default;
};

}  // namespace

namespace mozilla::dom::indexedDB {

/*static*/
void ScriptErrorHelper::Dump(const nsAString& aMessage,
                             const nsAString& aFilename, uint32_t aLineNumber,
                             uint32_t aColumnNumber, uint32_t aSeverityFlag,
                             bool aIsChrome, uint64_t aInnerWindowID) {
  if (NS_IsMainThread()) {
    ScriptErrorRunnable::Dump(aMessage, aFilename, aLineNumber, aColumnNumber,
                              aSeverityFlag, aIsChrome, aInnerWindowID);
  } else {
    RefPtr<ScriptErrorRunnable> runnable =
        new ScriptErrorRunnable(aMessage, aFilename, aLineNumber, aColumnNumber,
                                aSeverityFlag, aIsChrome, aInnerWindowID);
    MOZ_ALWAYS_SUCCEEDS(
        SchedulerGroup::Dispatch(TaskCategory::Other, runnable.forget()));
  }
}

/*static*/
void ScriptErrorHelper::DumpLocalizedMessage(
    const nsACString& aMessageName, const nsAString& aFilename,
    uint32_t aLineNumber, uint32_t aColumnNumber, uint32_t aSeverityFlag,
    bool aIsChrome, uint64_t aInnerWindowID) {
  if (NS_IsMainThread()) {
    ScriptErrorRunnable::DumpLocalizedMessage(
        aMessageName, aFilename, aLineNumber, aColumnNumber, aSeverityFlag,
        aIsChrome, aInnerWindowID);
  } else {
    RefPtr<ScriptErrorRunnable> runnable = new ScriptErrorRunnable(
        aMessageName, aFilename, aLineNumber, aColumnNumber, aSeverityFlag,
        aIsChrome, aInnerWindowID);
    MOZ_ALWAYS_SUCCEEDS(
        SchedulerGroup::Dispatch(TaskCategory::Other, runnable.forget()));
  }
}

}  // namespace mozilla::dom::indexedDB
