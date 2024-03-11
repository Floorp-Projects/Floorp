/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webcodecs/#audiodata
 */

// [Serializable, Transferable] are implemented without adding attributes here,
// but directly with {Read,Write}StructuredClone and Transfer/FromTransfered.
[Exposed=(Window,DedicatedWorker), Pref="dom.media.webcodecs.enabled"]
interface AudioData {
  [Throws]
  constructor(AudioDataInit init);

  readonly attribute AudioSampleFormat? format;
  readonly attribute float sampleRate;
  readonly attribute unsigned long numberOfFrames;
  readonly attribute unsigned long numberOfChannels;
  readonly attribute unsigned long long duration;  // microseconds
  readonly attribute long long timestamp;          // microseconds

  [Throws]
  unsigned long allocationSize(AudioDataCopyToOptions options);
  [Throws]
  undefined copyTo(
      // bug 1696216: Should be `copyTo(AllowSharedBufferSource destination, ...)`
      ([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer) destination,
       AudioDataCopyToOptions options);
  [Throws]
  AudioData clone();
  undefined close();
};

dictionary AudioDataInit {
  required AudioSampleFormat format;
  required float sampleRate;
  required [EnforceRange] unsigned long numberOfFrames;
  required [EnforceRange] unsigned long numberOfChannels;
  required [EnforceRange] long long timestamp;  // microseconds
  // bug 1696216: Should be AllowSharedBufferSource
  required ([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer) data;
  sequence<ArrayBuffer> transfer = [];
};

enum AudioSampleFormat {
  "u8",
  "s16",
  "s32",
  "f32",
  "u8-planar",
  "s16-planar",
  "s32-planar",
  "f32-planar",
};

dictionary AudioDataCopyToOptions {
  required [EnforceRange] unsigned long planeIndex;
  [EnforceRange] unsigned long frameOffset = 0;
  [EnforceRange] unsigned long frameCount;
  AudioSampleFormat format;
};
