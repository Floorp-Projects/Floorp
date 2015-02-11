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

enum MediaKeysRequirement {
  "required",
  "optional",
  "disallowed"
};

dictionary MediaKeySystemOptions {
  DOMString            initDataType = "";
  DOMString            audioType = "";
  DOMString            audioCapability = "";
  DOMString            videoType = "";
  DOMString            videoCapability = "";
  MediaKeysRequirement uniqueidentifier = "optional";
  MediaKeysRequirement stateful = "optional";
};

[Pref="media.eme.apiVisible"]
interface MediaKeySystemAccess {
  readonly    attribute DOMString keySystem;
  [NewObject]
  Promise<MediaKeys> createMediaKeys();
};
