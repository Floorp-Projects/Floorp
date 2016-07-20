/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An interface for tasks which can execute on the ImageLib DecodePool, and
 * various implementations.
 */

#ifndef mozilla_image_IDecodingTask_h
#define mozilla_image_IDecodingTask_h

#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"

#include "SourceBuffer.h"

namespace mozilla {
namespace image {

class Decoder;

/// A priority hint that DecodePool can use when scheduling an IDecodingTask.
enum class TaskPriority : uint8_t
{
  eLow,
  eHigh
};

/**
 * An interface for tasks which can execute on the ImageLib DecodePool.
 */
class IDecodingTask : public IResumable
{
public:
  /// Run the task.
  virtual void Run() = 0;

  /// @return true if, given the option, this task prefers to run synchronously.
  virtual bool ShouldPreferSyncRun() const = 0;

  /// @return a priority hint that DecodePool can use when scheduling this task.
  virtual TaskPriority Priority() const = 0;

  /// A default implementation of IResumable which resubmits the task to the
  /// DecodePool. Subclasses can override this if they need different behavior.
  void Resume() override;

  /// @return a non-null weak pointer to the Decoder associated with this task.
  virtual NotNull<Decoder*> GetDecoder() const = 0;

protected:
  virtual ~IDecodingTask() { }
};


/**
 * An IDecodingTask implementation for full decodes of images.
 */
class DecodingTask final : public IDecodingTask
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecodingTask, override)

  explicit DecodingTask(NotNull<Decoder*> aDecoder);

  void Run() override;
  bool ShouldPreferSyncRun() const override;

  // Full decodes are low priority compared to metadata decodes because they
  // don't block layout or page load.
  TaskPriority Priority() const override { return TaskPriority::eLow; }

  NotNull<Decoder*> GetDecoder() const override { return mDecoder; }

private:
  virtual ~DecodingTask() { }

  NotNull<RefPtr<Decoder>> mDecoder;
};


/**
 * An IDecodingTask implementation for metadata decodes of images.
 */
class MetadataDecodingTask final : public IDecodingTask
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MetadataDecodingTask, override)

  explicit MetadataDecodingTask(NotNull<Decoder*> aDecoder);

  void Run() override;

  // Metadata decodes are very fast (since they only need to examine an image's
  // header) so there's no reason to refuse to run them synchronously if the
  // caller will allow us to.
  bool ShouldPreferSyncRun() const override { return true; }

  // Metadata decodes run at the highest priority because they block layout and
  // page load.
  TaskPriority Priority() const override { return TaskPriority::eHigh; }

  NotNull<Decoder*> GetDecoder() const override { return mDecoder; }

private:
  virtual ~MetadataDecodingTask() { }

  NotNull<RefPtr<Decoder>> mDecoder;
};


/**
 * An IDecodingTask implementation for anonymous decoders - that is, decoders
 * with no associated Image object.
 */
class AnonymousDecodingTask final : public IDecodingTask
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AnonymousDecodingTask, override)

  explicit AnonymousDecodingTask(NotNull<Decoder*> aDecoder);

  void Run() override;

  bool ShouldPreferSyncRun() const override { return true; }
  TaskPriority Priority() const override { return TaskPriority::eLow; }

  // Anonymous decoders normally get all their data at once. We have tests where
  // they don't; in these situations, the test re-runs them manually. So no
  // matter what, we don't want to resume by posting a task to the DecodePool.
  void Resume() override { }

  NotNull<Decoder*> GetDecoder() const override { return mDecoder; }

private:
  virtual ~AnonymousDecodingTask() { }

  NotNull<RefPtr<Decoder>> mDecoder;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_IDecodingTask_h
