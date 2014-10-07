/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/2006/webapi/FileAPI/#blob
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

/*
[Constructor, Constructor((ArrayBuffer or ArrayBufferView or Blob or DOMString)[] blobParts, optional BlobPropertyBag options)] 
interface Blob {

  readonly attribute unsigned long long size;
  readonly attribute DOMString type;

  //slice Blob into byte-ranged chunks

  Blob slice(optional long long start,
             optional long long end,
             optional DOMString contentType);
  void close(); 

};
*/

enum EndingTypes{"transparent", "native"};

dictionary BlobPropertyBag {

  DOMString type = "";
  EndingTypes endings = "transparent";

};
