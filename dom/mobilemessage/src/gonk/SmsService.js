/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const RIL_SMSSERVICE_CONTRACTID = "@mozilla.org/sms/rilsmsservice;1";
const RIL_SMSSERVICE_CID =
  Components.ID("{46a9ed78-3574-40a1-9f12-ea179942d67f}");

const DELIVERY_STATE_RECEIVED = "received";
const DELIVERY_STATE_SENDING  = "sending";
const DELIVERY_STATE_SENT     = "sent";
const DELIVERY_STATE_ERROR    = "error";

// Observer topics to send.
const kSmsReceivedObserverTopic          = "sms-received";
const kSmsSendingObserverTopic           = "sms-sending";
const kSmsSentObserverTopic              = "sms-sent";
const kSmsFailedObserverTopic            = "sms-failed";
const kSmsDeliverySuccessObserverTopic   = "sms-delivery-success";
const kSmsDeliveryErrorObserverTopic     = "sms-delivery-error";
const kSilentSmsReceivedObserverTopic    = "silent-sms-received";

// Observer topics to watch.
const kPrefenceChangedObserverTopic = "nsPref:changed";
const kXpcomShutdownObserverTopic   = "xpcom-shutdown";

// Preference keys.
const kPrefKeyRilDebuggingEnabled = "ril.debugging.enabled";

XPCOMUtils.defineLazyServiceGetter(this, "gMobileMessageDatabaseService",
                                   "@mozilla.org/mobilemessage/rilmobilemessagedatabaseservice;1",
                                   "nsIRilMobileMessageDatabaseService");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileMessageService",
                                   "@mozilla.org/mobilemessage/mobilemessageservice;1",
                                   "nsIMobileMessageService");

XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

XPCOMUtils.defineLazyGetter(this, "gRadioInterface", function () {
  let ril = Cc["@mozilla.org/ril;1"].getService(Ci["nsIRadioInterfaceLayer"]);
  // TODO: Bug 854326 - B2G Multi-SIM: support multiple SIM cards for SMS/MMS
  return ril.getRadioInterface(0);
});

XPCOMUtils.defineLazyGetter(this, "gPhoneNumberUtils", function () {
  let ns = {};
  Cu.import("resource://gre/modules/PhoneNumberUtils.jsm", ns);
  return ns.PhoneNumberUtils;
});

XPCOMUtils.defineLazyGetter(this, "WAP", function () {
  let WAP = {};
  Cu.import("resource://gre/modules/WapPushManager.js", WAP);
  return WAP;
});

XPCOMUtils.defineLazyGetter(this, "gMessageManager", function () {
  let ns = {};
  Cu.import("resource://gre/modules/RilMessageManager.jsm", ns);
  return ns.RilMessageManager;
});

let DEBUG;
function debug(s) {
  dump("SmsService: " + s + "\n");
}

/**
 * SmsService
 */
function SmsService() {
  // Update |DEBUG|.
  this.observe(null, kPrefenceChangedObserverTopic,
               kPrefKeyRilDebuggingEnabled);

  this.silentNumbers = [];

  this.portAddressedSmsApps = {};
  this.portAddressedSmsApps[WAP.WDP_PORT_PUSH] =
    this._handleSmsWdpPortPush.bind(this);

  Services.obs.addObserver(this, kPrefenceChangedObserverTopic, false);
  Services.obs.addObserver(this, kXpcomShutdownObserverTopic, false);
}
SmsService.prototype = {

  classID: RIL_SMSSERVICE_CID,
  classInfo: XPCOMUtils.generateCI({classID: RIL_SMSSERVICE_CID,
                                    contractID: RIL_SMSSERVICE_CONTRACTID,
                                    classDescription: "SmsService",
                                    interfaces: [Ci.nsIRilSmsService,
                                                 Ci.nsISmsService],
                                    flags: Ci.nsIClassInfo.SINGLETON}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsIRilSmsService,
                                         Ci.nsISmsService]),

  /**
   * List of tuples of national language identifier pairs.
   *
   * TODO: Support static/runtime settings, see bug 733331.
   */
  enabledGsmTableTuples: [
    [RIL.PDU_NL_IDENTIFIER_DEFAULT, RIL.PDU_NL_IDENTIFIER_DEFAULT],
  ],

  /**
   * Use 16-bit reference number for concatenated outgoint messages.
   *
   * TODO: Support static/runtime settings, see bug 733331.
   */
  segmentRef16Bit: false,

  /**
   * Get valid SMS concatenation reference number.
   */
  segmentRef: 0,
  get nextSegmentRef() {
    let ref = this.segmentRef++;

    this.segmentRef %= (this.segmentRef16Bit ? 65535 : 255);

    // 0 is not a valid SMS concatenation reference number.
    return ref + 1;
  },

  statusReportPendingMessageIds: null,

  portAddressedSmsApps: null,

  silentNumbers: null,

  _getStrict7BitEncoding: function _getStrict7BitEncoding() {
    try {
      return Services.prefs.getBoolPref("dom.sms.strict7BitEncoding");
    } catch (e) {
      return false;
    }
  },

  _getRequestStatusReport: function _getRequestStatusReport() {
    try {
      return Services.prefs.getBoolPref("dom.sms.requestStatusReport");
    } catch (e) {
      return true;
    }
  },

  _getMsisdn: function _getMsisdn() {
    let iccInfo = gRadioInterface.rilContext.iccInfo;
    let number = iccInfo ? iccInfo.msisdn : null;

    // Workaround an xpconnect issue with undefined string objects.
    // See bug 808220
    if (number === undefined || number === "undefined") {
      return null;
    }
    return number;
  },

  /**
   * Calculate encoded length using specified locking/single shift table
   *
   * @param message
   *        message string to be encoded.
   * @param langTable
   *        locking shift table string.
   * @param langShiftTable
   *        single shift table string.
   * @param strict7BitEncoding
   *        Optional. Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return encoded length in septets.
   *
   * @note that the algorithm used in this function must match exactly with
   * GsmPDUHelper#writeStringAsSeptets.
   */
  _countGsm7BitSeptets: function _countGsm7BitSeptets(message,
                                                      langTable,
                                                      langShiftTable,
                                                      strict7BitEncoding) {
    let length = 0;
    for (let msgIndex = 0; msgIndex < message.length; msgIndex++) {
      let c = message.charAt(msgIndex);
      if (strict7BitEncoding) {
        c = RIL.GSM_SMS_STRICT_7BIT_CHARMAP[c] || c;
      }

      let septet = langTable.indexOf(c);

      // According to 3GPP TS 23.038, section 6.1.1 General notes, "The
      // characters marked '1)' are not used but are displayed as a space."
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        length++;
        continue;
      }

      septet = langShiftTable.indexOf(c);
      if (septet < 0) {
        if (!strict7BitEncoding) {
          return -1;
        }

        // Bug 816082, when strict7BitEncoding is enabled, we should replace
        // characters that can't be encoded with GSM 7-Bit alphabets with '*'.
        c = '*';
        if (langTable.indexOf(c) >= 0) {
          length++;
        } else if (langShiftTable.indexOf(c) >= 0) {
          length += 2;
        } else {
          // We can't even encode a '*' character with current configuration.
          return -1;
        }

        continue;
      }

      // According to 3GPP TS 23.038 B.2, "This code represents a control
      // character and therefore must not be used for language specific
      // characters."
      if (septet == RIL.PDU_NL_RESERVED_CONTROL) {
        continue;
      }

      // The character is not found in locking shfit table, but could be
      // encoded as <escape><char> with single shift table. Note that it's
      // still possible for septet to has the value of PDU_NL_EXTENDED_ESCAPE,
      // but we can display it as a space in this case as said in previous
      // comment.
      length += 2;
    }

    return length;
  },

  /**
   * Calculate user data length of specified message string encoded in GSM 7Bit
   * alphabets.
   *
   * @param message
   *        a message string to be encoded.
   * @param strict7BitEncoding
   *        Optional. Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return null or an options object with attributes `dcs`,
   *         `userDataHeaderLength`, `encodedFullBodyLength`, `langIndex`,
   *         `langShiftIndex`, `segmentMaxSeq` set.
   *
   * @see #_calculateUserDataLength().
   */
  _calculateUserDataLength7Bit: function _calculateUserDataLength7Bit(message,
                                                                      strict7BitEncoding) {
    let options = null;
    let minUserDataSeptets = Number.MAX_VALUE;
    for (let i = 0; i < this.enabledGsmTableTuples.length; i++) {
      let [langIndex, langShiftIndex] = this.enabledGsmTableTuples[i];

      const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
      const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];

      let bodySeptets = this._countGsm7BitSeptets(message,
                                                  langTable,
                                                  langShiftTable,
                                                  strict7BitEncoding);
      if (bodySeptets < 0) {
        continue;
      }

      let headerLen = 0;
      if (langIndex != RIL.PDU_NL_IDENTIFIER_DEFAULT) {
        headerLen += 3; // IEI + len + langIndex
      }
      if (langShiftIndex != RIL.PDU_NL_IDENTIFIER_DEFAULT) {
        headerLen += 3; // IEI + len + langShiftIndex
      }

      // Calculate full user data length, note the extra byte is for header len
      let headerSeptets = Math.ceil((headerLen ? headerLen + 1 : 0) * 8 / 7);
      let segmentSeptets = RIL.PDU_MAX_USER_DATA_7BIT;
      if ((bodySeptets + headerSeptets) > segmentSeptets) {
        headerLen += this.segmentRef16Bit ? 6 : 5;
        headerSeptets = Math.ceil((headerLen + 1) * 8 / 7);
        segmentSeptets -= headerSeptets;
      }

      let segments = Math.ceil(bodySeptets / segmentSeptets);
      let userDataSeptets = bodySeptets + headerSeptets * segments;
      if (userDataSeptets >= minUserDataSeptets) {
        continue;
      }

      minUserDataSeptets = userDataSeptets;

      options = {
        dcs: RIL.PDU_DCS_MSG_CODING_7BITS_ALPHABET,
        encodedFullBodyLength: bodySeptets,
        userDataHeaderLength: headerLen,
        langIndex: langIndex,
        langShiftIndex: langShiftIndex,
        segmentMaxSeq: segments,
        segmentChars: segmentSeptets,
      };
    }

    return options;
  },

  /**
   * Calculate user data length of specified message string encoded in UCS2.
   *
   * @param message
   *        a message string to be encoded.
   *
   * @return an options object with attributes `dcs`, `userDataHeaderLength`,
   *         `encodedFullBodyLength`, `segmentMaxSeq` set.
   *
   * @see #_calculateUserDataLength().
   */
  _calculateUserDataLengthUCS2: function _calculateUserDataLengthUCS2(message) {
    let bodyChars = message.length;
    let headerLen = 0;
    let headerChars = Math.ceil((headerLen ? headerLen + 1 : 0) / 2);
    let segmentChars = RIL.PDU_MAX_USER_DATA_UCS2;
    if ((bodyChars + headerChars) > segmentChars) {
      headerLen += this.segmentRef16Bit ? 6 : 5;
      headerChars = Math.ceil((headerLen + 1) / 2);
      segmentChars -= headerChars;
    }

    let segments = Math.ceil(bodyChars / segmentChars);

    return {
      dcs: RIL.PDU_DCS_MSG_CODING_16BITS_ALPHABET,
      encodedFullBodyLength: bodyChars * 2,
      userDataHeaderLength: headerLen,
      segmentMaxSeq: segments,
      segmentChars: segmentChars,
    };
  },

  /**
   * Calculate user data length and its encoding.
   *
   * @param message
   *        a message string to be encoded.
   * @param strict7BitEncoding
   *        Optional. Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return an options object with some or all of following attributes set:
   *
   * @param dcs
   *        Data coding scheme. One of the PDU_DCS_MSG_CODING_*BITS_ALPHABET
   *        constants.
   * @param userDataHeaderLength
   *        Length of embedded user data header, in bytes. The whole header
   *        size will be userDataHeaderLength + 1; 0 for no header.
   * @param encodedFullBodyLength
   *        Length of the message body when encoded with the given DCS. For
   *        UCS2, in bytes; for 7-bit, in septets.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   * @param segmentMaxSeq
   *        Max sequence number of a multi-part messages, or 1 for single one.
   *        This number might not be accurate for a multi-part message until
   *        it's processed by #_fragmentText() again.
   */
  _calculateUserDataLength: function _calculateUserDataLength(message,
                                                              strict7BitEncoding) {
    let options = this._calculateUserDataLength7Bit(message, strict7BitEncoding);
    if (!options) {
      options = this._calculateUserDataLengthUCS2(message);
    }

    if (DEBUG) debug("_calculateUserDataLength: " + JSON.stringify(options));
    return options;
  },

  /**
   * Fragment GSM 7-Bit encodable string for transmission.
   *
   * @param text
   *        text string to be fragmented.
   * @param langTable
   *        locking shift table string.
   * @param langShiftTable
   *        single shift table string.
   * @param segmentSeptets
   *        Number of available spetets per segment.
   * @param strict7BitEncoding
   *        Optional. Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return an array of objects. See #_fragmentText() for detailed definition.
   */
  _fragmentText7Bit: function _fragmentText7Bit(text, langTable, langShiftTable,
                                                segmentSeptets,
                                                strict7BitEncoding) {
    let ret = [];
    let body = "", len = 0;
    for (let i = 0, inc = 0; i < text.length; i++) {
      let c = text.charAt(i);
      if (strict7BitEncoding) {
        c = RIL.GSM_SMS_STRICT_7BIT_CHARMAP[c] || c;
      }

      let septet = langTable.indexOf(c);
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        inc = 1;
      } else {
        septet = langShiftTable.indexOf(c);
        if (septet == RIL.PDU_NL_RESERVED_CONTROL) {
          continue;
        }

        inc = 2;
        if (septet < 0) {
          if (!strict7BitEncoding) {
            throw new Error("Given text cannot be encoded with GSM 7-bit Alphabet!");
          }

          // Bug 816082, when strict7BitEncoding is enabled, we should replace
          // characters that can't be encoded with GSM 7-Bit alphabets with '*'.
          c = '*';
          if (langTable.indexOf(c) >= 0) {
            inc = 1;
          }
        }
      }

      if ((len + inc) > segmentSeptets) {
        ret.push({
          body: body,
          encodedBodyLength: len,
        });
        body = c;
        len = inc;
      } else {
        body += c;
        len += inc;
      }
    }

    if (len) {
      ret.push({
        body: body,
        encodedBodyLength: len,
      });
    }

    return ret;
  },

  /**
   * Fragment UCS2 encodable string for transmission.
   *
   * @param text
   *        text string to be fragmented.
   * @param segmentChars
   *        Number of available characters per segment.
   *
   * @return an array of objects. See #_fragmentText() for detailed definition.
   */
  _fragmentTextUCS2: function _fragmentTextUCS2(text, segmentChars) {
    let ret = [];
    for (let offset = 0; offset < text.length; offset += segmentChars) {
      let str = text.substr(offset, segmentChars);
      ret.push({
        body: str,
        encodedBodyLength: str.length * 2,
      });
    }

    return ret;
  },

  /**
   * Fragment string for transmission.
   *
   * Fragment input text string into an array of objects that contains
   * attributes `body`, substring for this segment, `encodedBodyLength`,
   * length of the encoded segment body in septets.
   *
   * @param text
   *        Text string to be fragmented.
   * @param options
   *        Optional pre-calculated option object. The output array will be
   *        stored at options.segments if there are multiple segments.
   * @param strict7BitEncoding
   *        Optional. Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return Populated options object.
   */
  _fragmentText: function _fragmentText(text, options, strict7BitEncoding) {
    if (!options) {
      options = this._calculateUserDataLength(text, strict7BitEncoding);
    }

    if (options.dcs == RIL.PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[options.langIndex];
      const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[options.langShiftIndex];
      options.segments = this._fragmentText7Bit(text,
                                                langTable, langShiftTable,
                                                options.segmentChars,
                                                strict7BitEncoding);
    } else {
      options.segments = this._fragmentTextUCS2(text,
                                                options.segmentChars);
    }

    // Re-sync options.segmentMaxSeq with actual length of returning array.
    options.segmentMaxSeq = options.segments.length;

    return options;
  },

  /**
   * A helper to broadcast the system message to launch registered apps
   * like Costcontrol, Notification and Message app... etc.
   *
   * @param aName
   *        The system message name.
   * @param aDomMessage
   *        The nsIDOMMozSmsMessage object.
   */
  _broadcastSystemMessage: function _broadcastSystemMessage(aName, aDomMessage) {
    if (DEBUG) debug("Broadcasting the SMS system message: " + aName);

    // Sadly we cannot directly broadcast the aDomMessage object
    // because the system message mechamism will rewrap the object
    // based on the content window, which needs to know the properties.
    gSystemMessenger.broadcastMessage(aName, {
      type:           aDomMessage.type,
      id:             aDomMessage.id,
      threadId:       aDomMessage.threadId,
      delivery:       aDomMessage.delivery,
      deliveryStatus: aDomMessage.deliveryStatus,
      sender:         aDomMessage.sender,
      receiver:       aDomMessage.receiver,
      body:           aDomMessage.body,
      messageClass:   aDomMessage.messageClass,
      timestamp:      aDomMessage.timestamp,
      read:           aDomMessage.read
    });
  },

  /**
   * Handle WDP port push PDU. Constructor WDP bearer information and deliver
   * to WapPushManager.
   *
   * @param message
   *        A SMS message.
   */
  _handleSmsWdpPortPush: function _handleSmsWdpPortPush(message) {
    if (message.encoding != RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      if (DEBUG) {
        debug("Got port addressed SMS but not encoded in 8-bit alphabet." +
              " Drop!");
      }
      return;
    }

    let options = {
      bearer: WAP.WDP_BEARER_GSM_SMS_GSM_MSISDN,
      sourceAddress: message.sender,
      sourcePort: message.header.originatorPort,
      destinationAddress: this.rilContext.iccInfo.msisdn,
      destinationPort: message.header.destinationPort,
    };
    WAP.WapPushManager.receiveWdpPDU(message.fullData, message.fullData.length,
                                     0, options);
  },

  _isSilentNumber: function _isSilentNumber(number) {
    return this.silentNumbers.indexOf(number) >= 0;
  },

  /**
   * nsISmsService methods.
   */

  // TODO: Bug 859616 - WebSMS: return undefined if the API is unsupported on
  // the platform, not null
  hasSupport: function hasSupport() {
    return true;
  },

  getSegmentInfoForText: function getSegmentInfoForText(text) {
    let strict7BitEncoding = this._getStrict7BitEncoding();

    let options = this._fragmentText(text, null, strict7BitEncoding);
    let charsInLastSegment;
    if (options.segmentMaxSeq) {
      let lastSegment = options.segments[options.segmentMaxSeq - 1];
      charsInLastSegment = lastSegment.encodedBodyLength;
      if (options.dcs == RIL.PDU_DCS_MSG_CODING_16BITS_ALPHABET) {
        // In UCS2 encoding, encodedBodyLength is in octets.
        charsInLastSegment /= 2;
      }
    } else {
      charsInLastSegment = 0;
    }

    let result = gMobileMessageService.createSmsSegmentInfo(options.segmentMaxSeq,
                                                            options.segmentChars,
                                                            options.segmentChars - charsInLastSegment);
    return result;
  },

  send: function send(number, message, silent, request) {
    let strict7BitEncoding = this._getStrict7BitEncoding();
    let requestStatusReport = this._getRequestStatusReport();

    let options = this._fragmentText(message, null, strict7BitEncoding);
    options.number = gPhoneNumberUtils.normalize(number);
    options.requestStatusReport = requestStatusReport && !silent;
    if (options.segmentMaxSeq > 1) {
      options.segmentRef16Bit = this.segmentRef16Bit;
      options.segmentRef = this.nextSegmentRef;
    }

    let notifyResult = (function notifyResult(rv, domMessage) {
      // TODO bug 832140 handle !Components.isSuccessCode(rv)
      if (!silent) {
        Services.obs.notifyObservers(domMessage, kSmsSendingObserverTopic, null);
      }

      // If the radio is disabled or the SIM card is not ready, just directly
      // return with the corresponding error code.
      let errorCode;
      if (!gPhoneNumberUtils.isPlainPhoneNumber(options.number)) {
        if (DEBUG) debug("Error! Address is invalid when sending SMS: " +
                              options.number);
        errorCode = Ci.nsIMobileMessageCallback.INVALID_ADDRESS_ERROR;
      } else if (gRadioInterface.rilContext.radioState !=
                 RIL.GECKO_RADIOSTATE_READY) {
        if (DEBUG) debug("Error! Radio is disabled when sending SMS.");
        errorCode = Ci.nsIMobileMessageCallback.RADIO_DISABLED_ERROR;
      } else if (gRadioInterface.rilContext.cardState !=
                 RIL.GECKO_CARDSTATE_READY) {
        if (DEBUG) debug("Error! SIM card is not ready when sending SMS.");
        errorCode = Ci.nsIMobileMessageCallback.NO_SIM_CARD_ERROR;
      }
      if (errorCode) {
        if (silent) {
          request.notifySendMessageFailed(errorCode);
          return;
        }

        gMobileMessageDatabaseService
          .setMessageDeliveryByMessageId(domMessage.id,
                                         null,
                                         DELIVERY_STATE_ERROR,
                                         RIL.GECKO_SMS_DELIVERY_STATUS_ERROR,
                                         null,
                                         function notifyResult(rv, domMessage) {
          // TODO bug 832140 handle !Components.isSuccessCode(rv)
          request.notifySendMessageFailed(errorCode);
          Services.obs.notifyObservers(domMessage, kSmsFailedObserverTopic, null);
        });
        return;
      }

      // Keep current SMS message info for sent/delivered notifications
      let context = {
        request: request,
        sms: domMessage,
        requestStatusReport: options.requestStatusReport,
        silent: silent
      };

      // This is the entry point starting to send SMS.
      gRadioInterface.sendWorkerMessage("sendSMS", options,
                                        (function(context, response) {
        if (response.errorMsg) {
          // Failed to send SMS out.
          let error = Ci.nsIMobileMessageCallback.UNKNOWN_ERROR;
          switch (message.errorMsg) {
            case RIL.ERROR_RADIO_NOT_AVAILABLE:
              error = Ci.nsIMobileMessageCallback.NO_SIGNAL_ERROR;
              break;
          }

          if (context.silent) {
            context.request.notifySendMessageFailed(error);
            return false;
          }

          gMobileMessageDatabaseService
            .setMessageDeliveryByMessageId(context.sms.id,
                                           null,
                                           DELIVERY_STATE_ERROR,
                                           RIL.GECKO_SMS_DELIVERY_STATUS_ERROR,
                                           null,
                                           function notifyResult(rv, domMessage) {
            // TODO bug 832140 handle !Components.isSuccessCode(rv)
            context.request.notifySendMessageFailed(error);
            Services.obs.notifyObservers(domMessage, kSmsFailedObserverTopic, null);
          });
          return false;
        } // End of send failure.

        if (response.deliveryStatus) {
          // Message delivery.
          gMobileMessageDatabaseService
            .setMessageDeliveryByMessageId(context.sms.id,
                                           null,
                                           context.sms.delivery,
                                           message.deliveryStatus,
                                           null,
                                           function notifyResult(rv, domMessage) {
            // TODO bug 832140 handle !Components.isSuccessCode(rv)
            let topic = (message.deliveryStatus == RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS)
                        ? kSmsDeliverySuccessObserverTopic
                        : kSmsDeliveryErrorObserverTopic;
            Services.obs.notifyObservers(domMessage, topic, null);
          });

          // Send transaction has ended completely.
          return false;
        } // End of message delivery.

        // Message sent.
        if (context.silent) {
          // There is no way to modify nsIDOMMozSmsMessage attributes as they are
          // read only so we just create a new sms instance to send along with
          // the notification.
          let sms = context.sms;
          context.request.notifyMessageSent(
            gMobileMessageService.createSmsMessage(sms.id,
                                                   sms.threadId,
                                                   DELIVERY_STATE_SENT,
                                                   sms.deliveryStatus,
                                                   sms.sender,
                                                   sms.receiver,
                                                   sms.body,
                                                   sms.messageClass,
                                                   sms.timestamp,
                                                   sms.read));
          // We don't wait for SMS-DELIVER-REPORT for silent one.
          return false;
        }

        gMobileMessageDatabaseService
          .setMessageDeliveryByMessageId(context.sms.id,
                                         null,
                                         DELIVERY_STATE_SENT,
                                         context.sms.deliveryStatus,
                                         null,
                                         (function notifyResult(rv, domMessage) {
          // TODO bug 832140 handle !Components.isSuccessCode(rv)
          this._broadcastSystemMessage("sms-sent", domMessage);

          if (context.requestStatusReport) {
            context.sms = domMessage;
          }

          context.request.notifyMessageSent(domMessage);
          Services.obs.notifyObservers(domMessage, kSmsSentObserverTopic, null);
        }).bind(this));

        // Only keep current context if we have requested for delivery report.
        return context.requestStatusReport;
      }).bind(this, context)); // End of |sendWorkerMessage| callback.
    }).bind(this); // End of DB saveSendingMessage callback.

    let sendingMessage = {
      type: "sms",
      sender: this._getMsisdn(),
      receiver: number,
      body: message,
      deliveryStatusRequested: options.requestStatusReport,
      timestamp: Date.now()
    };

    if (silent) {
      let deliveryStatus = RIL.GECKO_SMS_DELIVERY_STATUS_PENDING;
      let delivery = DELIVERY_STATE_SENDING;
      let domMessage =
        gMobileMessageService.createSmsMessage(-1, // id
                                               0,  // threadId
                                               delivery,
                                               deliveryStatus,
                                               sendingMessage.sender,
                                               sendingMessage.receiver,
                                               sendingMessage.body,
                                               "normal", // message class
                                               sendingMessage.timestamp,
                                               false);
      notifyResult(Cr.NS_OK, domMessage);
      return;
    }

    let id = gMobileMessageDatabaseService.saveSendingMessage(
      sendingMessage, notifyResult);
  },

  addSilentNumber: function addSilentNumber(number) {
    if (this._isSilentNumber(number)) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this.silentNumbers.push(number);
  },

  removeSilentNumber: function removeSilentNumber(number) {
    let index = this.silentNumbers.indexOf(number);
    if (index < 0) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    this.silentNumbers.splice(index, 1);
  },

  /**
   * nsIRilSmsService methods.
   */

  notifyMessageReceived: function notifyMessageReceived(message) {
    if (DEBUG) debug("notifyMessageReceived: " + JSON.stringify(message));

    // FIXME: Bug 737202 - Typed arrays become normal arrays when sent to/from workers
    if (message.encoding == RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      message.fullData = new Uint8Array(message.fullData);
    }

    // Dispatch to registered handler if application port addressing is
    // available. Note that the destination port can possibly be zero when
    // representing a UDP/TCP port.
    if (message.header && message.header.destinationPort != null) {
      let handler = this.portAddressedSmsApps[message.header.destinationPort];
      if (handler) {
        handler(message);
      }
      gRadioInterface.sendWorkerMessage("ackSMS", { result: RIL.PDU_FCS_OK });
      return;
    }

    if (message.encoding == RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      // Don't know how to handle binary data yet.
      gRadioInterface.sendWorkerMessage("ackSMS", { result: RIL.PDU_FCS_OK });
      return;
    }

    message.type = "sms";
    message.sender = message.sender || null;
    message.receiver = this._getMsisdn();
    message.body = message.fullBody = message.fullBody || null;
    message.timestamp = Date.now();

    if (this._isSilentNumber(message.sender)) {
      message.id = -1;
      message.threadId = 0;
      message.delivery = DELIVERY_STATE_RECEIVED;
      message.deliveryStatus = RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS;
      message.read = false;

      let domMessage =
        gMobileMessageService.createSmsMessage(message.id,
                                               message.threadId,
                                               message.delivery,
                                               message.deliveryStatus,
                                               message.sender,
                                               message.receiver,
                                               message.body,
                                               message.messageClass,
                                               message.timestamp,
                                               message.read);

      Services.obs.notifyObservers(domMessage,
                                   kSilentSmsReceivedObserverTopic,
                                   null);
      gRadioInterface.sendWorkerMessage("ackSMS", { result: RIL.PDU_FCS_OK });
      return;
    }

    // TODO: Bug #768441
    // For now we don't store indicators persistently. When the mwi.discard
    // flag is false, we'll need to persist the indicator to EFmwis.
    // See TS 23.040 9.2.3.24.2

    let mwi = message.mwi;
    if (mwi) {
      mwi.returnNumber = message.sender;
      mwi.returnMessage = message.fullBody;
      gMessageManager.sendVoicemailMessage("RIL:VoicemailNotification",
                                           this.clientId, mwi);
      gRadioInterface.sendWorkerMessage("ackSMS", { result: RIL.PDU_FCS_OK });
      return;
    }

    let notifyReceived = function notifyReceived(rv, domMessage) {
      let success = Components.isSuccessCode(rv);

      // Acknowledge the reception of the SMS.
      gRadioInterface.sendWorkerMessage("ackSMS", {
        result: (success ? RIL.PDU_FCS_OK
                         : RIL.PDU_FCS_MEMORY_CAPACITY_EXCEEDED)
      });

      if (!success) {
        // At this point we could send a message to content to notify the user
        // that storing an incoming SMS failed, most likely due to a full disk.
        if (DEBUG) {
          debug("Could not store SMS " + message.id + ", error code " + rv);
        }
        return;
      }

      this._broadcastSystemMessage("sms-received", domMessage);
      Services.obs.notifyObservers(domMessage, kSmsReceivedObserverTopic, null);
    }.bind(this);

    if (message.messageClass == RIL.GECKO_SMS_MESSAGE_CLASSES[RIL.PDU_DCS_MSG_CLASS_0]) {
      message.id = -1;
      message.threadId = 0;
      message.delivery = DELIVERY_STATE_RECEIVED;
      message.deliveryStatus = RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS;
      message.read = false;

      let domMessage =
        gMobileMessageService.createSmsMessage(message.id,
                                               message.threadId,
                                               message.delivery,
                                               message.deliveryStatus,
                                               message.sender,
                                               message.receiver,
                                               message.body,
                                               message.messageClass,
                                               message.timestamp,
                                               message.read);

      notifyReceived(Cr.NS_OK, domMessage);
      return;
    }

    message.id =
      gMobileMessageDatabaseService.saveReceivedMessage(message,
                                                        notifyReceived);
  },

  /**
   * nsIObserver methods.
   */

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case kPrefenceChangedObserverTopic:
        if (data === "ril.debugging.enabled") {
          try {
            DEBUG = RIL.DEBUG_RIL ||
                    Services.prefs.getBoolPref("ril.debugging.enabled");
          } catch(e) {}
        }
        break;

      case kXpcomShutdownObserverTopic:
        Services.obs.removeObserver(this, kPrefenceChangedObserverTopic);
        Services.obs.removeObserver(this, kXpcomShutdownObserverTopic);
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SmsService]);
