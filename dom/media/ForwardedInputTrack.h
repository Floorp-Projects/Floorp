/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_FORWARDEDINPUTTRACK_H_
#define MOZILLA_FORWARDEDINPUTTRACK_H_

#include "MediaTrackGraph.h"
#include "MediaTrackListener.h"
#include <algorithm>

namespace mozilla {

/**
 * See MediaTrackGraph::CreateForwardedInputTrack.
 */
class ForwardedInputTrack : public ProcessedMediaTrack {
 public:
  ForwardedInputTrack(TrackRate aSampleRate, MediaSegment::Type aType);

  virtual ForwardedInputTrack* AsForwardedInputTrack() override { return this; }
  friend class DOMMediaStream;

  void AddInput(MediaInputPort* aPort) override;
  void RemoveInput(MediaInputPort* aPort) override;
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;

  DisabledTrackMode CombinedDisabledMode() const override;
  void SetDisabledTrackModeImpl(DisabledTrackMode aMode) override;
  void OnInputDisabledModeChanged(DisabledTrackMode aInputMode) override;

  uint32_t NumberOfChannels() const override;

  friend class MediaTrackGraphImpl;

 protected:
  // Set up this track from a specific input.
  void SetInput(MediaInputPort* aPort);

  // MediaSegment-agnostic ProcessInput.
  void ProcessInputImpl(MediaTrack* aSource, MediaSegment* aSegment,
                        GraphTime aFrom, GraphTime aTo, uint32_t aFlags);

  void AddDirectListenerImpl(
      already_AddRefed<DirectMediaTrackListener> aListener) override;
  void RemoveDirectListenerImpl(DirectMediaTrackListener* aListener) override;
  void RemoveAllDirectListenersImpl() override;

  // These are direct track listeners that have been added to this
  // ForwardedInputTrack-track. While an input is set, these are forwarded to
  // the input track. We will update these when this track's disabled status
  // changes.
  nsTArray<RefPtr<DirectMediaTrackListener>> mOwnedDirectListeners;

  // Set if an input has been added, nullptr otherwise. Adding more than one
  // input is an error.
  MediaInputPort* mInputPort = nullptr;

  // This track's input's associated disabled mode. ENABLED if there is no
  // input. This is used with MediaTrackListener::NotifyEnabledStateChanged(),
  // which affects only video tracks. This is set only on ForwardedInputTracks.
  DisabledTrackMode mInputDisabledMode = DisabledTrackMode::ENABLED;
};

}  // namespace mozilla

#endif /* MOZILLA_FORWARDEDINPUTTRACK_H_ */
