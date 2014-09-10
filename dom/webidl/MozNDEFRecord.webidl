/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2013 Deutsche Telekom, Inc. */

enum TNF {
  "empty",
  "well-known",
  "media-type",
  "absolute-uri",
  "external",
  "unknown",
  "unchanged"
};

[Constructor(optional MozNDEFRecordOptions options)]
interface MozNDEFRecord
{
  /**
   * Type Name Field - Specifies the NDEF record type in general.
   */
  [Constant]
  readonly attribute TNF tnf;

  /**
   * type - Describes the content of the payload. This can be a mime type.
   */
  [Constant]
  readonly attribute Uint8Array? type;

  /**
   * id - Identifer is application dependent.
   */
  [Constant]
  readonly attribute Uint8Array? id;

  /**
   * payload - Binary data blob. The meaning of this field is application
   * dependent.
   */
  [Constant]
  readonly attribute Uint8Array? payload;
};

dictionary MozNDEFRecordOptions {
  TNF tnf = "empty";
  Uint8Array type;
  Uint8Array id;
  Uint8Array payload;
};
