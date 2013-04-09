#include "MediaDecoder.h"
#include "MediaDecoderReader.h"
#include "MediaDecoderStateMachine.h"

namespace mozilla {

class MediaOmxStateMachine : public MediaDecoderStateMachine
{
public:
  MediaOmxStateMachine(MediaDecoder *aDecoder,
                       MediaDecoderReader *aReader)
    : MediaDecoderStateMachine(aDecoder, aReader) { }

protected:
  // Due to a bug in the OMX.qcom.video.decoder.mpeg4 decoder, we can't own too
  // many video buffers before shutting down the decoder. When we release these
  // buffers, they asynchronously signal to OMXCodec that we have returned
  // ownership of the buffer.
  // If this signal happens while the OMXCodec is shutting down, OMXCodec will
  // crash. If the OMXCodec shuts down before all buffers are returned,
  // OMXCodec will crash.
  // So we need few enough buffers in the queue that all buffers will be
  // returned before OMXCodec begins shutdown.
  uint32_t GetAmpleVideoFrames() { return 1; }
};

} // namespace mozilla
