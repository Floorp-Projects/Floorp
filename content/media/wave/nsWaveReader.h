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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Matthew Gregan <kinetik@flim.org>
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
#if !defined(nsWaveReader_h_)
#define nsWaveReader_h_

#include "nsBuiltinDecoderReader.h"

class nsBuiltinDecoder;
class nsTimeRanges;

class nsWaveReader : public nsBuiltinDecoderReader
{
public:
  nsWaveReader(nsBuiltinDecoder* aDecoder);
  ~nsWaveReader();

  virtual nsresult Init(nsBuiltinDecoderReader* aCloneDonor);
  virtual bool DecodeAudioData();
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                  PRInt64 aTimeThreshold);

  virtual bool HasAudio()
  {
    return true;
  }

  virtual bool HasVideo()
  {
    return false;
  }

  virtual nsresult ReadMetadata(nsVideoInfo* aInfo);
  virtual nsresult Seek(PRInt64 aTime, PRInt64 aStartTime, PRInt64 aEndTime, PRInt64 aCurrentTime);
  virtual nsresult GetBuffered(nsTimeRanges* aBuffered, PRInt64 aStartTime);

private:
  bool ReadAll(char* aBuf, PRInt64 aSize, PRInt64* aBytesRead = nsnull);
  bool LoadRIFFChunk();
  bool ScanForwardUntil(PRUint32 aWantedChunk, PRUint32* aChunkSize);
  bool LoadFormatChunk();
  bool FindDataOffset();

  // Returns the number of seconds that aBytes represents based on the
  // current audio parameters.  e.g.  176400 bytes is 1 second at 16-bit
  // stereo 44.1kHz. The time is rounded to the nearest microsecond.
  double BytesToTime(PRInt64 aBytes) const;

  // Returns the number of bytes that aTime represents based on the current
  // audio parameters.  e.g.  1 second is 176400 bytes at 16-bit stereo
  // 44.1kHz.
  PRInt64 TimeToBytes(double aTime) const;

  // Rounds aBytes down to the nearest complete audio frame.  Assumes
  // beginning of byte range is already frame aligned by caller.
  PRInt64 RoundDownToFrame(PRInt64 aBytes) const;
  PRInt64 GetDataLength();
  PRInt64 GetPosition();

  /*
    Metadata extracted from the WAVE header.  Used to initialize the audio
    stream, and for byte<->time domain conversions.
  */

  // Number of samples per second.  Limited to range [100, 96000] in LoadFormatChunk.
  PRUint32 mSampleRate;

  // Number of channels.  Limited to range [1, 2] in LoadFormatChunk.
  PRUint32 mChannels;

  // Size of a single audio frame, which includes a sample for each channel
  // (interleaved).
  PRUint32 mFrameSize;

  // The sample format of the PCM data.
  nsAudioStream::SampleFormat mSampleFormat;

  // Size of PCM data stored in the WAVE as reported by the data chunk in
  // the media.
  PRInt64 mWaveLength;

  // Start offset of the PCM data in the media stream.  Extends mWaveLength
  // bytes.
  PRInt64 mWavePCMOffset;
};

#endif
