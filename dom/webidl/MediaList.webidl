/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// https://drafts.csswg.org/cssom/#the-medialist-interface

[Exposed=Window]
interface MediaList {
  stringifier attribute [LegacyNullToEmptyString] UTF8String mediaText;

  readonly attribute unsigned long    length;
  getter UTF8String? item(unsigned long index);
  [Throws]
  void               deleteMedium(UTF8String oldMedium);
  [Throws]
  void               appendMedium(UTF8String newMedium);
};
