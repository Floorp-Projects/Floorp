/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webcodecs/#encodedvideochunk
 */

// [Serializable] is implemented without adding attribute here.
[Exposed=(Window,DedicatedWorker), Pref="dom.media.webcodecs.enabled"]
interface EncodedVideoChunk {
  [Throws]
  constructor(EncodedVideoChunkInit init);
  readonly attribute EncodedVideoChunkType type;
  readonly attribute long long timestamp;             // microseconds
  readonly attribute unsigned long long? duration;    // microseconds
  readonly attribute unsigned long byteLength;

  // bug 1696216: Should be `copyTo([AllowShared] BufferSource destination)`
  [Throws]
  undefined copyTo(([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer) destination);
};

dictionary EncodedVideoChunkInit {
  required EncodedVideoChunkType type;
  required [EnforceRange] long long timestamp;        // microseconds
  [EnforceRange] unsigned long long duration;         // microseconds
  // bug 1696216: Should be `required BufferSource data`
  required ([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer) data;
};

enum EncodedVideoChunkType {
    "key",
    "delta",
};
