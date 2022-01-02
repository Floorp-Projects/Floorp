/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origins of this IDL file are
 * http://url.spec.whatwg.org/#api
 * http://dev.w3.org/2006/webapi/FileAPI/#creating-revoking
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Exposed=(Window,Worker,WorkerDebugger),
 LegacyWindowAlias=webkitURL]
interface URL {
  [Throws]
  constructor(USVString url, optional USVString base);

  [SetterThrows]
  stringifier attribute USVString href;
  [GetterThrows]
  readonly attribute USVString origin;
  [SetterThrows]
           attribute USVString protocol;
           attribute USVString username;
           attribute USVString password;
           attribute USVString host;
           attribute USVString hostname;
           attribute USVString port;
           attribute USVString pathname;
           attribute USVString search;
  [SameObject]
  readonly attribute URLSearchParams searchParams;
           attribute USVString hash;

  USVString toJSON();
};

[Exposed=(Window,DedicatedWorker,SharedWorker)]
partial interface URL {
  [Throws]
  static DOMString createObjectURL(Blob blob);
  [Throws]
  static void revokeObjectURL(DOMString url);
  [ChromeOnly, Throws]
  static boolean isValidURL(DOMString url);

  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html
  [Throws]
  static DOMString createObjectURL(MediaSource source);
};
