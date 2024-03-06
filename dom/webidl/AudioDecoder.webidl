/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webcodecs/#audiodecoder
 */

[Exposed=(Window,DedicatedWorker), SecureContext, Pref="dom.media.webcodecs.enabled"]
interface AudioDecoder : EventTarget {
  [Throws]
  constructor(AudioDecoderInit init);

  readonly attribute CodecState state;
  readonly attribute unsigned long decodeQueueSize;
  attribute EventHandler ondequeue;

  [Throws]
  undefined configure(AudioDecoderConfig config);
  [Throws]
  undefined decode(EncodedAudioChunk chunk);
  [NewObject, Throws]
  Promise<undefined> flush();
  [Throws]
  undefined reset();
  [Throws]
  undefined close();

  [NewObject, Throws]
  static Promise<AudioDecoderSupport> isConfigSupported(AudioDecoderConfig config);
};

dictionary AudioDecoderInit {
  required AudioDataOutputCallback output;
  required WebCodecsErrorCallback error;
};

callback AudioDataOutputCallback = undefined(AudioData output);

dictionary AudioDecoderSupport {
  boolean supported;
  AudioDecoderConfig config;
};

dictionary AudioDecoderConfig {
  required DOMString codec;
  required [EnforceRange] unsigned long sampleRate;
  required [EnforceRange] unsigned long numberOfChannels;

  // Bug 1696216: Should be AllowSharedBufferSource
  ([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer) description;
};
