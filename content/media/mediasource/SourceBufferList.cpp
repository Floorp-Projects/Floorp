/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBufferList.h"

#include "mozilla/dom/SourceBufferListBinding.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaSourceLog;
#define LOG(type, msg) PR_LOG(gMediaSourceLog, type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {
namespace dom {

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
  mSourceBuffers.Clear();
  QueueAsyncSimpleEvent("removesourcebuffer");
}

bool
SourceBufferList::IsEmpty()
{
  return mSourceBuffers.IsEmpty();
}

void
SourceBufferList::DetachAndClear()
{
  for (uint32_t i = 0; i < mSourceBuffers.Length(); ++i) {
    mSourceBuffers[i]->Detach();
  }
  Clear();
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
SourceBufferList::DispatchSimpleEvent(const char* aName)
{
  LOG(PR_LOG_DEBUG, ("%p Dispatching event %s to SourceBufferList", this, aName));
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void
SourceBufferList::QueueAsyncSimpleEvent(const char* aName)
{
  LOG(PR_LOG_DEBUG, ("%p Queuing event %s to SourceBufferList", this, aName));
  nsCOMPtr<nsIRunnable> event = new AsyncEventRunnner<SourceBufferList>(this, aName);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

SourceBufferList::SourceBufferList(MediaSource* aMediaSource)
  : nsDOMEventTargetHelper(aMediaSource->GetParentObject())
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
SourceBufferList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SourceBufferListBinding::Wrap(aCx, aScope, this);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(SourceBufferList, nsDOMEventTargetHelper,
                                     mMediaSource, mSourceBuffers)

NS_IMPL_ADDREF_INHERITED(SourceBufferList, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SourceBufferList, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SourceBufferList)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

} // namespace dom
} // namespace mozilla
