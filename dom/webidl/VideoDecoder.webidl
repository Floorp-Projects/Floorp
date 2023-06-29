/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webcodecs/#videodecoder
 */

[Exposed=(Window,DedicatedWorker), SecureContext, Pref="dom.media.webcodecs.enabled"]
interface VideoDecoder : EventTarget {
  [Throws]
  constructor(VideoDecoderInit init);

  readonly attribute CodecState state;
  readonly attribute unsigned long decodeQueueSize;
  attribute EventHandler ondequeue;

  [Throws]
  undefined configure(VideoDecoderConfig config);
  [Throws]
  undefined decode(EncodedVideoChunk chunk);
  [NewObject, Throws]
  Promise<undefined> flush();
  [Throws]
  undefined reset();
  [Throws]
  undefined close();

  [NewObject, Throws]
  static Promise<VideoDecoderSupport> isConfigSupported(VideoDecoderConfig config);
};

dictionary VideoDecoderInit {
  required VideoFrameOutputCallback output;
  required WebCodecsErrorCallback error;
};

callback VideoFrameOutputCallback = undefined(VideoFrame output);

dictionary VideoDecoderSupport {
  boolean supported;
  VideoDecoderConfig config;
};

dictionary VideoDecoderConfig {
  required DOMString codec;
  // Bug 1696216: Should be 1696216 [AllowShared] BufferSource description;
  ([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer) description;
  [EnforceRange] unsigned long codedWidth;
  [EnforceRange] unsigned long codedHeight;
  [EnforceRange] unsigned long displayAspectWidth;
  [EnforceRange] unsigned long displayAspectHeight;
  VideoColorSpaceInit colorSpace;
  HardwareAcceleration hardwareAcceleration = "no-preference";
  boolean optimizeForLatency;
};

enum HardwareAcceleration {
  "no-preference",
  "prefer-hardware",
  "prefer-software",
};

enum CodecState {
  "unconfigured",
  "configured",
  "closed"
};

callback WebCodecsErrorCallback = undefined(DOMException error);
