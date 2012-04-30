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

/**
 * DOM wrapper for MediaStreams.
 */
class nsDOMMediaStream : public nsIDOMMediaStream
{
  typedef mozilla::MediaStream MediaStream;

public:
  nsDOMMediaStream() : mStream(nsnull) {}
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
  static already_AddRefed<nsDOMMediaStream> CreateInputStream();

protected:
  // MediaStream is owned by the graph, but we tell it when to die, and it won't
  // die until we let it.
  MediaStream* mStream;
  // Principal identifying who may access the contents of this stream.
  // If null, this stream can be used by anyone because it has no content yet.
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

#endif /* NSDOMMEDIASTREAM_H_ */
