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
#include "nsAString.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "prlog.h"
#include "nsOggDecoder.h"
#include "nsChannelReader.h"
#include "nsIScriptSecurityManager.h"

// The minimum size buffered byte range inside which we'll consider
// trying a bounded-seek. When we seek, we first try to seek inside all
// buffered ranges larger than this, and if they all fail we fall back to
// an unbounded seek over the whole media. 64K is approximately 16 pages.
#define MIN_BOUNDED_SEEK_SIZE (64 * 1024)

OggPlayErrorCode nsChannelReader::initialise(int aBlock)
{
  return E_OGGPLAY_OK;
}

OggPlayErrorCode nsChannelReader::destroy()
{
  // We don't have to do anything here, the decoder will clean stuff up
  return E_OGGPLAY_OK;
}

void nsChannelReader::SetLastFrameTime(PRInt64 aTime)
{
  mLastFrameTime = aTime;
}

size_t nsChannelReader::io_read(char* aBuffer, size_t aCount)
{
  PRUint32 bytes = 0;
  nsresult rv = mStream->Read(aBuffer, aCount, &bytes);
  if (!NS_SUCCEEDED(rv)) {
    return static_cast<size_t>(OGGZ_ERR_SYSTEM);
  }
  nsOggDecoder* decoder =
    static_cast<nsOggDecoder*>(mStream->Decoder());
  decoder->NotifyBytesConsumed(bytes);
  return bytes;
}

int nsChannelReader::io_seek(long aOffset, int aWhence)
{
  nsresult rv = mStream->Seek(aWhence, aOffset);
  if (NS_SUCCEEDED(rv))
    return aOffset;
  
  return OGGZ_STOP_ERR;
}

long nsChannelReader::io_tell()
{
  return mStream->Tell();
}

ogg_int64_t nsChannelReader::duration()
{
  return mLastFrameTime;
}

static OggPlayErrorCode oggplay_channel_reader_initialise(OggPlayReader* aReader, int aBlock) 
{
  nsChannelReader * me = static_cast<nsChannelReader*>(aReader);

  if (me == NULL) {
    return E_OGGPLAY_BAD_READER;
  }
  return me->initialise(aBlock);
}

static OggPlayErrorCode oggplay_channel_reader_destroy(OggPlayReader* aReader) 
{
  nsChannelReader* me = static_cast<nsChannelReader*>(aReader);
  return me->destroy();
}

static size_t oggplay_channel_reader_io_read(void* aReader, void* aBuffer, size_t aCount) 
{
  nsChannelReader* me = static_cast<nsChannelReader*>(aReader);
  return me->io_read(static_cast<char*>(aBuffer), aCount);
}

static int oggplay_channel_reader_io_seek(void* aReader, long aOffset, int aWhence) 
{
  nsChannelReader* me = static_cast<nsChannelReader*>(aReader);
  return me->io_seek(aOffset, aWhence);
}

static long oggplay_channel_reader_io_tell(void* aReader) 
{
  nsChannelReader* me = static_cast<nsChannelReader*>(aReader);
  return me->io_tell();
}

static ogg_int64_t oggplay_channel_reader_duration(struct _OggPlayReader *aReader)
{
  nsChannelReader* me = static_cast<nsChannelReader*>(aReader);
  return me->duration();
}

class ByteRange {
public:
  ByteRange() : mStart(-1), mEnd(-1) {}
  ByteRange(PRInt64 aStart, PRInt64 aEnd) : mStart(aStart), mEnd(aEnd) {}
  PRInt64 mStart, mEnd;
};

static void GetBufferedBytes(nsMediaStream* aStream, nsTArray<ByteRange>& aRanges)
{
  PRInt64 startOffset = 0;
  while (PR_TRUE) {
    PRInt64 endOffset = aStream->GetCachedDataEnd(startOffset);
    if (endOffset == startOffset) {
      // Uncached at startOffset.
      endOffset = aStream->GetNextCachedData(startOffset);
      if (endOffset == -1) {
        // Uncached at startOffset until endOffset of stream, or we're at
        // the end of stream.
        break;
      }
    } else {
      // Bytes [startOffset..endOffset] are cached.
      PRInt64 cachedLength = endOffset - startOffset;
      // Only bother trying to seek inside ranges greater than
      // MIN_BOUNDED_SEEK_SIZE, so that the bounded seek is unlikely to
      // read outside of the range when finding Ogg page boundaries.
      if (cachedLength > MIN_BOUNDED_SEEK_SIZE) {
        aRanges.AppendElement(ByteRange(startOffset, endOffset));
      }
    }
    startOffset = endOffset;
  }
}

OggPlayErrorCode oggplay_channel_reader_seek(struct _OggPlayReader *me,
                                             OGGZ *oggz, 
                                             ogg_int64_t aTargetMs)
{
  nsChannelReader* reader = static_cast<nsChannelReader*>(me);
  nsMediaStream* stream = reader->Stream(); 
  nsAutoTArray<ByteRange, 16> ranges;
  stream->Pin();
  GetBufferedBytes(stream, ranges);
  PRInt64 rv = -1;
  for (PRUint32 i = 0; rv == -1 && i < ranges.Length(); i++) {
    rv = oggz_bounded_seek_set(oggz,
                               aTargetMs,
                               ranges[i].mStart,
                               ranges[i].mEnd);
  }
  stream->Unpin();

  if (rv == -1) {
    // Could not seek in a buffered range, fall back to seeking over the
    // entire media.
    rv = oggz_bounded_seek_set(oggz,
                               aTargetMs,
                               0,
                               stream->GetLength());
  }
  return (rv == -1) ? E_OGGPLAY_CANT_SEEK : E_OGGPLAY_OK;
  
}

nsresult nsChannelReader::Init(nsMediaDecoder* aDecoder, nsIURI* aURI,
                               nsIChannel* aChannel,
                               nsIStreamListener** aStreamListener)
{
  return nsMediaStream::Open(aDecoder, aURI, aChannel,
                             getter_Transfers(mStream), aStreamListener);
}

nsChannelReader::~nsChannelReader()
{
  MOZ_COUNT_DTOR(nsChannelReader);
}

nsChannelReader::nsChannelReader() :
  mLastFrameTime(-1)
{
  MOZ_COUNT_CTOR(nsChannelReader);
  OggPlayReader* reader = this;
  reader->initialise = &oggplay_channel_reader_initialise;
  reader->destroy = &oggplay_channel_reader_destroy;
  reader->seek = &oggplay_channel_reader_seek;
  reader->io_read  = &oggplay_channel_reader_io_read;
  reader->io_seek  = &oggplay_channel_reader_io_seek;
  reader->io_tell  = &oggplay_channel_reader_io_tell;
  reader->duration = &oggplay_channel_reader_duration;
}
