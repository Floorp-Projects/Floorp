/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/FileAPI/#blob
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

typedef (BufferSource or Blob or USVString) BlobPart;

[Exposed=(Window,Worker)]
interface Blob {
  [Throws]
  constructor(optional sequence<BlobPart> blobParts,
              optional BlobPropertyBag options = {});

  [GetterThrows]
  readonly attribute unsigned long long size;

  readonly attribute DOMString type;

  //slice Blob into byte-ranged chunks

  [Throws]
  Blob slice(optional [Clamp] long long start,
             optional [Clamp] long long end,
             optional DOMString contentType);

  // read from the Blob.
  [NewObject] ReadableStream stream();
  [NewObject] Promise<USVString> text();
  [NewObject] Promise<ArrayBuffer> arrayBuffer();
};

enum EndingTypes { "transparent", "native" };

dictionary BlobPropertyBag {
  DOMString type = "";
  EndingTypes endings = "transparent";
};

partial interface Blob {
  // This returns the type of BlobImpl used for this Blob.
  [ChromeOnly]
  readonly attribute DOMString blobImplType;
};

