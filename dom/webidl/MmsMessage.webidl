/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary MmsDeliveryInfo {
  DOMString? receiver = null;
  DOMString? deliveryStatus = null;
  DOMTimeStamp deliveryTimestamp = 0; // 0 if not available (e.g.,
                                      // |delivery| = "received" or not yet delivered).
  DOMString? readStatus = null;
  DOMTimeStamp readTimestamp = 0; // 0 if not available (e.g.,
                                  // |delivery| = "received" or not yet read).
};

[Pref="dom.sms.enabled",
 ChromeOnly]
interface MmsMessage {
  /**
   * |type| is always "mms".
   */
  readonly attribute DOMString type;

  /**
   * The id of the message record in the database.
   */
  readonly attribute long id;

  /**
   * The Thread id this message belonging to.
   */
  readonly attribute unsigned long long threadId;

  /**
   * Integrated Circuit Card Identifier.
   *
   * Will be null if ICC is not available.
   */
  readonly attribute DOMString iccId;

  /**
   * Should be "not-downloaded", "received", "sending", "sent" or "error".
   */
  readonly attribute DOMString delivery;

  [Cached, Pure]
  readonly attribute sequence<MmsDeliveryInfo> deliveryInfo;

  /**
   * The sender's address.
   */
  readonly attribute DOMString sender;

  /**
   * The addreses of the receivers.
   */
  [Cached, Pure]
  readonly attribute sequence<DOMString> receivers;

  /**
   * Device timestamp when message is either sent or received.
   */
  readonly attribute DOMTimeStamp timestamp;

  /**
   * The timestamp from MMSC when |delivery| is |received|.
   */
  readonly attribute DOMTimeStamp sentTimestamp;

  /**
   * The read status of this message.
   */
  readonly attribute boolean read;

  /**
   * The subject of this message.
   */
  readonly attribute DOMString subject;

  /**
   * The SMIL document of this message.
   */
  readonly attribute DOMString smil;

  /**
   * The attachments of this message.
   */
  [Cached, Pure]
  readonly attribute sequence<MmsAttachment> attachments;

  /**
   * Expiry date for an MMS to be retrieved.
   */
  readonly attribute DOMTimeStamp expiryDate;

  /**
   * The flag to indicate that a read report is requested by the sender or not.
   */
  readonly attribute boolean readReportRequested;
};
