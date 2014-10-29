/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type, optional MozNFCTagEventInit eventInitDict),
 Func="Navigator::HasNFCSupport", CheckPermissions="nfc-read nfc-write",
 AvailableIn="CertifiedApps"]
interface MozNFCTagEvent : Event
{
  /**
   * The detected NFCTag.
   */
  readonly attribute MozNFCTag? tag;

  /**
   * The MozNDEFRecords pre-read during tag-discovered.
   */
  [Cached, Pure]
  readonly attribute sequence<MozNDEFRecord>? ndefRecords;
};

dictionary MozNFCTagEventInit : EventInit
{
  MozNFCTag? tag = null;

  sequence<MozNDEFRecord>? ndefRecords = [];
};
