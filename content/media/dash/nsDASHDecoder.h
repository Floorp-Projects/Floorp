/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */

/* This Source Code Form Is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DASH - Dynamic Adaptive Streaming over HTTP
 *
 * DASH is an adaptive bitrate streaming technology where a multimedia file is
 * partitioned into one or more segments and delivered to a client using HTTP.
 *
 * see nsDASHDecoder.cpp for info on DASH interaction with the media engine.*/

#if !defined(nsDASHDecoder_h_)
#define nsDASHDecoder_h_

#include "nsTArray.h"
#include "nsIURI.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "nsBuiltinDecoder.h"
#include "nsDASHReader.h"

class nsDASHRepDecoder;

namespace mozilla {
namespace net {
class IMPDManager;
class nsDASHMPDParser;
class Representation;
}// net
}// mozilla

class nsDASHDecoder : public nsBuiltinDecoder
{
public:
  typedef class mozilla::net::IMPDManager IMPDManager;
  typedef class mozilla::net::nsDASHMPDParser nsDASHMPDParser;
  typedef class mozilla::net::Representation Representation;

  // XXX Arbitrary max file size for MPD. 50MB seems generously large.
  static const uint32_t DASH_MAX_MPD_SIZE = 50*1024*1024;

  nsDASHDecoder();
  ~nsDASHDecoder();

  // Clone not supported; just return nullptr.
  nsMediaDecoder* Clone() { return nullptr; }

  // Creates a single state machine for all stream decoders.
  // Called from Load on the main thread only.
  nsDecoderStateMachine* CreateStateMachine();

  // Loads the MPD from the network and subsequently loads the media streams.
  // Called from the main thread only.
  nsresult Load(MediaResource* aResource,
                nsIStreamListener** aListener,
                nsMediaDecoder* aCloneDonor);

  // Notifies download of MPD file has ended.
  // Called on the main thread only.
  void NotifyDownloadEnded(nsresult aStatus);

  // Notifies that a byte range download has ended. As per the DASH spec, this
  // allows for stream switching at the boundaries of the byte ranges.
  // Called on the main thread only.
  void NotifyDownloadEnded(nsDASHRepDecoder* aRepDecoder,
                           nsresult aStatus,
                           MediaByteRange &aRange);

  // Drop reference to state machine and tell sub-decoders to do the same.
  // Only called during shutdown dance, on main thread only.
  void ReleaseStateMachine();

  // Overridden to forward |Shutdown| to sub-decoders.
  // Called on the main thread only.
  void Shutdown();

  // Called by sub-decoders when load has been aborted. Will notify media
  // element only once. Called on the main thread only.
  void LoadAborted();

  // Notifies the element that decoding has failed. On main thread, call is
  // forwarded to |nsBuiltinDecoder|::|Error| immediately. On other threads,
  // a call is dispatched for execution on the main thread.
  void DecodeError();

private:
  // Reads the MPD data from resource to a byte stream.
  // Called on the MPD reader thread.
  void ReadMPDBuffer();

  // Called when MPD data is completely read.
  // On the main thread.
  void OnReadMPDBufferCompleted();

  // Parses the copied MPD byte stream.
  // On the main thread: DOM APIs complain when off the main thread.
  nsresult ParseMPDBuffer();

  // Creates the sub-decoders for a |Representation|, i.e. media streams.
  // On the main thread.
  nsresult CreateRepDecoders();

  // Creates audio/video decoders for individual |Representation|s.
  // On the main thread.
  nsresult CreateAudioRepDecoder(nsIURI* aUrl, Representation const * aRep);
  nsresult CreateVideoRepDecoder(nsIURI* aUrl, Representation const * aRep);

  // Creates audio/video resources for individual |Representation|s.
  // On the main thread.
  MediaResource* CreateAudioSubResource(nsIURI* aUrl,
                                        nsMediaDecoder* aAudioDecoder);
  MediaResource* CreateVideoSubResource(nsIURI* aUrl,
                                        nsMediaDecoder* aVideoDecoder);

  // Creates an http channel for a |Representation|.
  // On the main thread.
  nsresult CreateSubChannel(nsIURI* aUrl, nsIChannel** aChannel);

  // Loads the media |Representations|, i.e. the media streams.
  // On the main thread.
  nsresult LoadRepresentations();

  // True when media element has already been notified of an aborted load.
  bool mNotifiedLoadAborted;

  // Ptr for the MPD data.
  nsAutoArrayPtr<char>         mBuffer;
  // Length of the MPD data.
  uint32_t                     mBufferLength;
  // Ptr to the MPD Reader thread.
  nsCOMPtr<nsIThread>          mMPDReaderThread;
  // Document Principal.
  nsCOMPtr<nsIPrincipal>       mPrincipal;

  // MPD Manager provides access to the MPD information.
  nsAutoPtr<IMPDManager>       mMPDManager;

  // Main reader object; manages all sub-readers for |Representation|s. Owned by
  // state machine; destroyed in state machine's destructor.
  nsDASHReader* mDASHReader;

  // Sub-decoder for current audio |Representation|.
  nsRefPtr<nsDASHRepDecoder> mAudioRepDecoder;
  // Array of pointers for the |Representation|s in the audio |AdaptationSet|.
  nsTArray<nsRefPtr<nsDASHRepDecoder> > mAudioRepDecoders;

  // Sub-decoder for current video |Representation|.
  nsRefPtr<nsDASHRepDecoder> mVideoRepDecoder;
  // Array of pointers for the |Representation|s in the video |AdaptationSet|.
  nsTArray<nsRefPtr<nsDASHRepDecoder> > mVideoRepDecoders;
};

#endif
