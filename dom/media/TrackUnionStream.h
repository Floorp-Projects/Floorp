/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_TRACKUNIONSTREAM_H_
#define MOZILLA_TRACKUNIONSTREAM_H_

#include "MediaStreamGraph.h"
#include "nsAutoPtr.h"
#include <algorithm>

namespace mozilla {

/**
 * See MediaStreamGraph::CreateTrackUnionStream.
 */
class TrackUnionStream : public ProcessedMediaStream {
 public:
  TrackUnionStream(TrackRate aSampleRate, MediaSegment::Type aType);

  virtual TrackUnionStream* AsTrackUnionStream() override { return this; }
  friend class DOMMediaStream;

  void AddInput(MediaInputPort* aPort) override;
  void RemoveInput(MediaInputPort* aPort) override;
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;

  void SetEnabledImpl(DisabledTrackMode aMode) override;

  friend class MediaStreamGraphImpl;

 protected:
  // Set up this stream from a specific input.
  void SetInput(MediaInputPort* aPort);

  // MediaSegment-agnostic ProcessInput.
  void ProcessInputImpl(MediaStream* aSource, MediaSegment* aSegment,
                        GraphTime aFrom, GraphTime aTo, uint32_t aFlags);

  void AddDirectListenerImpl(
      already_AddRefed<DirectMediaStreamTrackListener> aListener) override;
  void RemoveDirectListenerImpl(
      DirectMediaStreamTrackListener* aListener) override;
  void RemoveAllDirectListenersImpl() override;

  // These are direct track listeners that have been added to this
  // TrackUnionStream-track. While an input is set, these are forwarded to the
  // input stream. We will update these when this track's disabled status
  // changes.
  nsTArray<RefPtr<DirectMediaStreamTrackListener>> mOwnedDirectListeners;

  // Set if an input has been added, nullptr otherwise. Adding more than one
  // input is an error.
  MediaInputPort* mInputPort = nullptr;
};

}  // namespace mozilla

#endif /* MOZILLA_MEDIASTREAMGRAPH_H_ */
