/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.sms.enabled",
 ChromeOnly]
interface SmsMessage {
  /**
   * |type| is always "sms".
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
   * Should be "received", "sending", "sent" or "error".
   */
  readonly attribute DOMString delivery;

  /**
   * Possible delivery status values for above delivery states are:
   *
   * "received": "success"
   * "sending" : "pending", or "not-applicable" if the message was sent without
   *             status report requisition.
   * "sent"    : "pending", "success", "error", or "not-applicable"
   *             if the message was sent without status report requisition.
   * "error"   : "error"
   */
  readonly attribute DOMString deliveryStatus;

  /**
   * The sender's address.
   */
  readonly attribute DOMString sender;

  /**
   * The receiver's address.
   */
  readonly attribute DOMString receiver;

  /**
   * Text body of the message.
   */
  readonly attribute DOMString body;

  /**
   * Should be "normal", "class-0", "class-1", "class-2" or "class-3".
   */
  readonly attribute DOMString messageClass;

  /**
   * Device timestamp when message is either sent or received.
   */
  readonly attribute DOMTimeStamp timestamp;

  /**
   * The timestamp from SMSC when |delivery| is |received|.
   */
  readonly attribute DOMTimeStamp sentTimestamp;

  /**
   * The delivery timestamp when |deliveryStatus| is updated to |success|.
   */
  readonly attribute DOMTimeStamp deliveryTimestamp;

  /**
   * The read status of this message.
   */
  readonly attribute boolean read;
};
