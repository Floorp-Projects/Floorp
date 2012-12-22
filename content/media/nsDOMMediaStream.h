/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDOMMEDIASTREAM_H_
#define NSDOMMEDIASTREAM_H_

#include "nsIDOMMediaStream.h"
#include "MediaStreamGraph.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPrincipal.h"

class nsXPCClassInfo;

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with NS_DECL_NSIDOMMEDIASTREAM, containing
// currentTime getter.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

/**
 * DOM wrapper for MediaStreams.
 */
class nsDOMMediaStream : public nsIDOMMediaStream
{
  friend class nsDOMLocalMediaStream;
  typedef mozilla::MediaStream MediaStream;

public:
  nsDOMMediaStream() : mStream(nullptr), mHintContents(0) {}
  virtual ~nsDOMMediaStream();

  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMMediaStream)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_NSIDOMMEDIASTREAM

  MediaStream* GetStream() { return mStream; }
  bool IsFinished() { return !mStream || mStream->IsFinished(); }
  /**
   * Returns a principal indicating who may access this stream. The stream contents
   * can only be accessed by principals subsuming this principal.
   */
  nsIPrincipal* GetPrincipal() { return mPrincipal; }

  /**
   * Indicate that data will be contributed to this stream from origin aPrincipal.
   * If aPrincipal is null, this is ignored. Otherwise, from now on the contents
   * of this stream can only be accessed by principals that subsume aPrincipal.
   * Returns true if the stream's principal changed.
   */
  bool CombineWithPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Create an nsDOMMediaStream whose underlying stream is a SourceMediaStream.
   */
  static already_AddRefed<nsDOMMediaStream> CreateSourceStream(uint32_t aHintContents);

  // Hints to tell the SDP generator about whether this
  // MediaStream probably has audio and/or video
  enum {
    HINT_CONTENTS_AUDIO = 0x00000001U,
    HINT_CONTENTS_VIDEO = 0x00000002U
  };
  uint32_t GetHintContents() const { return mHintContents; }
  void SetHintContents(uint32_t aHintContents) { mHintContents = aHintContents; }

  /**
   * Create an nsDOMMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<nsDOMMediaStream> CreateTrackUnionStream();

protected:
  // MediaStream is owned by the graph, but we tell it when to die, and it won't
  // die until we let it.
  MediaStream* mStream;
  // Principal identifying who may access the contents of this stream.
  // If null, this stream can be used by anyone because it has no content yet.
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // tells the SDP generator about whether this
  // MediaStream probably has audio and/or video
  uint32_t mHintContents;
};

class nsDOMLocalMediaStream : public nsDOMMediaStream,
                              public nsIDOMLocalMediaStream
{
public:
  nsDOMLocalMediaStream() {}
  virtual ~nsDOMLocalMediaStream() {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMLocalMediaStream, nsDOMMediaStream)
  NS_DECL_NSIDOMLOCALMEDIASTREAM

  NS_FORWARD_NSIDOMMEDIASTREAM(nsDOMMediaStream::)

  /**
   * Create an nsDOMLocalMediaStream whose underlying stream is a SourceMediaStream.
   */
  static already_AddRefed<nsDOMLocalMediaStream> CreateSourceStream(uint32_t aHintContents);

  /**
   * Create an nsDOMLocalMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<nsDOMLocalMediaStream> CreateTrackUnionStream();
};

#endif /* NSDOMMEDIASTREAM_H_ */
