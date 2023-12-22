/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webcodecs/#videoencoder
 *
 * Some members of this API are codec-specific, in which case the source of the
 * IDL are in the codec-specific registry entries, that are listed in
 * https://w3c.github.io/webcodecs/codec_registry.html. Those members are
 * commented with a link of the document in which the member is listed.
 */

[Exposed=(Window,DedicatedWorker), SecureContext, Pref="dom.media.webcodecs.enabled"]
interface VideoEncoder : EventTarget {
  [Throws]
  constructor(VideoEncoderInit init);

  readonly attribute CodecState state;
  readonly attribute unsigned long encodeQueueSize;
  attribute EventHandler ondequeue;

  [Throws]
  undefined configure(VideoEncoderConfig config);
  [Throws, BinaryName="VideoEncoder::EncodeVideoFrame"]
  undefined encode(VideoFrame frame , optional VideoEncoderEncodeOptions options = {});
  [Throws]
  Promise<undefined> flush();
  [Throws]
  undefined reset();
  [Throws]
  undefined close();

  [NewObject, Throws]
  static Promise<VideoEncoderSupport> isConfigSupported(VideoEncoderConfig config);
};

dictionary VideoEncoderInit {
  required EncodedVideoChunkOutputCallback output;
  required WebCodecsErrorCallback error;
};

callback EncodedVideoChunkOutputCallback =
    undefined (EncodedVideoChunk chunk,
               optional EncodedVideoChunkMetadata metadata = {});

// AVC (H264)-specific
// https://w3c.github.io/webcodecs/avc_codec_registration.html
enum AvcBitstreamFormat {
  "annexb",
  "avc",
};

// AVC (H264)-specific
// https://w3c.github.io/webcodecs/avc_codec_registration.html
dictionary AvcEncoderConfig {
  AvcBitstreamFormat format = "avc";
};

dictionary VideoEncoderConfig {
  required DOMString codec;
  required [EnforceRange] unsigned long width;
  required [EnforceRange] unsigned long height;
  [EnforceRange] unsigned long displayWidth;
  [EnforceRange] unsigned long displayHeight;
  [EnforceRange] unsigned long long bitrate;
  double framerate;
  HardwareAcceleration hardwareAcceleration = "no-preference";
  AlphaOption alpha = "discard";
  DOMString scalabilityMode;
  VideoEncoderBitrateMode bitrateMode = "variable";
  LatencyMode latencyMode = "quality";
  DOMString contentHint;
  // AVC (H264)-specific
  // https://w3c.github.io/webcodecs/avc_codec_registration.html
  AvcEncoderConfig avc;
};

dictionary VideoEncoderEncodeOptions {
  boolean keyFrame = false;
  // AVC (H264)-specific
  // https://w3c.github.io/webcodecs/avc_codec_registration.html
  VideoEncoderEncodeOptionsForAvc avc;
};

// AVC (H264)-specific
// https://w3c.github.io/webcodecs/avc_codec_registration.html
dictionary VideoEncoderEncodeOptionsForAvc {
  unsigned short? quantizer;
};

enum VideoEncoderBitrateMode {
  "constant",
  "variable",
  // AVC (H264)-specific
  // https://w3c.github.io/webcodecs/avc_codec_registration.html
  "quantizer"
};

enum LatencyMode {
  "quality",
  "realtime"
};

dictionary VideoEncoderSupport {
  boolean supported;
  VideoEncoderConfig config;
};

dictionary EncodedVideoChunkMetadata {
  VideoDecoderConfig decoderConfig;
  SvcOutputMetadata svc;
  // Not implemented https://bugzilla.mozilla.org/show_bug.cgi?id=1867067
  // BufferSource alphaSideData;
};

dictionary SvcOutputMetadata {
  unsigned long temporalLayerId;
};
