/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/multipage/webappapis.html#images
 */

typedef (HTMLImageElement or
         HTMLVideoElement or
         HTMLCanvasElement or
         Blob or
         ImageData or
         CanvasRenderingContext2D or
         ImageBitmap) ImageBitmapSource;

[Exposed=(Window,Worker)]
interface ImageBitmap {
  [Constant]
  readonly attribute unsigned long width;
  [Constant]
  readonly attribute unsigned long height;
};

[NoInterfaceObject, Exposed=(Window,Worker)]
interface ImageBitmapFactories {
  [Throws]
  Promise<ImageBitmap> createImageBitmap(ImageBitmapSource aImage);
  [Throws]
  Promise<ImageBitmap> createImageBitmap(ImageBitmapSource aImage, long aSx, long aSy, long aSw, long aSh);
};
