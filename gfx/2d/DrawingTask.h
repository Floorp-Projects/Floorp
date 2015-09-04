/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMMANDBUFFER_H_
#define MOZILLA_GFX_COMMANDBUFFER_H_

#include <stdint.h>

#include "mozilla/RefPtr.h"
#include "mozilla/Assertions.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/TaskScheduler.h"
#include "mozilla/gfx/IterableArena.h"
#include "DrawCommand.h"

namespace mozilla {
namespace gfx {

class DrawingCommand;
class PrintCommand;
class SignalCommand;
class DrawingTask;
class WaitCommand;

class SyncObject;
class MultiThreadedTaskQueue;

class DrawTarget;

class DrawingTaskBuilder;
class CommandBufferBuilder;

/// Contains a sequence of immutable drawing commands that are typically used by
/// several DrawingTasks.
///
/// CommandBuffer objects are built using CommandBufferBuilder.
class CommandBuffer : public external::AtomicRefCounted<CommandBuffer>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(CommandBuffer)

  ~CommandBuffer();

  const DrawingCommand* GetDrawingCommand(ptrdiff_t aId);

protected:
  explicit CommandBuffer(size_t aSize = 256)
  : mStorage(IterableArena::GROWABLE, aSize)
  {}

  IterableArena mStorage;
  friend class CommandBufferBuilder;
};

/// Generates CommandBuffer objects.
///
/// The builder is a separate object to ensure that commands are not added to a
/// submitted CommandBuffer.
class CommandBufferBuilder
{
public:
  void BeginCommandBuffer(size_t aBufferSize = 256);

  already_AddRefed<CommandBuffer> EndCommandBuffer();

  /// Build the CommandBuffer, command after command.
  /// This must be used between BeginCommandBuffer and EndCommandBuffer.
  template<typename T, typename... Args>
  ptrdiff_t AddCommand(Args&&... aArgs)
  {
    static_assert(IsBaseOf<DrawingCommand, T>::value,
                  "T must derive from DrawingCommand");
    return mCommands->mStorage.Alloc<T>(Forward<Args>(aArgs)...);
  }

  bool HasCommands() const { return !!mCommands; }

protected:
  RefPtr<CommandBuffer> mCommands;
};

/// Stores multiple commands to be executed sequencially.
class DrawingTask : public Task {
public:
  DrawingTask(MultiThreadedTaskQueue* aTaskQueue,
              DrawTarget* aTarget,
              IntPoint aOffset,
              SyncObject* aStart);

  ~DrawingTask();

  virtual TaskStatus Run() override;

protected:
  /// Runs the tasks's destructors and resets the buffer.
  void Clear();

  std::vector<ptrdiff_t> mCommandOffsets;
  RefPtr<CommandBuffer> mCommandBuffer;
  uint32_t mCursor;

  RefPtr<DrawTarget> mDrawTarget;
  IntPoint mOffset;

  friend class DrawingTaskBuilder;
};

/// Generates DrawingTask objects.
///
/// The builder is a separate object to ensure that commands are not added to a
/// submitted DrawingTask.
class DrawingTaskBuilder {
public:
  DrawingTaskBuilder();

  ~DrawingTaskBuilder();

  /// Allocates a DrawingTask.
  ///
  /// call this method before starting to add commands.
  void BeginDrawingTask(MultiThreadedTaskQueue* aTaskQueue,
                        DrawTarget* aTarget, IntPoint aOffset,
                        SyncObject* aStart = nullptr);

  /// Build the DrawingTask, command after command.
  /// This must be used between BeginDrawingTask and EndDrawingTask.
  void AddCommand(ptrdiff_t offset)
  {
    MOZ_ASSERT(mTask);
    mTask->mCommandOffsets.push_back(offset);
  }

  /// Finalizes and returns the command buffer.
  ///
  /// If aCompletion is not null, the sync object will be signaled after the
  /// task buffer is destroyed (and after the destructor of the tasks have run).
  /// In most cases this means after the completion of all tasks in the task buffer,
  /// but also when the task buffer is destroyed due to an error.
  DrawingTask* EndDrawingTask(CommandBuffer* aCmdBuffer, SyncObject* aCompletion = nullptr);

  /// Returns true between BeginDrawingTask and EndDrawingTask, false otherwise.
  bool HasDrawingTask() const { return !!mTask; }

protected:
  DrawingTask* mTask;
};

} // namespace
} // namespace

#endif
