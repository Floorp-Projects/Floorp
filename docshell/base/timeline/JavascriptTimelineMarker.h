/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_JavascriptTimelineMarker_h_
#define mozilla_JavascriptTimelineMarker_h_

#include "TimelineMarker.h"

#include "mozilla/Maybe.h"

#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ToJSValue.h"

namespace mozilla {

class JavascriptTimelineMarker : public TimelineMarker
{
public:
  // The caller owns |aAsyncCause| here, so we must copy it into a separate
  // string for use later on.
  JavascriptTimelineMarker(const char* aReason,
                           const char16_t* aFunctionName,
                           const char16_t* aFileName,
                           uint32_t aLineNumber,
                           MarkerTracingType aTracingType,
                           JS::Handle<JS::Value> aAsyncStack,
                           const char* aAsyncCause)
    : TimelineMarker("Javascript", aTracingType, MarkerStackRequest::NO_STACK)
    , mCause(NS_ConvertUTF8toUTF16(aReason))
    , mFunctionName(aFunctionName)
    , mFileName(aFileName)
    , mLineNumber(aLineNumber)
    , mAsyncCause(aAsyncCause)
  {
    JSContext* ctx = nsContentUtils::GetCurrentJSContext();
    if (ctx) {
      mAsyncStack.init(ctx, aAsyncStack);
    }
  }

  virtual void AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker) override
  {
    TimelineMarker::AddDetails(aCx, aMarker);

    aMarker.mCauseName.Construct(mCause);

    if (!mFunctionName.IsEmpty() || !mFileName.IsEmpty()) {
      dom::RootedDictionary<dom::ProfileTimelineStackFrame> stackFrame(aCx);
      stackFrame.mLine.Construct(mLineNumber);
      stackFrame.mSource.Construct(mFileName);
      stackFrame.mFunctionDisplayName.Construct(mFunctionName);

      if (mAsyncStack.isObject() && !mAsyncCause.IsEmpty()) {
        JS::Rooted<JSObject*> asyncStack(aCx, &mAsyncStack.toObject());
        JS::Rooted<JSObject*> parentFrame(aCx);
        JS::Rooted<JSString*> asyncCause(aCx, JS_NewUCStringCopyN(aCx, mAsyncCause.BeginReading(),
                                                                  mAsyncCause.Length()));
        if (!asyncCause) {
          JS_ClearPendingException(aCx);
          return;
        }

        if (JS::IsMaybeWrappedSavedFrame(asyncStack) &&
            !JS::CopyAsyncStack(aCx, asyncStack, asyncCause, &parentFrame, mozilla::Nothing())) {
          JS_ClearPendingException(aCx);
        } else {
          stackFrame.mAsyncParent = parentFrame;
        }
      }

      JS::Rooted<JS::Value> newStack(aCx);
      if (ToJSValue(aCx, stackFrame, &newStack)) {
        if (newStack.isObject()) {
          aMarker.mStack = &newStack.toObject();
        }
      } else {
        JS_ClearPendingException(aCx);
      }
    }
  }

private:
  nsString mCause;
  nsString mFunctionName;
  nsString mFileName;
  uint32_t mLineNumber;
  JS::PersistentRooted<JS::Value> mAsyncStack;
  NS_ConvertUTF8toUTF16 mAsyncCause;
};

} // namespace mozilla

#endif // mozilla_JavascriptTimelineMarker_h_
