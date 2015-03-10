/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2013 Deutsche Telekom, Inc. */

/**
 * Type Name Format.
 *
 * @see NFCForum-TS-NDEF 3.2.6 TNF
 */
enum TNF {
  "empty",
  "well-known",
  "media-type",
  "absolute-uri",
  "external",
  "unknown",
  "unchanged"
};

/**
 * Prefixes of well-known URI.
 *
 * @see NFCForum-TS-RTD_URI Table 3. Abbreviation Table.
 */
enum WellKnownURIPrefix {
  "",
  "http://www.",
  "https://www.",
  "http://",
  "https://",
  "tel:",
  "mailto:",
  "ftp://anonymous:anonymous@",
  "ftp://ftp.",
  "ftps://",
  "sftp://",
  "smb://",
  "nfs://",
  "ftp://",
  "dav://",
  "news:",
  "telnet://",
  "imap:",
  "rtsp://",
  "urn:",
  "pop:",
  "sip:",
  "sips:",
  "tftp:",
  "btspp://",
  "btl2cap://",
  "btgoep://",
  "tcpobex://",
  "irdaobex://",
  "file://",
  "urn:epc:id:",
  "urn:epc:tag:",
  "urn:epc:pat:",
  "urn:epc:raw:",
  "urn:epc:",
  "urn:nfc:"
};

/**
 * Record Type Description.
 *
 * Record Types from well-known NDEF Records.
 * @see NFCForum-TS-RTD
 */
enum RTD {
  "U", // URI
};

[Constructor(optional MozNDEFRecordOptions options),
 Constructor(DOMString uri)]
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

  /**
   * Get the size of this NDEF Record.
   */
  [Constant]
  readonly attribute unsigned long size;

  /**
   * Returns this NDEF Record as URI, return null if this record cannot be
   * decoded as a well-known URI record.
   */
  DOMString? getAsURI();
};

dictionary MozNDEFRecordOptions {
  TNF tnf = "empty";
  Uint8Array? type;
  Uint8Array? id;
  Uint8Array? payload;
};
