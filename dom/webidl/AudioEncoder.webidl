/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webcodecs/#audioencoder

 * Some members of this API are codec-specific, in which case the source of the
 * IDL are in the codec-specific registry entries, that are listed in
 * https://w3c.github.io/webcodecs/codec_registry.html. Those members are
 * commented with a link of the document in which the member is listed.
 */

dictionary AudioEncoderSupport {
  boolean supported;
  AudioEncoderConfig config;
};

dictionary AudioEncoderConfig {
  required DOMString codec;
  [EnforceRange] unsigned long sampleRate;
  [EnforceRange] unsigned long numberOfChannels;
  [EnforceRange] unsigned long long bitrate;
  BitrateMode bitrateMode = "variable";
  OpusEncoderConfig opus;
};

// Opus specific configuration options:
// https://w3c.github.io/webcodecs/opus_codec_registration.html
enum OpusBitstreamFormat {
  "opus",
  "ogg",
};

dictionary OpusEncoderConfig {
  OpusBitstreamFormat format = "opus";
  [EnforceRange] unsigned long long frameDuration = 20000;
  [EnforceRange] unsigned long complexity;
  [EnforceRange] unsigned long packetlossperc = 0;
  boolean useinbandfec = false;
  boolean usedtx = false;
};

[Exposed=(Window,DedicatedWorker), SecureContext, Pref="dom.media.webcodecs.enabled"]
interface AudioEncoder : EventTarget {
  [Throws]
  constructor(AudioEncoderInit init);

  readonly attribute CodecState state;
  readonly attribute unsigned long encodeQueueSize;
  attribute EventHandler ondequeue;

  [Throws]
  undefined configure(AudioEncoderConfig config);
  [Throws, BinaryName="AudioEncoder::EncodeAudioData"]
  undefined encode(AudioData data);
  [Throws]
  Promise<undefined> flush();
  [Throws]
  undefined reset();
  [Throws]
  undefined close();

  [NewObject, Throws]
  static Promise<AudioEncoderSupport> isConfigSupported(AudioEncoderConfig config);
};

dictionary AudioEncoderInit {
  required EncodedAudioChunkOutputCallback output;
  required WebCodecsErrorCallback error;
};

callback EncodedAudioChunkOutputCallback =
    undefined (EncodedAudioChunk output,
               optional EncodedAudioChunkMetadata metadata = {});

dictionary EncodedAudioChunkMetadata {
  AudioDecoderConfig decoderConfig;
};
