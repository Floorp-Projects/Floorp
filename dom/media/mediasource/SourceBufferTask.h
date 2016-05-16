/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SOURCEBUFFERTASK_H_
#define MOZILLA_SOURCEBUFFERTASK_H_

#include "mozilla/MozPromise.h"
#include "mozilla/Pair.h"
#include "mozilla/RefPtr.h"
#include "SourceBufferAttributes.h"
#include "TimeUnits.h"

namespace mozilla {

class SourceBufferTask {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SourceBufferTask);
  enum class Type  {
    AppendBuffer,
    Abort,
    Reset,
    RangeRemoval,
    EvictData,
    Detach
  };

  typedef Pair<bool, SourceBufferAttributes> AppendBufferResult;
  typedef MozPromise<AppendBufferResult, nsresult, /* IsExclusive = */ true> AppendPromise;
  typedef MozPromise<bool, nsresult, /* IsExclusive = */ true> RangeRemovalPromise;

  virtual Type GetType() const = 0;

  template<typename ReturnType>
  ReturnType* As()
  {
    MOZ_ASSERT(this->GetType() == ReturnType::sType);
    return static_cast<ReturnType*>(this);
  }

protected:
  virtual ~SourceBufferTask() {}
};

class AppendBufferTask : public SourceBufferTask {
public:
  AppendBufferTask(MediaByteBuffer* aData,
                   SourceBufferAttributes aAttributes)
  : mBuffer(aData)
  , mAttributes(aAttributes)
  {}

  static const Type sType = Type::AppendBuffer;
  Type GetType() const override { return Type::AppendBuffer; }

  RefPtr<MediaByteBuffer> mBuffer;
  SourceBufferAttributes mAttributes;
  MozPromiseHolder<AppendPromise> mPromise;
};

class AbortTask : public SourceBufferTask {
public:
  static const Type sType = Type::Abort;
  Type GetType() const override { return Type::Abort; }
};

class ResetTask : public SourceBufferTask {
public:
  static const Type sType = Type::Reset;
  Type GetType() const override { return Type::Reset; }
};

class RangeRemovalTask : public SourceBufferTask {
public:
  explicit RangeRemovalTask(const media::TimeInterval& aRange)
  : mRange(aRange)
  {}

  static const Type sType = Type::RangeRemoval;
  Type GetType() const override { return Type::RangeRemoval; }

  media::TimeInterval mRange;
  MozPromiseHolder<RangeRemovalPromise> mPromise;
};

class EvictDataTask : public SourceBufferTask {
public:
  EvictDataTask(const media::TimeUnit& aPlaybackTime, int64_t aSizetoEvict)
  : mPlaybackTime(aPlaybackTime)
  , mSizeToEvict(aSizetoEvict)
  {}

  static const Type sType = Type::EvictData;
  Type GetType() const override { return Type::EvictData; }

  media::TimeUnit mPlaybackTime;
  int64_t mSizeToEvict;
};

class DetachTask : public SourceBufferTask {
public:
  static const Type sType = Type::Detach;
  Type GetType() const override { return Type::Detach; }
};

} // end mozilla namespace

#endif