/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *  Chris Pearce <chris@pearce.org.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#if !defined(nsOggCodecState_h_)
#define nsOggCodecState_h_

#include <ogg/ogg.h>
#include <theora/theoradec.h>
#include <vorbis/codec.h>
#include <nsDeque.h>

class OggPageDeallocator : public nsDequeFunctor {
  virtual void* operator() (void* aPage) {
    ogg_page* p = static_cast<ogg_page*>(aPage);
    delete p->header;
    delete p;
    return nsnull;
  }
};

// A queue of ogg_pages. When we read a page, and it's not from the bitstream
// which we're looking for a page for, we buffer the page in the nsOggCodecState,
// rather than pushing it immediately into the ogg_stream_state object. This
// is because if we're skipping up to the next keyframe in very large frame
// sized videos, there may be several megabytes of data between keyframes,
// and the ogg_stream_state would end up resizing its buffer every time we
// added a new 4K page to the bitstream, which kills performance on Windows.
class nsPageQueue : private nsDeque {
public:
  nsPageQueue() : nsDeque(new OggPageDeallocator()) {}
  ~nsPageQueue() { Erase(); }
  PRBool IsEmpty() { return nsDeque::GetSize() == 0; }
  void Append(ogg_page* aPage);
  ogg_page* PopFront() { return static_cast<ogg_page*>(nsDeque::PopFront()); }
  ogg_page* PeekFront() { return static_cast<ogg_page*>(nsDeque::PeekFront()); }
  void Erase() { nsDeque::Erase(); }
};

// Encapsulates the data required for decoding an ogg bitstream and for
// converting granulepos to timestamps.
class nsOggCodecState {
 public:
  // Ogg types we know about
  enum CodecType {
    TYPE_VORBIS=0,
    TYPE_THEORA=1,
    TYPE_SKELETON=2,
    TYPE_UNKNOWN=3
  };

 public:
  nsOggCodecState(ogg_page* aBosPage);
  virtual ~nsOggCodecState();
  
  // Factory for creating nsCodecStates.
  static nsOggCodecState* Create(ogg_page* aPage);
  
  virtual CodecType GetType() { return TYPE_UNKNOWN; }
  
  // Reads a header packet. Returns PR_TRUE when last header has been read.
  virtual PRBool DecodeHeader(ogg_packet* aPacket) {
    return (mDoneReadingHeaders = PR_TRUE);
  }

  // Returns the end time that a granulepos represents.
  virtual PRInt64 Time(PRInt64 granulepos) { return -1; }

  // Returns the start time that a granulepos represents.
  virtual PRInt64 StartTime(PRInt64 granulepos) { return -1; }

  // Initializes the codec state.
  virtual PRBool Init();

  // Returns PR_TRUE when this bitstream has finished reading all its
  // header packets.
  PRBool DoneReadingHeaders() { return mDoneReadingHeaders; }

  // Deactivates the bitstream. Only the primary video and audio bitstreams
  // should be active.
  void Deactivate() { mActive = PR_FALSE; }

  // Resets decoding state.
  virtual nsresult Reset();

  // Clones a page and adds it to our buffer of pages which we'll insert to
  // the bitstream at a later time (using PageInFromBuffer()). Memory stored in
  // cloned pages is freed when Reset() or PageInFromBuffer() are called.
  inline void AddToBuffer(ogg_page* aPage) { mBuffer.Append(aPage); }

  // Returns PR_TRUE if we had a buffered page and we successfully inserted it
  // into the bitstream.
  PRBool PageInFromBuffer();

public:

  // Number of packets read.  
  PRUint64 mPacketCount;

  // Serial number of the bitstream.
  PRUint32 mSerial;

  // Ogg specific state.
  ogg_stream_state mState;

  // Buffer of pages which we've not yet inserted into the ogg_stream_state.
  nsPageQueue mBuffer;

  // Is the bitstream active; whether we're decoding and playing this bitstream.
  PRPackedBool mActive;
  
  // PR_TRUE when all headers packets have been read.
  PRPackedBool mDoneReadingHeaders;
};

class nsVorbisState : public nsOggCodecState {
public:
  nsVorbisState(ogg_page* aBosPage);
  virtual ~nsVorbisState();

  virtual CodecType GetType() { return TYPE_VORBIS; }
  virtual PRBool DecodeHeader(ogg_packet* aPacket);
  virtual PRInt64 Time(PRInt64 granulepos);
  virtual PRBool Init();
  virtual nsresult Reset();

  vorbis_info mInfo;
  vorbis_comment mComment;
  vorbis_dsp_state mDsp;
  vorbis_block mBlock;
};

class nsTheoraState : public nsOggCodecState {
public:
  nsTheoraState(ogg_page* aBosPage);
  virtual ~nsTheoraState();

  virtual CodecType GetType() { return TYPE_THEORA; }
  virtual PRBool DecodeHeader(ogg_packet* aPacket);
  virtual PRInt64 Time(PRInt64 granulepos);
  virtual PRInt64 StartTime(PRInt64 granulepos);
  virtual PRBool Init();

  // Returns the maximum number of milliseconds which a keyframe can be offset
  // from any given interframe.
  PRInt64 MaxKeyframeOffset();

  th_info mInfo;
  th_comment mComment;
  th_setup_info *mSetup;
  th_dec_ctx* mCtx;

  // Frame duration in ms.
  PRUint32 mFrameDuration;

  // Number of frames per second.
  float mFrameRate;

  float mAspectRatio;
};

class nsSkeletonState : public nsOggCodecState {
public:
  nsSkeletonState(ogg_page* aBosPage);
  virtual ~nsSkeletonState();
  virtual CodecType GetType() { return TYPE_SKELETON; }
  virtual PRBool DecodeHeader(ogg_packet* aPacket);
  virtual PRInt64 Time(PRInt64 granulepos) { return -1; }
  virtual PRBool Init() { return PR_TRUE; }
};

#endif
