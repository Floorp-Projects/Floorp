/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface MozMmsMessage;
interface MozSmsFilter;
interface MozSmsMessage;

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

[Pref="dom.sms.enabled"]
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

  // The parameter can be either a message id, or a Moz{Mms,Sms}Message, or an
  // array of Moz{Mms,Sms}Message objects.
  [Throws]
  DOMRequest delete(long id);
  [Throws]
  DOMRequest delete(MozSmsMessage message);
  [Throws]
  DOMRequest delete(MozMmsMessage message);
  [Throws]
  DOMRequest delete(sequence<(long or MozSmsMessage or MozMmsMessage)> params);

  // Iterates through Moz{Mms,Sms}Message.
  [Throws]
  DOMCursor getMessages(optional MozSmsFilter? filter = null,
                        optional boolean reverse = false);

  [Throws]
  DOMRequest markMessageRead(long id,
                             boolean read,
                             optional boolean sendReadReport = false);

  // Iterates through nsIDOMMozMobileMessageThread.
  [Throws]
  DOMCursor getThreads();

  [Throws]
  DOMRequest retrieveMMS(long id);
  [Throws]
  DOMRequest retrieveMMS(MozMmsMessage message);

  [Throws]
  DOMRequest getSmscAddress(optional unsigned long serviceId);

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
