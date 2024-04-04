/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origins of this IDL file are
 * http://url.spec.whatwg.org/#api
 * https://w3c.github.io/FileAPI/#creating-revoking
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface URI;

[Exposed=(Window,Worker,WorkerDebugger),
 LegacyWindowAlias=webkitURL]
interface URL {
  [Throws]
  constructor(UTF8String url, optional UTF8String base);

  static URL? parse(UTF8String url, optional UTF8String base);
  static boolean canParse(UTF8String url, optional UTF8String base);

  [SetterThrows]
  stringifier attribute UTF8String href;
  readonly attribute UTF8String origin;
           attribute UTF8String protocol;
           attribute UTF8String username;
           attribute UTF8String password;
           attribute UTF8String host;
           attribute UTF8String hostname;
           attribute UTF8String port;
           attribute UTF8String pathname;
           attribute UTF8String search;
  [SameObject]
  readonly attribute URLSearchParams searchParams;
           attribute UTF8String hash;

  [ChromeOnly]
  readonly attribute URI URI;
  [ChromeOnly]
  static URL fromURI(URI uri);

  UTF8String toJSON();
};

[Exposed=(Window,DedicatedWorker,SharedWorker)]
partial interface URL {
  [Throws]
  static UTF8String createObjectURL(Blob blob);
  [Throws]
  static undefined revokeObjectURL(UTF8String url);
  [ChromeOnly, Throws]
  static boolean isValidObjectURL(UTF8String url);

  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html
  [Throws]
  static UTF8String createObjectURL(MediaSource source);
};
