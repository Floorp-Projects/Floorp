/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawingJob.h"
#include "JobScheduler.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace gfx {

DrawingJobBuilder::DrawingJobBuilder()
{}

DrawingJobBuilder::~DrawingJobBuilder()
{
  MOZ_ASSERT(!mDrawTarget);
}

void
DrawingJob::Clear()
{
  mCommandBuffer = nullptr;
  mCursor = 0;
}

void
DrawingJobBuilder::BeginDrawingJob(DrawTarget* aTarget, IntPoint aOffset,
                                     SyncObject* aStart)
{
  MOZ_ASSERT(mCommandOffsets.empty());
  MOZ_ASSERT(aTarget);
  mDrawTarget = aTarget;
  mOffset = aOffset;
  mStart = aStart;
}

DrawingJob*
DrawingJobBuilder::EndDrawingJob(CommandBuffer* aCmdBuffer,
                                   SyncObject* aCompletion,
                                   WorkerThread* aPinToWorker)
{
  MOZ_ASSERT(mDrawTarget);
  DrawingJob* task = new DrawingJob(mDrawTarget, mOffset, mStart, aCompletion, aPinToWorker);
  task->mCommandBuffer = aCmdBuffer;
  task->mCommandOffsets = std::move(mCommandOffsets);

  mDrawTarget = nullptr;
  mOffset = IntPoint();
  mStart = nullptr;

  return task;
}

DrawingJob::DrawingJob(DrawTarget* aTarget, IntPoint aOffset,
                         SyncObject* aStart, SyncObject* aCompletion,
                         WorkerThread* aPinToWorker)
: Job(aStart, aCompletion, aPinToWorker)
, mCommandBuffer(nullptr)
, mCursor(0)
, mDrawTarget(aTarget)
, mOffset(aOffset)
{
  mCommandOffsets.reserve(64);
}

JobStatus
DrawingJob::Run()
{
  while (mCursor < mCommandOffsets.size()) {

    const DrawingCommand* cmd = mCommandBuffer->GetDrawingCommand(mCommandOffsets[mCursor]);

    if (!cmd) {
      return JobStatus::Error;
    }

    cmd->ExecuteOnDT(mDrawTarget);

    ++mCursor;
  }

  return JobStatus::Complete;
}

DrawingJob::~DrawingJob()
{
  Clear();
}

const DrawingCommand*
CommandBuffer::GetDrawingCommand(ptrdiff_t aId)
{
  return static_cast<DrawingCommand*>(mStorage.GetStorage(aId));
}

CommandBuffer::~CommandBuffer()
{
  mStorage.ForEach([](void* item){
    static_cast<DrawingCommand*>(item)->~DrawingCommand();
  });
  mStorage.Clear();
}

void
CommandBufferBuilder::BeginCommandBuffer(size_t aBufferSize)
{
  MOZ_ASSERT(!mCommands);
  mCommands = new CommandBuffer(aBufferSize);
}

already_AddRefed<CommandBuffer>
CommandBufferBuilder::EndCommandBuffer()
{
  return mCommands.forget();
}

} // namespace
} // namespace
