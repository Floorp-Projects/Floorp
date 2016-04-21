/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary SmsSegmentInfo {
  /**
   * The number of total segments for the input string. The value is always
   * larger-equal than 1.
   */
  long segments = 0;

  /**
   * The number of characters available per segment. The value is always
   * larger-equal than 1.
   */
  long charsPerSegment = 0;

  /**
   * The maximum number of available characters in the last segment. The value
   * is always larger-equal than 0.
   */
  long charsAvailableInLastSegment = 0;
};

dictionary MmsAttachment {
  DOMString? id = null;
  DOMString? location = null;
  Blob? content = null;
};

dictionary MmsParameters {
  sequence<DOMString> receivers;
  DOMString? subject = null;
  DOMString? smil = null;
  sequence<MmsAttachment> attachments;
};

dictionary SmsSendParameters {
  unsigned long serviceId; // The ID of the RIL service which needs to be
                           // specified under the multi-sim scenario.
};

dictionary MmsSendParameters {
  unsigned long serviceId; // The ID of the RIL service which needs to be
                           // specified under the multi-sim scenario.
};

enum MobileMessageFilterDelivery { "sent", "received" };

dictionary MobileMessageFilter
{
  // Close lower bound range for filtering by the message timestamp.
  // Time in milliseconds since Epoch.
  [EnforceRange] DOMTimeStamp? startDate = null;

  // Close upper bound range for filtering by the message timestamp.
  // Time in milliseconds since Epoch.
  [EnforceRange] DOMTimeStamp? endDate = null;

  // An array of string message participant addresses that any of which
  // appears or matches a message's sendor or recipients addresses.
  sequence<DOMString>? numbers = null;

  MobileMessageFilterDelivery? delivery = null;

  // Filtering by whether a message has been read or not.
  boolean? read = null;

  // Filtering by a message's threadId attribute.
  [EnforceRange] unsigned long long? threadId = null;
};

/**
 * TON defined in |Table 10.5.118: Called party BCD number| of 3GPP TS 24.008.
 * It's used in SM-RL originator / destination address element as defined in
 * |8.2.5.2 Destination address element| of 3GPP TS 24.011.
 */
enum TypeOfNumber { "unknown", "international", "national", "network-specific",
  "dedicated-access-short-code" };

/**
 * NPI defined in |Table 10.5.118: Called party BCD number| of 3GPP TS 24.008.
 * It's used in SM-RL originator / destination address element as defined in
 * |8.2.5.2 Destination address element| of 3GPP TS 24.011.
 */
enum NumberPlanIdentification { "unknown", "isdn", "data", "telex", "national",
  "private" };

/**
 * Type of address used in SmscAddress.
 *
 * As described in |3.1 Parameters Definitions| of 3GPP TS 27.005, the default
 * value of <tosca> should be 129 (typeOfNumber=unknown,
 * numberPlanIdentification=isdn) if the number does not begin with '+'.
 *
 * |setSmscAddress| updates typeOfNumber to international automatically if the
 * given number begins with '+'.
 */
dictionary TypeOfAddress {
  TypeOfNumber typeOfNumber = "unknown";
  NumberPlanIdentification numberPlanIdentification = "isdn";
};

/**
 * SMSC address.
 */
dictionary SmscAddress {
  DOMString address;
  TypeOfAddress typeOfAddress;
};

[Pref="dom.sms.enabled",
 ChromeOnly]
interface MozMobileMessageManager : EventTarget
{
  [Throws]
  DOMRequest getSegmentInfoForText(DOMString text);

  /**
   * Send SMS.
   *
   * @param number
   *        Either a DOMString (only one number) or an array of numbers.
   * @param text
   *        The text message to be sent.
   * @param sendParameters
   *        A SmsSendParameters object.
   *
   * @return
   *        A DOMRequest object indicating the sending result if one number
   *        has been passed; an array of DOMRequest objects otherwise.
   */
  [Throws]
  DOMRequest send(DOMString number,
                  DOMString text,
                  optional SmsSendParameters sendParameters);
  [Throws]
  sequence<DOMRequest> send(sequence<DOMString> numbers,
                            DOMString text,
                            optional SmsSendParameters sendParameters);

  /**
   * Send MMS.
   *
   * @param parameters
   *        A MmsParameters object.
   * @param sendParameters
   *        A MmsSendParameters object.
   *
   * @return
   *        A DOMRequest object indicating the sending result.
   */
  [Throws]
  DOMRequest sendMMS(optional MmsParameters parameters,
                     optional MmsSendParameters sendParameters);

  [Throws]
  DOMRequest getMessage(long id);

  // The parameter can be either a message id, or a {Mms,Sms}Message, or an
  // array of {Mms,Sms}Message objects.
  [Throws]
  DOMRequest delete(long id);
  [Throws]
  DOMRequest delete(SmsMessage message);
  [Throws]
  DOMRequest delete(MmsMessage message);
  [Throws]
  DOMRequest delete(sequence<(long or SmsMessage or MmsMessage)> params);

  // Iterates through {Mms,Sms}Message.
  [Throws]
  DOMCursor getMessages(optional MobileMessageFilter filter,
                        optional boolean reverse = false);

  [Throws]
  DOMRequest markMessageRead(long id,
                             boolean read,
                             optional boolean sendReadReport = false);

  // Iterates through MobileMessageThread.
  [Throws]
  DOMCursor getThreads();

  [Throws]
  DOMRequest retrieveMMS(long id);
  [Throws]
  DOMRequest retrieveMMS(MmsMessage message);

  [Throws]
  Promise<SmscAddress> getSmscAddress(optional unsigned long serviceId);

  /**
   * Set the SMSC address.
   *
   * @param smscAddress
   *        SMSC address to use.
   *        Reject if smscAddress.address does not present.
   * @param serviceId (optional)
   *        The ID of the RIL service which needs to be specified under
   *        the multi-sim scenario.
   * @return a Promise
   *         Resolve if success. Otherwise, reject with error cause.
   */
  [NewObject]
  Promise<void> setSmscAddress(optional SmscAddress smscAddress,
                               optional unsigned long serviceId);

  attribute EventHandler onreceived;
  attribute EventHandler onretrieving;
  attribute EventHandler onsending;
  attribute EventHandler onsent;
  attribute EventHandler onfailed;
  attribute EventHandler ondeliverysuccess;
  attribute EventHandler ondeliveryerror;
  attribute EventHandler onreadsuccess;
  attribute EventHandler onreaderror;
  attribute EventHandler ondeleted;
};
