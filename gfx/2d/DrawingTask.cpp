/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawingTask.h"
#include "TaskScheduler.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace gfx {

DrawingTaskBuilder::DrawingTaskBuilder()
: mTask(nullptr)
{}

DrawingTaskBuilder::~DrawingTaskBuilder()
{
  if (mTask) {
    delete mTask;
  }
}

void
DrawingTask::Clear()
{
  mCommandBuffer = nullptr;
  mCursor = 0;
}

void
DrawingTaskBuilder::BeginDrawingTask(MultiThreadedTaskQueue* aTaskQueue,
                                     DrawTarget* aTarget, IntPoint aOffset,
                                     SyncObject* aStart)
{
  MOZ_ASSERT(!mTask);
  MOZ_ASSERT(aTaskQueue);
  mTask = new DrawingTask(aTaskQueue, aTarget, aOffset, aStart);
}

DrawingTask*
DrawingTaskBuilder::EndDrawingTask(CommandBuffer* aCmdBuffer, SyncObject* aCompletion)
{
  MOZ_ASSERT(mTask);
  mTask->mCompletionSync = aCompletion;
  mTask->mCommandBuffer = aCmdBuffer;
  DrawingTask* task = mTask;
  mTask = nullptr;
  return task;
}

DrawingTask::DrawingTask(MultiThreadedTaskQueue* aTaskQueue,
                         DrawTarget* aTarget, IntPoint aOffset,
                         SyncObject* aStart)
: Task(aTaskQueue, aStart, nullptr)
, mCommandBuffer(nullptr)
, mCursor(0)
, mDrawTarget(aTarget)
, mOffset(aOffset)
{
  mCommandOffsets.reserve(64);
}

TaskStatus
DrawingTask::Run()
{
  while (mCursor < mCommandOffsets.size()) {

    const DrawingCommand* cmd = mCommandBuffer->GetDrawingCommand(mCommandOffsets[mCursor]);

    if (!cmd) {
      return TaskStatus::Error;
    }

    cmd->ExecuteOnDT(mDrawTarget);

    ++mCursor;
  }

  return TaskStatus::Complete;
}

DrawingTask::~DrawingTask()
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
