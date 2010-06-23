/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * Portions created by the Initial Developer are Copyright (C) 2007
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
#if !defined(nsOggReader_h_)
#define nsOggReader_h_

#include <ogg/ogg.h>
#include <theora/theoradec.h>
#include <vorbis/codec.h>
#include "nsBuiltinDecoderReader.h"
#include "nsOggCodecState.h"

class nsMediaDecoder;

class nsOggReader : public nsBuiltinDecoderReader
{
public:
  nsOggReader(nsBuiltinDecoder* aDecoder);
  ~nsOggReader();

  virtual nsresult Init();
  virtual nsresult ResetDecode();
  virtual PRBool DecodeAudioData();

  // If the Theora granulepos has not been captured, it may read several packets
  // until one with a granulepos has been captured, to ensure that all packets
  // read have valid time info.  
  virtual PRBool DecodeVideoFrame(PRBool &aKeyframeSkip,
                                  PRInt64 aTimeThreshold);
  virtual VideoData* FindStartTime(PRInt64 aOffset,
                                   PRInt64& aOutStartTime);
  virtual PRInt64 FindEndTime(PRInt64 aEndOffset);

  virtual PRBool HasAudio()
  {
    mozilla::MonitorAutoEnter mon(mMonitor);
    return mVorbisState != 0 && mVorbisState->mActive;
  }

  virtual PRBool HasVideo()
  {
    mozilla::MonitorAutoEnter mon(mMonitor);
    return mTheoraState != 0 && mTheoraState->mActive;
  }

  virtual nsresult ReadMetadata();
  virtual nsresult Seek(PRInt64 aTime, PRInt64 aStartTime, PRInt64 aEndTime);

private:
  // Decodes one packet of Vorbis data, storing the resulting chunks of
  // PCM samples in aChunks.
  nsresult DecodeVorbis(nsTArray<SoundData*>& aChunks,
                        ogg_packet* aPacket);

  // May return NS_ERROR_OUT_OF_MEMORY.
  nsresult DecodeTheora(nsTArray<VideoData*>& aFrames,
                        ogg_packet* aPacket);

  // Read a page of data from the Ogg file. Returns the offset of the start
  // of the page, or -1 if the page read failed.
  PRInt64 ReadOggPage(ogg_page* aPage);

  // Read a packet for an Ogg bitstream/codec state. Returns PR_TRUE on
  // success, or PR_FALSE if the read failed.
  PRBool ReadOggPacket(nsOggCodecState* aCodecState, ogg_packet* aPacket);

  // Performs a seek bisection to move the media stream's read cursor to the
  // last ogg page boundary which has end time before aTarget ms on both the
  // Theora and Vorbis bitstreams. Limits its search to data inside aRange;
  // i.e. it will only read inside of the aRange's start and end offsets.
  // aFuzz is the number of ms of leniency we'll allow; we'll terminate the
  // seek when we land in the range (aTime - aFuzz, aTime) ms.
  nsresult SeekBisection(PRInt64 aTarget,
                         const ByteRange& aRange,
                         PRUint32 aFuzz);

private:
  // Maps Ogg serialnos to nsOggStreams.
  nsClassHashtable<nsUint32HashKey, nsOggCodecState> mCodecStates;

  // Decode state of the Theora bitstream we're decoding, if we have video.
  nsTheoraState* mTheoraState;

  // Decode state of the Vorbis bitstream we're decoding, if we have audio.
  nsVorbisState* mVorbisState;

  // Ogg decoding state.
  ogg_sync_state mOggState;

  // The offset of the end of the last page we've read, or the start of
  // the page we're about to read.
  PRInt64 mPageOffset;

  // The granulepos of the last decoded Theora frame.
  PRInt64 mTheoraGranulepos;

  // The granulepos of the last decoded Vorbis sample.
  PRInt64 mVorbisGranulepos;
};

#endif
