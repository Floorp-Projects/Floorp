/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2013 Deutsche Telekom, Inc. */

[Constructor(octet tnf, optional Uint8Array type, optional Uint8Array id, optional Uint8Array payload)]
interface MozNDEFRecord
{
  /**
   * Type Name Field (3-bits) - Specifies the NDEF record type in general.
   *   tnf_empty: 0x00
   *   tnf_well_known: 0x01
   *   tnf_mime_media: 0x02
   *   tnf_absolute_uri: 0x03
   *   tnf_external type: 0x04
   *   tnf_unknown: 0x05
   *   tnf_unchanged: 0x06
   *   tnf_reserved: 0x07
   */
  [Constant]
  readonly attribute octet tnf;

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
