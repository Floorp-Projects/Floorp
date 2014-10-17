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

  readonly attribute unrestricted double expiration;

  readonly attribute Promise<void> closed;

  [NewObject, Throws]
  Promise<void> generateRequest(DOMString initDataType, (ArrayBufferView or ArrayBuffer) initData);

  [NewObject, Throws]
  Promise<boolean> load(DOMString sessionId);

  // session operations
  [NewObject, Throws]
  Promise<void> update((ArrayBufferView or ArrayBuffer) response);

  [NewObject, Throws]
  Promise<void> close();

  [NewObject, Throws]
  Promise<void> remove();

  [NewObject, Throws]
  Promise<sequence<ArrayBuffer>> getUsableKeyIds();
};
