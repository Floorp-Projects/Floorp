/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioParamTimeline_h_
#define AudioParamTimeline_h_

#include "AudioEventTimeline.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"
#include "MediaStreamGraph.h"

namespace mozilla {

namespace dom {

// This helper class is used to represent the part of the AudioParam
// class that gets sent to AudioNodeEngine instances.  In addition to
// AudioEventTimeline methods, it holds a pointer to an optional
// MediaStream which represents the AudioNode inputs to the AudioParam.
// This MediaStream is managed by the AudioParam subclass on the main
// thread, and can only be obtained from the AudioNodeEngine instances
// consuming this class.
class AudioParamTimeline : public AudioEventTimeline<ErrorResult>
{
public:
  explicit AudioParamTimeline(float aDefaultValue)
    : AudioEventTimeline<ErrorResult>(aDefaultValue)
  {
  }

  MediaStream* Stream() const
  {
    return mStream;
  }

protected:
  // This is created lazily when needed.
  nsRefPtr<MediaStream> mStream;
};

}
}

#endif

