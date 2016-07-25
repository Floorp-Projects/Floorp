/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origins of this IDL file are
 * http://url.spec.whatwg.org/#api
 * http://dev.w3.org/2006/webapi/FileAPI/#creating-revoking
 * http://dev.w3.org/2011/webrtc/editor/getusermedia.html#url
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

// [Constructor(DOMString url, optional (URL or DOMString) base = "about:blank")]
[Constructor(DOMString url, URL base),
 Constructor(DOMString url, optional DOMString base),
 Exposed=(Window,Worker,WorkerDebugger)]
interface URL {
  // Bug 824857: no support for stringifier attributes yet.
  //  stringifier attribute USVString href;

  // Bug 824857 should remove this.
  [Throws]
  stringifier;

  [Throws]
  attribute USVString href;
  [Throws]
  readonly attribute USVString origin;
  [Throws]
           attribute USVString protocol;
  [Throws]
           attribute USVString username;
  [Throws]
           attribute USVString password;
  [Throws]
           attribute USVString host;
  [Throws]
           attribute USVString hostname;
  [Throws]
           attribute USVString port;
  [Throws]
           attribute USVString pathname;
  [Throws]
           attribute USVString search;
  readonly attribute URLSearchParams searchParams;
  [Throws]
           attribute USVString hash;
};

partial interface URL {
  [Throws]
  static DOMString? createObjectURL(Blob blob, optional objectURLOptions options);
  [Throws]
  static DOMString? createObjectURL(MediaStream stream, optional objectURLOptions options);
  [Throws]
  static void revokeObjectURL(DOMString url);
  [ChromeOnly, Throws]
  static boolean isValidURL(DOMString url);
};

dictionary objectURLOptions
{
/* boolean autoRevoke = true; */ /* not supported yet */
};

// https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html
partial interface URL {
  [Throws]
  static DOMString? createObjectURL(MediaSource source, optional objectURLOptions options);
};
