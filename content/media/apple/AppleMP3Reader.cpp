/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleMP3Reader.h"

#include "nsISeekableStream.h"
#include "MediaDecoder.h"

// Number of bytes we will read and pass to the audio parser in each
// |DecodeAudioData| call.
#define AUDIO_READ_BYTES 4096

// Maximum number of audio frames we will accept from the audio decoder in one
// go.
#define MAX_AUDIO_FRAMES 4096

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define LOGE(...) PR_LOG(gMediaDecoderLog, PR_LOG_ERROR, (__VA_ARGS__))
#define LOGW(...) PR_LOG(gMediaDecoderLog, PR_LOG_WARNING, (__VA_ARGS__))
#define LOGD(...) PR_LOG(gMediaDecoderLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOGE(...)
#define LOGW(...)
#define LOGD(...)
#endif

#define PROPERTY_ID_FORMAT "%c%c%c%c"
#define PROPERTY_ID_PRINT(x) ((x) >> 24), \
                             ((x) >> 16) & 0xff, \
                             ((x) >> 8) & 0xff, \
                             (x) & 0xff

AppleMP3Reader::AppleMP3Reader(AbstractMediaDecoder *aDecoder)
  : MediaDecoderReader(aDecoder)
  , mStreamReady(false)
  , mAudioFramesPerCompressedPacket(0)
  , mCurrentAudioFrame(0)
  , mAudioChannels(0)
  , mAudioSampleRate(0)
  , mAudioFileStream(nullptr)
  , mAudioConverter(nullptr)
  , mMP3FrameParser(mDecoder->GetResource()->GetLength())
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread");
}

AppleMP3Reader::~AppleMP3Reader()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread");
}


/*
 * The Apple audio decoding APIs are very callback-happy. When the parser has
 * some metadata, it will call back to here.
 */
static void _AudioMetadataCallback(void *aThis,
                                   AudioFileStreamID aFileStream,
                                   AudioFileStreamPropertyID aPropertyID,
                                   UInt32 *aFlags)
{
  ((AppleMP3Reader*)aThis)->AudioMetadataCallback(aFileStream, aPropertyID,
                                                  aFlags);
}

/*
 * Similar to above, this is called when the parser has enough data to parse
 * one or more samples.
 */
static void _AudioSampleCallback(void *aThis,
                                 UInt32 aNumBytes, UInt32 aNumPackets,
                                 const void *aData,
                                 AudioStreamPacketDescription *aPackets)
{
  ((AppleMP3Reader*)aThis)->AudioSampleCallback(aNumBytes, aNumPackets,
                                                aData, aPackets);
}


/*
 * If we're not at end of stream, read |aNumBytes| from the media resource,
 * put it in |aData|, and return true.
 * Otherwise, put as much data as is left into |aData|, set |aNumBytes| to the
 * amount of data we have left, and return false.
 *
 * This function also calls NotifyBytesConsumed() on the media resource and
 * passes the read data on to the MP3 frame parser for stream duration
 * estimation.
 */
nsresult
AppleMP3Reader::ReadAndNotify(uint32_t *aNumBytes, char *aData)
{
  MediaResource *resource = mDecoder->GetResource();

  uint64_t offset = resource->Tell();

  // Loop until we have all the data asked for, or we've reached EOS
  uint32_t totalBytes = 0;
  uint32_t numBytes;
  do {
    uint32_t bytesWanted = *aNumBytes - totalBytes;
    nsresult rv = resource->Read(aData + totalBytes, bytesWanted, &numBytes);
    totalBytes += numBytes;

    if (NS_FAILED(rv)) {
      *aNumBytes = 0;
      return NS_ERROR_FAILURE;
    }
  } while(totalBytes < *aNumBytes && numBytes);

  mDecoder->NotifyBytesConsumed(totalBytes);

  // Pass the buffer to the MP3 frame parser to improve our duration estimate.
  if (mMP3FrameParser.IsMP3()) {
    mMP3FrameParser.Parse(aData, totalBytes, offset);
    uint64_t duration = mMP3FrameParser.GetDuration();
    if (duration != mDuration) {
      LOGD("Updating media duration to %lluus\n", duration);
      mDuration = duration;
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mDecoder->UpdateEstimatedMediaDuration(duration);
    }
  }

  *aNumBytes = totalBytes;

  // We will have read some data in the last iteration iff we filled the buffer.
  // XXX Maybe return a better value than NS_ERROR_FAILURE?
  return numBytes ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
AppleMP3Reader::Init(MediaDecoderReader* aCloneDonor)
{
  AudioFileTypeID fileType = kAudioFileMP3Type;

  OSStatus rv = AudioFileStreamOpen(this,
                                    _AudioMetadataCallback,
                                    _AudioSampleCallback,
                                    fileType,
                                    &mAudioFileStream);

  if (rv) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


struct PassthroughUserData {
  AppleMP3Reader *mReader;
  UInt32 mNumPackets;
  UInt32 mDataSize;
  const void *mData;
  AudioStreamPacketDescription *mPacketDesc;
  bool mDone;
};

// Error value we pass through the decoder to signal that nothing has gone wrong
// during decoding, but more data is needed.
const UInt32 kNeedMoreData = 'MOAR';

/*
 * This function is called from |AudioConverterFillComplexBuffer|, which is
 * called from |AudioSampleCallback| below, which in turn is called by
 * |AudioFileStreamParseBytes|, which is called by |DecodeAudioData|.
 *
 * Mercifully, this is all synchronous.
 *
 * This callback is run when the AudioConverter (decoder) wants more MP3 packets
 * to decode.
 */
/* static */ OSStatus
AppleMP3Reader::PassthroughInputDataCallback(AudioConverterRef aAudioConverter,
                                             UInt32 *aNumDataPackets /* in/out */,
                                             AudioBufferList *aData /* in/out */,
                                             AudioStreamPacketDescription **aPacketDesc,
                                             void *aUserData)
{
  PassthroughUserData *userData = (PassthroughUserData *)aUserData;
  if (userData->mDone) {
    // We make sure this callback is run _once_, with all the data we received
    // from |AudioFileStreamParseBytes|. When we return an error, the decoder
    // simply passes the return value on to the calling method,
    // |AudioSampleCallback|; and flushes all of the audio frames it had
    // buffered. It does not change the decoder's state.
    LOGD("requested too much data; returning\n");
    *aNumDataPackets = 0;
    return kNeedMoreData;
  }

  userData->mDone = true;

  LOGD("AudioConverter wants %u packets of audio data\n", *aNumDataPackets);

  *aNumDataPackets = userData->mNumPackets;
  *aPacketDesc = userData->mPacketDesc;

  aData->mBuffers[0].mNumberChannels = userData->mReader->mAudioChannels;
  aData->mBuffers[0].mDataByteSize = userData->mDataSize;
  aData->mBuffers[0].mData = const_cast<void *>(userData->mData);

  return 0;
}

/*
 * This callback is called when |AudioFileStreamParseBytes| has enough data to
 * extract one or more MP3 packets.
 */
void
AppleMP3Reader::AudioSampleCallback(UInt32 aNumBytes,
                                    UInt32 aNumPackets,
                                    const void *aData,
                                    AudioStreamPacketDescription *aPackets)
{
  LOGD("got %u bytes, %u packets\n", aNumBytes, aNumPackets);

  // 1 frame per packet * num channels * 32-bit float
  uint32_t decodedSize = MAX_AUDIO_FRAMES * mAudioChannels * 4;

  // descriptions for _decompressed_ audio packets. ignored.
  nsAutoArrayPtr<AudioStreamPacketDescription>
    packets(new AudioStreamPacketDescription[MAX_AUDIO_FRAMES]);

  // This API insists on having MP3 packets spoon-fed to it from a callback.
  // This structure exists only to pass our state and the result of the parser
  // on to the callback above.
  PassthroughUserData userData = { this, aNumPackets, aNumBytes, aData, aPackets, false };

  do {
    // Decompressed audio buffer
    nsAutoArrayPtr<uint8_t> decoded(new uint8_t[decodedSize]);

    AudioBufferList decBuffer;
    decBuffer.mNumberBuffers = 1;
    decBuffer.mBuffers[0].mNumberChannels = mAudioChannels;
    decBuffer.mBuffers[0].mDataByteSize = decodedSize;
    decBuffer.mBuffers[0].mData = decoded.get();

    // in: the max number of packets we can handle from the decoder.
    // out: the number of packets the decoder is actually returning.
    UInt32 numFrames = MAX_AUDIO_FRAMES;

    OSStatus rv = AudioConverterFillComplexBuffer(mAudioConverter,
                                                  PassthroughInputDataCallback,
                                                  &userData,
                                                  &numFrames /* in/out */,
                                                  &decBuffer,
                                                  packets.get());

    if (rv && rv != kNeedMoreData) {
      LOGE("Error decoding audio stream: %x\n", rv);
      break;
    }

    int64_t time = FramesToUsecs(mCurrentAudioFrame, mAudioSampleRate).value();
    int64_t duration = FramesToUsecs(numFrames, mAudioSampleRate).value();

    LOGD("pushed audio at time %lfs; duration %lfs\n",
         (double)time / USECS_PER_S, (double)duration / USECS_PER_S);

    AudioData *audio = new AudioData(mDecoder->GetResource()->Tell(),
                                     time, duration, numFrames,
                                     reinterpret_cast<AudioDataValue *>(decoded.forget()),
                                     mAudioChannels);
    mAudioQueue.Push(audio);

    mCurrentAudioFrame += numFrames;

    if (rv == kNeedMoreData) {
      // No error; we just need more data.
      LOGD("FillComplexBuffer out of data\n");
      break;
    }
  } while (true);
}

bool
AppleMP3Reader::DecodeAudioData()
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread");

  // Read AUDIO_READ_BYTES if we can
  char bytes[AUDIO_READ_BYTES];
  uint32_t numBytes = AUDIO_READ_BYTES;

  nsresult readrv = ReadAndNotify(&numBytes, bytes);

  // This function calls |AudioSampleCallback| above, synchronously, when it
  // finds compressed MP3 frame.
  OSStatus rv = AudioFileStreamParseBytes(mAudioFileStream,
                                          numBytes,
                                          bytes,
                                          0 /* flags */);

  if (NS_FAILED(readrv)) {
    mAudioQueue.Finish();
    return false;
  }

  // DataUnavailable just means there wasn't enough data to demux anything.
  // We should have more to push into the demuxer next time we're called.
  if (rv && rv != kAudioFileStreamError_DataUnavailable) {
    LOGE("AudioFileStreamParseBytes returned unknown error %x", rv);
    return false;
  }

  return true;
}

bool
AppleMP3Reader::DecodeVideoFrame(bool &aKeyframeSkip,
                                 int64_t aTimeThreshold)
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread");
  return false;
}


bool
AppleMP3Reader::HasAudio()
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread");
  return mStreamReady;
}

bool
AppleMP3Reader::HasVideo()
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread");
  return false;
}


/*
 * Query the MP3 parser for a piece of metadata.
 */
static nsresult
GetProperty(AudioFileStreamID aAudioFileStream,
            AudioFileStreamPropertyID aPropertyID, void *aData)
{
  UInt32 size;
  Boolean writeable;
  OSStatus rv = AudioFileStreamGetPropertyInfo(aAudioFileStream, aPropertyID,
                                               &size, &writeable);

  if (rv) {
    LOGW("Couldn't get property " PROPERTY_ID_FORMAT "\n",
         PROPERTY_ID_PRINT(aPropertyID));
    return NS_ERROR_FAILURE;
  }

  rv = AudioFileStreamGetProperty(aAudioFileStream, aPropertyID,
                                  &size, aData);

  return NS_OK;
}


nsresult
AppleMP3Reader::ReadMetadata(VideoInfo* aInfo,
                             MetadataTags** aTags)
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread");

  *aTags = nullptr;

  /*
   * Feed bytes into the parser until we have all the metadata we need to
   * set up the decoder. When the parser has enough data, it will
   * synchronously call back to |AudioMetadataCallback| below.
   */
  OSStatus rv;
  nsresult readrv;
  do {
    char bytes[AUDIO_READ_BYTES];
    uint32_t numBytes = AUDIO_READ_BYTES;
    readrv = ReadAndNotify(&numBytes, bytes);

    rv = AudioFileStreamParseBytes(mAudioFileStream,
                                   numBytes,
                                   bytes,
                                   0 /* flags */);

    // We have to do our decoder setup from the callback. When it's done it will
    // set mStreamReady.
  } while (!mStreamReady && !rv && NS_SUCCEEDED(readrv));

  if (rv) {
    LOGE("Error decoding audio stream metadata\n");
    return NS_ERROR_FAILURE;
  }

  if (!mAudioConverter) {
    LOGE("Failed to setup the AudioToolbox audio decoder\n");
    return NS_ERROR_FAILURE;
  }

  aInfo->mAudioRate = mAudioSampleRate;
  aInfo->mAudioChannels = mAudioChannels;
  aInfo->mHasAudio = mStreamReady;

  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(mDuration);
  }

  return NS_OK;
}


void
AppleMP3Reader::AudioMetadataCallback(AudioFileStreamID aFileStream,
                                      AudioFileStreamPropertyID aPropertyID,
                                      UInt32 *aFlags)
{
  if (aPropertyID == kAudioFileStreamProperty_ReadyToProducePackets) {
    /*
     * The parser is ready to send us packets of MP3 audio.
     *
     * We need to set the decoder up here, because if
     * |AudioFileStreamParseBytes| has enough audio data, then it will call
     * |AudioSampleCallback| before we get back to |ReadMetadata|.
     */
    SetupDecoder();
    mStreamReady = true;
  }
}


void
AppleMP3Reader::SetupDecoder()
{
  // Get input format description from demuxer
  AudioStreamBasicDescription inputFormat, outputFormat;
  GetProperty(mAudioFileStream, kAudioFileStreamProperty_DataFormat, &inputFormat);

  memset(&outputFormat, 0, sizeof(outputFormat));

  // Set output format
#if defined(MOZ_SAMPLE_TYPE_FLOAT32)
  outputFormat.mBitsPerChannel = 32;
  outputFormat.mFormatFlags =
    kLinearPCMFormatFlagIsFloat |
    0;
#else
#error Unknown audio sample type
#endif

  mAudioSampleRate = outputFormat.mSampleRate = inputFormat.mSampleRate;
  mAudioChannels
    = outputFormat.mChannelsPerFrame = inputFormat.mChannelsPerFrame;
  mAudioFramesPerCompressedPacket = inputFormat.mFramesPerPacket;

  outputFormat.mFormatID = kAudioFormatLinearPCM;

  // Set up the decoder so it gives us one sample per frame; this way, it will
  // pass us all the samples it has in one go. Also makes it much easier to
  // deinterlace.
  outputFormat.mFramesPerPacket = 1;
  outputFormat.mBytesPerPacket = outputFormat.mBytesPerFrame
    = outputFormat.mChannelsPerFrame * outputFormat.mBitsPerChannel / 8;

  OSStatus rv = AudioConverterNew(&inputFormat,
                                  &outputFormat,
                                  &mAudioConverter);

  if (rv) {
    LOGE("Error constructing audio format converter: %x\n", rv);
    mAudioConverter = nullptr;
    return;
  }
}


nsresult
AppleMP3Reader::Seek(int64_t aTime,
                     int64_t aStartTime,
                     int64_t aEndTime,
                     int64_t aCurrentTime)
{
  MOZ_ASSERT(mDecoder->OnDecodeThread(), "Should be on decode thread");
  NS_ASSERTION(aStartTime < aEndTime,
               "Seeking should happen over a positive range");

  // Find the exact frame/packet that contains |aTime|.
  mCurrentAudioFrame = aTime * mAudioSampleRate / USECS_PER_S;
  SInt64 packet = mCurrentAudioFrame / mAudioFramesPerCompressedPacket;

  // |AudioFileStreamSeek| will pass back through |byteOffset| the byte offset
  // into the stream it expects next time it reads.
  SInt64 byteOffset;
  UInt32 flags = 0;

  OSStatus rv = AudioFileStreamSeek(mAudioFileStream,
                                    packet,
                                    &byteOffset,
                                    &flags);

  if (rv) {
    LOGE("Couldn't seek demuxer. Error code %x\n", rv);
    return NS_ERROR_FAILURE;
  }

  LOGD("computed byte offset = %lld; estimated = %s\n",
       byteOffset,
       (flags & kAudioFileStreamSeekFlag_OffsetIsEstimated) ? "YES" : "NO");

  mDecoder->GetResource()->Seek(nsISeekableStream::NS_SEEK_SET, byteOffset);

  ResetDecode();

  return NS_OK;
}


nsresult
AppleMP3Reader::GetBuffered(dom::TimeRanges* aBuffered,
                            int64_t aStartTime)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  GetEstimatedBufferedTimeRanges(mDecoder->GetResource(),
                                 mDecoder->GetMediaDuration(),
                                 aBuffered);
  return NS_OK;
}

} // namespace mozilla
