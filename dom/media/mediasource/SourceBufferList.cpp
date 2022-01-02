/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBufferList.h"

#include "AsyncEventRunner.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/SourceBufferListBinding.h"
#include "mozilla/mozalloc.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"

extern mozilla::LogModule* GetMediaSourceLog();
extern mozilla::LogModule* GetMediaSourceAPILog();

#define MSE_API(arg, ...)                                   \
  MOZ_LOG(GetMediaSourceAPILog(), mozilla::LogLevel::Debug, \
          ("SourceBufferList(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MSE_DEBUG(arg, ...)                              \
  MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, \
          ("SourceBufferList(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

struct JSContext;
class JSObject;

namespace mozilla::dom {

SourceBufferList::~SourceBufferList() = default;

SourceBuffer* SourceBufferList::IndexedGetter(uint32_t aIndex, bool& aFound) {
  MOZ_ASSERT(NS_IsMainThread());
  aFound = aIndex < mSourceBuffers.Length();

  if (!aFound) {
    return nullptr;
  }
  return mSourceBuffers[aIndex];
}

uint32_t SourceBufferList::Length() {
  MOZ_ASSERT(NS_IsMainThread());
  return mSourceBuffers.Length();
}

void SourceBufferList::Append(SourceBuffer* aSourceBuffer) {
  MOZ_ASSERT(NS_IsMainThread());
  mSourceBuffers.AppendElement(aSourceBuffer);
  QueueAsyncSimpleEvent("addsourcebuffer");
}

void SourceBufferList::AppendSimple(SourceBuffer* aSourceBuffer) {
  MOZ_ASSERT(NS_IsMainThread());
  mSourceBuffers.AppendElement(aSourceBuffer);
}

void SourceBufferList::Remove(SourceBuffer* aSourceBuffer) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ALWAYS_TRUE(mSourceBuffers.RemoveElement(aSourceBuffer));
  aSourceBuffer->Detach();
  QueueAsyncSimpleEvent("removesourcebuffer");
}

bool SourceBufferList::Contains(SourceBuffer* aSourceBuffer) {
  MOZ_ASSERT(NS_IsMainThread());
  return mSourceBuffers.Contains(aSourceBuffer);
}

void SourceBufferList::Clear() {
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Detach();
  }
  mSourceBuffers.Clear();
  QueueAsyncSimpleEvent("removesourcebuffer");
}

void SourceBufferList::ClearSimple() {
  MOZ_ASSERT(NS_IsMainThread());
  mSourceBuffers.Clear();
}

bool SourceBufferList::IsEmpty() {
  MOZ_ASSERT(NS_IsMainThread());
  return mSourceBuffers.IsEmpty();
}

bool SourceBufferList::AnyUpdating() {
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    if (mSourceBuffers[i]->Updating()) {
      return true;
    }
  }
  return false;
}

void SourceBufferList::RangeRemoval(double aStart, double aEnd) {
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("RangeRemoval(aStart=%f, aEnd=%f)", aStart, aEnd);
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->RangeRemoval(aStart, aEnd);
  }
}

void SourceBufferList::Ended() {
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Ended();
  }
}

double SourceBufferList::GetHighestBufferedEndTime() {
  MOZ_ASSERT(NS_IsMainThread());
  double highestEndTime = 0;
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    highestEndTime =
        std::max(highestEndTime, mSourceBuffers[i]->GetBufferedEnd());
  }
  return highestEndTime;
}

void SourceBufferList::DispatchSimpleEvent(const char* aName) {
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("Dispatch event '%s'", aName);
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void SourceBufferList::QueueAsyncSimpleEvent(const char* aName) {
  MSE_DEBUG("Queue event '%s'", aName);
  nsCOMPtr<nsIRunnable> event =
      new AsyncEventRunner<SourceBufferList>(this, aName);
  mAbstractMainThread->Dispatch(event.forget());
}

SourceBufferList::SourceBufferList(MediaSource* aMediaSource)
    : DOMEventTargetHelper(aMediaSource->GetParentObject()),
      mMediaSource(aMediaSource),
      mAbstractMainThread(mMediaSource->AbstractMainThread()) {
  MOZ_ASSERT(aMediaSource);
}

MediaSource* SourceBufferList::GetParentObject() const { return mMediaSource; }

double SourceBufferList::HighestStartTime() {
  MOZ_ASSERT(NS_IsMainThread());
  double highestStartTime = 0;
  for (auto& sourceBuffer : mSourceBuffers) {
    highestStartTime =
        std::max(sourceBuffer->HighestStartTime(), highestStartTime);
  }
  return highestStartTime;
}

double SourceBufferList::HighestEndTime() {
  MOZ_ASSERT(NS_IsMainThread());
  double highestEndTime = 0;
  for (auto& sourceBuffer : mSourceBuffers) {
    highestEndTime = std::max(sourceBuffer->HighestEndTime(), highestEndTime);
  }
  return highestEndTime;
}

JSObject* SourceBufferList::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return SourceBufferList_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(SourceBufferList, DOMEventTargetHelper,
                                   mMediaSource, mSourceBuffers)

NS_IMPL_ADDREF_INHERITED(SourceBufferList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SourceBufferList, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SourceBufferList)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

#undef MSE_API
#undef MSE_DEBUG
}  // namespace mozilla::dom
