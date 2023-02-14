/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * https://html.spec.whatwg.org/#the-offscreencanvas-interface
 */

typedef (OffscreenCanvasRenderingContext2D or ImageBitmapRenderingContext or WebGLRenderingContext or WebGL2RenderingContext or GPUCanvasContext) OffscreenRenderingContext;

dictionary ImageEncodeOptions {
  DOMString type = "image/png";
  unrestricted double quality;
};

enum OffscreenRenderingContextId { "2d", "bitmaprenderer", "webgl", "webgl2", "webgpu" };

[Exposed=(Window,Worker), Pref="gfx.offscreencanvas.enabled"]
interface OffscreenCanvas : EventTarget {
  [Throws]
  constructor([EnforceRange] unsigned long width, [EnforceRange] unsigned long height);

  [Pure, SetterThrows]
  attribute [EnforceRange] unsigned long width;
  [Pure, SetterThrows]
  attribute [EnforceRange] unsigned long height;

  [Throws]
  OffscreenRenderingContext? getContext(OffscreenRenderingContextId contextId,
                                        optional any contextOptions = null);

  [Throws]
  ImageBitmap transferToImageBitmap();
  [NewObject]
  Promise<Blob> convertToBlob(optional ImageEncodeOptions options = {});

  attribute EventHandler oncontextlost;
  attribute EventHandler oncontextrestored;

  // Deprecated by convertToBlob
  [Deprecated="OffscreenCanvasToBlob", NewObject]
  Promise<Blob> toBlob(optional DOMString type = "",
                       optional any encoderOptions);
};
