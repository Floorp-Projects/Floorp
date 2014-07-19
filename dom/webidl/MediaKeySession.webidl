/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html
 *
 * Copyright © 2014 W3C® (MIT, ERCIM, Keio, Beihang), All Rights Reserved.
 * W3C liability, trademark and document use rules apply.
 */

[Pref="media.eme.enabled"]
interface MediaKeySession : EventTarget {
  // error state
  readonly attribute MediaKeyError? error;

  // session properties
  readonly attribute DOMString keySystem;
  readonly attribute DOMString sessionId;

  // Invalid WebIDL, doesn't work.
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=25594
  // readonly attribute Array<Uint8Array> usableKeyIds;

  readonly attribute unrestricted double expiration;

  // Promise<any>
  readonly attribute Promise closed;

  // session operations
  //Promise<any>
  [NewObject, Throws]
  Promise update(Uint8Array response);

  // Promise<any>
  [NewObject, Throws]
  Promise close();

  // Promise<any>
  [NewObject, Throws]
  Promise remove();
};
