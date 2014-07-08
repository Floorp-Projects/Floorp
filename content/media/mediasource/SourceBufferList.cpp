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
#include "nsIEventTarget.h"
#include "nsIRunnable.h"
#include "nsStringGlue.h"
#include "nsThreadUtils.h"
#include "prlog.h"

struct JSContext;
class JSObject;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaSourceLog;
#define MSE_DEBUG(...) PR_LOG(gMediaSourceLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#endif

namespace mozilla {

namespace dom {

SourceBufferList::~SourceBufferList()
{
}

SourceBuffer*
SourceBufferList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = aIndex < mSourceBuffers.Length();
  return aFound ? mSourceBuffers[aIndex] : nullptr;
}

uint32_t
SourceBufferList::Length()
{
  return mSourceBuffers.Length();
}

void
SourceBufferList::Append(SourceBuffer* aSourceBuffer)
{
  mSourceBuffers.AppendElement(aSourceBuffer);
  QueueAsyncSimpleEvent("addsourcebuffer");
}

void
SourceBufferList::Remove(SourceBuffer* aSourceBuffer)
{
  MOZ_ALWAYS_TRUE(mSourceBuffers.RemoveElement(aSourceBuffer));
  aSourceBuffer->Detach();
  QueueAsyncSimpleEvent("removesourcebuffer");
}

bool
SourceBufferList::Contains(SourceBuffer* aSourceBuffer)
{
  return mSourceBuffers.Contains(aSourceBuffer);
}

void
SourceBufferList::Clear()
{
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Detach();
  }
  mSourceBuffers.Clear();
  QueueAsyncSimpleEvent("removesourcebuffer");
}

bool
SourceBufferList::IsEmpty()
{
  return mSourceBuffers.IsEmpty();
}

bool
SourceBufferList::AnyUpdating()
{
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    if (mSourceBuffers[i]->Updating()) {
      return true;
    }
  }
  return false;
}

void
SourceBufferList::Remove(double aStart, double aEnd, ErrorResult& aRv)
{
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Remove(aStart, aEnd, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

void
SourceBufferList::Evict(double aStart, double aEnd)
{
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Evict(aStart, aEnd);
  }
}

bool
SourceBufferList::AllContainsTime(double aTime)
{
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    if (!mSourceBuffers[i]->ContainsTime(aTime)) {
      return false;
    }
  }
  return mSourceBuffers.Length() > 0;
}

void
SourceBufferList::Ended()
{
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Ended();
  }
}

void
SourceBufferList::DispatchSimpleEvent(const char* aName)
{
  MSE_DEBUG("%p Dispatching event %s to SourceBufferList", this, aName);
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void
SourceBufferList::QueueAsyncSimpleEvent(const char* aName)
{
  MSE_DEBUG("%p Queuing event %s to SourceBufferList", this, aName);
  nsCOMPtr<nsIRunnable> event = new AsyncEventRunner<SourceBufferList>(this, aName);
  NS_DispatchToMainThread(event);
}

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
SourceBufferList::WrapObject(JSContext* aCx)
{
  return SourceBufferListBinding::Wrap(aCx, this);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(SourceBufferList, DOMEventTargetHelper,
                                   mMediaSource, mSourceBuffers)

NS_IMPL_ADDREF_INHERITED(SourceBufferList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SourceBufferList, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SourceBufferList)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

} // namespace dom

} // namespace mozilla
