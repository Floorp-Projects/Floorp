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

extern PRLogModuleInfo* GetMediaSourceLog();
extern PRLogModuleInfo* GetMediaSourceAPILog();

#define MSE_API(arg, ...) PR_LOG(GetMediaSourceAPILog(), PR_LOG_DEBUG, ("SourceBufferList(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MSE_DEBUG(arg, ...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG, ("SourceBufferList(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

struct JSContext;
class JSObject;

namespace mozilla {

namespace dom {

SourceBufferList::~SourceBufferList()
{
}

SourceBuffer*
SourceBufferList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  MOZ_ASSERT(NS_IsMainThread());
  aFound = aIndex < mSourceBuffers.Length();
  return aFound ? mSourceBuffers[aIndex] : nullptr;
}

uint32_t
SourceBufferList::Length()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mSourceBuffers.Length();
}

void
SourceBufferList::Append(SourceBuffer* aSourceBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());
  mSourceBuffers.AppendElement(aSourceBuffer);
  QueueAsyncSimpleEvent("addsourcebuffer");
}

void
SourceBufferList::AppendSimple(SourceBuffer* aSourceBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());
  mSourceBuffers.AppendElement(aSourceBuffer);
}

void
SourceBufferList::Remove(SourceBuffer* aSourceBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ALWAYS_TRUE(mSourceBuffers.RemoveElement(aSourceBuffer));
  aSourceBuffer->Detach();
  QueueAsyncSimpleEvent("removesourcebuffer");
}

bool
SourceBufferList::Contains(SourceBuffer* aSourceBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());
  return mSourceBuffers.Contains(aSourceBuffer);
}

void
SourceBufferList::Clear()
{
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Detach();
  }
  mSourceBuffers.Clear();
  QueueAsyncSimpleEvent("removesourcebuffer");
}

void
SourceBufferList::ClearSimple()
{
  MOZ_ASSERT(NS_IsMainThread());
  mSourceBuffers.Clear();
}

bool
SourceBufferList::IsEmpty()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mSourceBuffers.IsEmpty();
}

bool
SourceBufferList::AnyUpdating()
{
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    if (mSourceBuffers[i]->Updating()) {
      return true;
    }
  }
  return false;
}

void
SourceBufferList::RangeRemoval(double aStart, double aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("RangeRemoval(aStart=%f, aEnd=%f)", aStart, aEnd);
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->RangeRemoval(aStart, aEnd);
  }
}

void
SourceBufferList::Evict(double aStart, double aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("Evict(aStart=%f, aEnd=%f)", aStart, aEnd);
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Evict(aStart, aEnd);
  }
}

void
SourceBufferList::Ended()
{
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Ended();
  }
}

double
SourceBufferList::GetHighestBufferedEndTime()
{
  MOZ_ASSERT(NS_IsMainThread());
  double highestEndTime = 0;
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    highestEndTime = std::max(highestEndTime, mSourceBuffers[i]->GetBufferedEnd());
  }
  return highestEndTime;
}

void
SourceBufferList::DispatchSimpleEvent(const char* aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("Dispatch event '%s'", aName);
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void
SourceBufferList::QueueAsyncSimpleEvent(const char* aName)
{
  MSE_DEBUG("Queue event '%s'", aName);
  nsCOMPtr<nsIRunnable> event = new AsyncEventRunner<SourceBufferList>(this, aName);
  NS_DispatchToMainThread(event);
}

#if defined(DEBUG)
void
SourceBufferList::Dump(const char* aPath)
{
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Dump(aPath);
  }
}
#endif

SourceBufferList::SourceBufferList(MediaSource* aMediaSource)
  : DOMEventTargetHelper(aMediaSource->GetParentObject())
  , mMediaSource(aMediaSource)
{
  MOZ_ASSERT(aMediaSource);
}

MediaSource*
SourceBufferList::GetParentObject() const
{
  return mMediaSource;
}

JSObject*
SourceBufferList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SourceBufferListBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(SourceBufferList, DOMEventTargetHelper,
                                   mMediaSource, mSourceBuffers)

NS_IMPL_ADDREF_INHERITED(SourceBufferList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SourceBufferList, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SourceBufferList)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

#undef MSE_API
#undef MSE_DEBUG
} // namespace dom

} // namespace mozilla
