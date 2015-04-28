/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIONODEEXTERNALINPUTSTREAM_H_
#define MOZILLA_AUDIONODEEXTERNALINPUTSTREAM_H_

#include "MediaStreamGraph.h"
#include "AudioNodeStream.h"
#include "mozilla/Atomics.h"

namespace mozilla {

/**
 * This is a MediaStream implementation that acts for a Web Audio node but
 * unlike other AudioNodeStreams, supports any kind of MediaStream as an
 * input --- handling any number of audio tracks and handling blocking of
 * the input MediaStream.
 */
class AudioNodeExternalInputStream final : public AudioNodeStream
{
public:
  AudioNodeExternalInputStream(AudioNodeEngine* aEngine, TrackRate aSampleRate,
                               uint32_t aContextId);
protected:
  ~AudioNodeExternalInputStream();

public:
  virtual void ProcessInput(GraphTime aFrom, GraphTime aTo,
                            uint32_t aFlags) override;

private:
  /**
   * Determines if this is enabled or not.  Disabled nodes produce silence.
   * This node becomes disabled if the document principal does not subsume the
   * DOMMediaStream principal.
   */
  bool IsEnabled();
};

}

#endif /* MOZILLA_AUDIONODESTREAM_H_ */
