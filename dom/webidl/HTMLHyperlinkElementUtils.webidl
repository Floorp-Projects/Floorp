/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/multipage/semantics.html#htmlhyperlinkelementutils
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

[NoInterfaceObject]
interface HTMLHyperlinkElementUtils {
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
  [Throws]
           attribute USVString hash;
};
