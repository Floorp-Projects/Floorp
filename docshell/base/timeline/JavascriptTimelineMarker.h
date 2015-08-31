/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_JavascriptTimelineMarker_h_
#define mozilla_JavascriptTimelineMarker_h_

#include "TimelineMarker.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ToJSValue.h"

namespace mozilla {

class JavascriptTimelineMarker : public TimelineMarker
{
public:
  explicit JavascriptTimelineMarker(const char* aReason,
                                    const char16_t* aFunctionName,
                                    const char16_t* aFileName,
                                    uint32_t aLineNumber,
                                    TracingMetadata aMetaData)
    : TimelineMarker("Javascript", NS_ConvertUTF8toUTF16(aReason), aMetaData, NO_STACK)
    , mFunctionName(aFunctionName)
    , mFileName(aFileName)
    , mLineNumber(aLineNumber)
  {}

  virtual void AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker) override
  {
    aMarker.mCauseName.Construct(GetCause());

    if (!mFunctionName.IsEmpty() || !mFileName.IsEmpty()) {
      dom::RootedDictionary<dom::ProfileTimelineStackFrame> stackFrame(aCx);
      stackFrame.mLine.Construct(mLineNumber);
      stackFrame.mSource.Construct(mFileName);
      stackFrame.mFunctionDisplayName.Construct(mFunctionName);

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
  nsString mFunctionName;
  nsString mFileName;
  uint32_t mLineNumber;
};

} // namespace mozilla

#endif // mozilla_JavascriptTimelineMarker_h_
