/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webcodecs/#videoframe
 */

enum AlphaOption {
  "keep",
  "discard",
};

// [Serializable, Transferable] are implemented without adding attributes here.
[Exposed=(Window,DedicatedWorker), Pref="dom.media.webcodecs.enabled"]
interface VideoFrame {
  // The constructors should be shorten to:
  //   ```
  //   constructor([AllowShared] BufferSource data, VideoFrameBufferInit init);
  //   constructor(CanvasImageSource image, optional VideoFrameInit init = {});
  //   ```
  // However, `[AllowShared] BufferSource` doesn't work for now (bug 1696216), and
  // `No support for unions as distinguishing arguments yet` error occurs when using
  //   `constructor(CanvasImageSource image, optional VideoFrameInit init = {})` and
  //   `constructor(([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer) data, VideoFrameBufferInit init)`
  // at the same time (bug 1786410).
  [Throws]
  constructor(HTMLImageElement imageElement, optional VideoFrameInit init = {});
  [Throws]
  constructor(SVGImageElement svgImageElement, optional VideoFrameInit init = {});
  [Throws]
  constructor(HTMLCanvasElement canvasElement, optional VideoFrameInit init = {});
  [Throws]
  constructor(HTMLVideoElement videoElement, optional VideoFrameInit init = {});
  [Throws]
  constructor(OffscreenCanvas offscreenCanvas, optional VideoFrameInit init = {});
  [Throws]
  constructor(ImageBitmap imageBitmap, optional VideoFrameInit init = {});
  [Throws]
  constructor(VideoFrame videoFrame, optional VideoFrameInit init = {});
  [Throws]
  constructor([AllowShared] ArrayBufferView bufferView, VideoFrameBufferInit init);
  [Throws]
  constructor([AllowShared] ArrayBuffer buffer, VideoFrameBufferInit init);


  readonly attribute VideoPixelFormat? format;
  readonly attribute unsigned long codedWidth;
  readonly attribute unsigned long codedHeight;
  readonly attribute DOMRectReadOnly? codedRect;
  readonly attribute DOMRectReadOnly? visibleRect;
  readonly attribute unsigned long displayWidth;
  readonly attribute unsigned long displayHeight;
  readonly attribute unsigned long long? duration;  // microseconds
  readonly attribute long long timestamp;           // microseconds
  readonly attribute VideoColorSpace colorSpace;

  [Throws]
  unsigned long allocationSize(
      optional VideoFrameCopyToOptions options = {});
  [Throws]
  Promise<sequence<PlaneLayout>> copyTo(
      // bug 1696216: Should be `copyTo([AllowShared] BufferSource destination, ...)`
      ([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer) destination,
      optional VideoFrameCopyToOptions options = {});
  [Throws]
  VideoFrame clone();
  undefined close();
};

dictionary VideoFrameInit {
  unsigned long long duration;  // microseconds
  long long timestamp;          // microseconds
  AlphaOption alpha = "keep";

  // Default matches image. May be used to efficiently crop. Will trigger
  // new computation of displayWidth and displayHeight using image’s pixel
  // aspect ratio unless an explicit displayWidth and displayHeight are given.
  DOMRectInit visibleRect;

  // Default matches image unless visibleRect is provided.
  [EnforceRange] unsigned long displayWidth;
  [EnforceRange] unsigned long displayHeight;
};

dictionary VideoFrameBufferInit {
  required VideoPixelFormat format;
  required [EnforceRange] unsigned long codedWidth;
  required [EnforceRange] unsigned long codedHeight;
  required [EnforceRange] long long timestamp;  // microseconds
  [EnforceRange] unsigned long long duration;   // microseconds

  // Default layout is tightly-packed.
  sequence<PlaneLayout> layout;

  // Default visible rect is coded size positioned at (0,0)
  DOMRectInit visibleRect;

  // Default display dimensions match visibleRect.
  [EnforceRange] unsigned long displayWidth;
  [EnforceRange] unsigned long displayHeight;

  VideoColorSpaceInit colorSpace;
};

dictionary VideoFrameCopyToOptions {
  DOMRectInit rect;
  sequence<PlaneLayout> layout;
};

dictionary PlaneLayout {
  // TODO: https://github.com/w3c/webcodecs/pull/488
  required [EnforceRange] unsigned long offset;
  required [EnforceRange] unsigned long stride;
};

enum VideoPixelFormat {
  // 4:2:0 Y, U, V
  "I420",
  // 4:2:0 Y, U, V, A
  "I420A",
  // 4:2:2 Y, U, V
  "I422",
  // 4:4:4 Y, U, V
  "I444",
  // 4:2:0 Y, UV
  "NV12",
  // 32bpp RGBA
  "RGBA",
  // 32bpp RGBX (opaque)
  "RGBX",
  // 32bpp BGRA
  "BGRA",
  // 32bpp BGRX (opaque)
  "BGRX",
};
