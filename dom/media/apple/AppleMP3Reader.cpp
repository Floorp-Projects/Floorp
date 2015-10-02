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
// go.  Carefully select this to work well with both the mp3 1152 max frames
// per block and power-of-2 allocation sizes.  Since we must pre-allocate the
// buffer we cannot use AudioCompactor without paying for an additional
// allocation and copy.  Therefore, choosing a value that divides exactly into
// 1152 is most memory efficient.
#define MAX_AUDIO_FRAMES 128

using namespace mozilla::media;

namespace mozilla {

extern PRLogModuleInfo* gMediaDecoderLog;
#define LOGE(...) MOZ_LOG(gMediaDecoderLog, mozilla::LogLevel::Error, (__VA_ARGS__))
#define LOGW(...) MOZ_LOG(gMediaDecoderLog, mozilla::LogLevel::Warning, (__VA_ARGS__))
#define LOGD(...) MOZ_LOG(gMediaDecoderLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

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
  , mResource(mDecoder->GetResource())
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
 */
nsresult
AppleMP3Reader::Read(uint32_t *aNumBytes, char *aData)
{
  nsresult rv = mResource.Read(aData, *aNumBytes, aNumBytes);

  if (NS_FAILED(rv)) {
    *aNumBytes = 0;
    return NS_ERROR_FAILURE;
  }

  // XXX Maybe return a better value than NS_ERROR_FAILURE?
  return *aNumBytes ? NS_OK : NS_ERROR_FAILURE;
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
  if (!aPacketDesc) {
    return kAudioFileStreamError_UnspecifiedError;
  }

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
  uint32_t decodedSize = MAX_AUDIO_FRAMES * mAudioChannels *
                         sizeof(AudioDataValue);

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

    // If we decoded zero frames then AudiOConverterFillComplexBuffer is out
    // of data to provide.  We drained its internal buffer completely on the
    // last pass.
    if (numFrames == 0 && rv == kNeedMoreData) {
      LOGD("FillComplexBuffer out of data exactly\n");
      break;
    }

    int64_t time = FramesToUsecs(mCurrentAudioFrame, mAudioSampleRate).value();
    int64_t duration = FramesToUsecs(numFrames, mAudioSampleRate).value();

    LOGD("pushed audio at time %lfs; duration %lfs\n",
         (double)time / USECS_PER_S, (double)duration / USECS_PER_S);

    AudioData *audio = new AudioData(mResource.Tell(),
                                     time, duration, numFrames,
                                     reinterpret_cast<AudioDataValue *>(decoded.forget()),
                                     mAudioChannels, mAudioSampleRate);
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
  MOZ_ASSERT(OnTaskQueue());

  // Read AUDIO_READ_BYTES if we can
  char bytes[AUDIO_READ_BYTES];
  uint32_t numBytes = AUDIO_READ_BYTES;

  nsresult readrv = Read(&numBytes, bytes);

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
  MOZ_ASSERT(OnTaskQueue());
  return false;
}


bool
AppleMP3Reader::HasAudio()
{
  MOZ_ASSERT(OnTaskQueue());
  return mStreamReady;
}

bool
AppleMP3Reader::HasVideo()
{
  MOZ_ASSERT(OnTaskQueue());
  return false;
}

bool
AppleMP3Reader::IsMediaSeekable()
{
  // not used
  return true;
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
AppleMP3Reader::ReadMetadata(MediaInfo* aInfo,
                             MetadataTags** aTags)
{
  MOZ_ASSERT(OnTaskQueue());

  *aTags = nullptr;

  /*
   * Feed bytes into the parser until we have all the metadata we need to
   * set up the decoder. When the parser has enough data, it will
   * synchronously call back to |AudioMetadataCallback| below.
   */
  OSStatus rv;
  nsresult readrv;
  uint32_t offset = 0;
  do {
    char bytes[AUDIO_READ_BYTES];
    uint32_t numBytes = AUDIO_READ_BYTES;
    readrv = Read(&numBytes, bytes);

    rv = AudioFileStreamParseBytes(mAudioFileStream,
                                   numBytes,
                                   bytes,
                                   0 /* flags */);

    mMP3FrameParser.Parse(reinterpret_cast<uint8_t*>(bytes), numBytes, offset);

    offset += numBytes;

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

  if (!mMP3FrameParser.IsMP3()) {
    LOGE("Frame parser failed to parse MP3 stream\n");
    return NS_ERROR_FAILURE;
  }

  if (mStreamReady) {
    aInfo->mAudio.mRate = mAudioSampleRate;
    aInfo->mAudio.mChannels = mAudioChannels;
  }

  // This special snowflake reader doesn't seem to set *aInfo = mInfo like all
  // the others. Yuck.
  mDuration = mMP3FrameParser.GetDuration();
  mInfo.mMetadataDuration.emplace(TimeUnit::FromMicroseconds(mDuration));
  aInfo->mMetadataDuration.emplace(TimeUnit::FromMicroseconds(mDuration));

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


nsRefPtr<MediaDecoderReader::SeekPromise>
AppleMP3Reader::Seek(int64_t aTime, int64_t aEndTime)
{
  MOZ_ASSERT(OnTaskQueue());

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
    return SeekPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  LOGD("computed byte offset = %lld; estimated = %s\n",
       byteOffset,
       (flags & kAudioFileStreamSeekFlag_OffsetIsEstimated) ? "YES" : "NO");

  mResource.Seek(nsISeekableStream::NS_SEEK_SET, byteOffset);

  ResetDecode();

  return SeekPromise::CreateAndResolve(aTime, __func__);
}

void
AppleMP3Reader::NotifyDataArrivedInternal(uint32_t aLength, int64_t aOffset)
{
  MOZ_ASSERT(OnTaskQueue());
  if (!mMP3FrameParser.NeedsData()) {
    return;
  }

  IntervalSet<int64_t> intervals = mFilter.NotifyDataArrived(aLength, aOffset);
  for (const auto& interval : intervals) {
    nsRefPtr<MediaByteBuffer> bytes =
      mResource.MediaReadAt(interval.mStart, interval.Length());
    NS_ENSURE_TRUE_VOID(bytes);
    mMP3FrameParser.Parse(bytes->Elements(), interval.Length(), interval.mStart);
    if (!mMP3FrameParser.IsMP3()) {
      return;
    }

    uint64_t duration = mMP3FrameParser.GetDuration();
    if (duration != mDuration) {
      LOGD("Updating media duration to %lluus\n", duration);
      MOZ_ASSERT(mDecoder);
      mDuration = duration;
      mDecoder->DispatchUpdateEstimatedMediaDuration(duration);
    }
  }
}

} // namespace mozilla
