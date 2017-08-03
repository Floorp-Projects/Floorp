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
#include "mozilla/gfx/JobScheduler.h"
#include "mozilla/gfx/IterableArena.h"
#include "mozilla/RefCounted.h"
#include "DrawCommand.h"

namespace mozilla {
namespace gfx {

class DrawingCommand;
class PrintCommand;
class SignalCommand;
class DrawingJob;
class WaitCommand;

class SyncObject;
class MultiThreadedJobQueue;

class DrawTarget;

class DrawingJobBuilder;
class CommandBufferBuilder;

/// Contains a sequence of immutable drawing commands that are typically used by
/// several DrawingJobs.
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
class DrawingJob : public Job {
public:
  ~DrawingJob();

  virtual JobStatus Run() override;

protected:
  DrawingJob(DrawTarget* aTarget,
              IntPoint aOffset,
              SyncObject* aStart,
              SyncObject* aCompletion,
              WorkerThread* aPinToWorker = nullptr);

  /// Runs the tasks's destructors and resets the buffer.
  void Clear();

  std::vector<ptrdiff_t> mCommandOffsets;
  RefPtr<CommandBuffer> mCommandBuffer;
  uint32_t mCursor;

  RefPtr<DrawTarget> mDrawTarget;
  IntPoint mOffset;

  friend class DrawingJobBuilder;
};

/// Generates DrawingJob objects.
///
/// The builder is a separate object to ensure that commands are not added to a
/// submitted DrawingJob.
class DrawingJobBuilder {
public:
  DrawingJobBuilder();

  ~DrawingJobBuilder();

  /// Allocates a DrawingJob.
  ///
  /// call this method before starting to add commands.
  void BeginDrawingJob(DrawTarget* aTarget, IntPoint aOffset,
                        SyncObject* aStart = nullptr);

  /// Build the DrawingJob, command after command.
  /// This must be used between BeginDrawingJob and EndDrawingJob.
  void AddCommand(ptrdiff_t offset)
  {
    mCommandOffsets.push_back(offset);
  }

  /// Finalizes and returns the drawing task.
  ///
  /// If aCompletion is not null, the sync object will be signaled after the
  /// task buffer is destroyed (and after the destructor of the tasks have run).
  /// In most cases this means after the completion of all tasks in the task buffer,
  /// but also when the task buffer is destroyed due to an error.
  DrawingJob* EndDrawingJob(CommandBuffer* aCmdBuffer,
                              SyncObject* aCompletion = nullptr,
                              WorkerThread* aPinToWorker = nullptr);

  /// Returns true between BeginDrawingJob and EndDrawingJob, false otherwise.
  bool HasDrawingJob() const { return !!mDrawTarget; }

protected:
  std::vector<ptrdiff_t> mCommandOffsets;
  RefPtr<DrawTarget> mDrawTarget;
  IntPoint mOffset;
  RefPtr<SyncObject> mStart;
};

} // namespace
} // namespace

#endif
