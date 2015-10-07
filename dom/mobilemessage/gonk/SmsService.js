/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const GONK_SMSSERVICE_CONTRACTID = "@mozilla.org/sms/gonksmsservice;1";
const GONK_SMSSERVICE_CID = Components.ID("{f9b9b5e2-73b4-11e4-83ff-a33e27428c86}");

const NS_XPCOM_SHUTDOWN_OBSERVER_ID      = "xpcom-shutdown";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID  = "nsPref:changed";

const kPrefDefaultServiceId = "dom.sms.defaultServiceId";
const kPrefRilDebuggingEnabled = "ril.debugging.enabled";
const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
const kPrefLastKnownSimMcc = "ril.lastKnownSimMcc";

const kDiskSpaceWatcherObserverTopic = "disk-space-watcher";

const kSmsReceivedObserverTopic          = "sms-received";
const kSilentSmsReceivedObserverTopic    = "silent-sms-received";
const kSmsSendingObserverTopic           = "sms-sending";
const kSmsSentObserverTopic              = "sms-sent";
const kSmsFailedObserverTopic            = "sms-failed";
const kSmsDeliverySuccessObserverTopic   = "sms-delivery-success";
const kSmsDeliveryErrorObserverTopic     = "sms-delivery-error";
const kSmsDeletedObserverTopic           = "sms-deleted";

const DOM_MOBILE_MESSAGE_DELIVERY_RECEIVED = "received";
const DOM_MOBILE_MESSAGE_DELIVERY_SENDING  = "sending";
const DOM_MOBILE_MESSAGE_DELIVERY_SENT     = "sent";
const DOM_MOBILE_MESSAGE_DELIVERY_ERROR    = "error";

const SMS_HANDLED_WAKELOCK_TIMEOUT = 5000;

XPCOMUtils.defineLazyGetter(this, "gRadioInterfaces", function() {
  let ril = { numRadioInterfaces: 0 };
  try {
    ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
  } catch(e) {}

  let interfaces = [];
  for (let i = 0; i < ril.numRadioInterfaces; i++) {
    interfaces.push(ril.getRadioInterface(i));
  }
  return interfaces;
});

XPCOMUtils.defineLazyGetter(this, "gSmsSegmentHelper", function() {
  let ns = {};
  Cu.import("resource://gre/modules/SmsSegmentHelper.jsm", ns);

  // Initialize enabledGsmTableTuples from current MCC.
  ns.SmsSegmentHelper.enabledGsmTableTuples = getEnabledGsmTableTuplesFromMcc();

  return ns.SmsSegmentHelper;
});

XPCOMUtils.defineLazyGetter(this, "gPhoneNumberUtils", function() {
  let ns = {};
  Cu.import("resource://gre/modules/PhoneNumberUtils.jsm", ns);
  return ns.PhoneNumberUtils;
});

XPCOMUtils.defineLazyGetter(this, "gWAP", function() {
  let ns = {};
  Cu.import("resource://gre/modules/WapPushManager.js", ns);
  return ns;
});

XPCOMUtils.defineLazyGetter(this, "gSmsSendingSchedulers", function() {
  return {
    _schedulers: [],
    getSchedulerByServiceId: function(aServiceId) {
      let scheduler = this._schedulers[aServiceId];
      if (!scheduler) {
        scheduler = this._schedulers[aServiceId] =
          new SmsSendingScheduler(aServiceId);
      }

      return scheduler;
    }
  };
});

XPCOMUtils.defineLazyServiceGetter(this, "gCellBroadcastService",
                                   "@mozilla.org/cellbroadcast/cellbroadcastservice;1",
                                   "nsIGonkCellBroadcastService");

XPCOMUtils.defineLazyServiceGetter(this, "gIccService",
                                   "@mozilla.org/icc/iccservice;1",
                                   "nsIIccService");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileConnectionService",
                                   "@mozilla.org/mobileconnection/mobileconnectionservice;1",
                                   "nsIMobileConnectionService");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileMessageDatabaseService",
                                   "@mozilla.org/mobilemessage/gonkmobilemessagedatabaseservice;1",
                                   "nsIGonkMobileMessageDatabaseService");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileMessageService",
                                   "@mozilla.org/mobilemessage/mobilemessageservice;1",
                                   "nsIMobileMessageService");

XPCOMUtils.defineLazyServiceGetter(this, "gPowerManagerService",
                                   "@mozilla.org/power/powermanagerservice;1",
                                   "nsIPowerManagerService");

XPCOMUtils.defineLazyServiceGetter(this, "gSmsMessenger",
                                   "@mozilla.org/ril/system-messenger-helper;1",
                                   "nsISmsMessenger");

var DEBUG = RIL.DEBUG_RIL;
function debug(s) {
  dump("SmsService: " + s);
}

function SmsService() {
  this._updateDebugFlag();
  this._silentNumbers = [];
  this.smsDefaultServiceId = this._getDefaultServiceId();

  this._portAddressedSmsApps = {};
  this._portAddressedSmsApps[gWAP.WDP_PORT_PUSH] =
    (aMessage, aServiceId) => this._handleSmsWdpPortPush(aMessage, aServiceId);

  this._receivedSmsSegmentsMap = {};

  Services.prefs.addObserver(kPrefRilDebuggingEnabled, this, false);
  Services.prefs.addObserver(kPrefDefaultServiceId, this, false);
  Services.prefs.addObserver(kPrefLastKnownSimMcc, this, false);
  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  Services.obs.addObserver(this, kDiskSpaceWatcherObserverTopic, false);
}
SmsService.prototype = {
  classID: GONK_SMSSERVICE_CID,

  classInfo: XPCOMUtils.generateCI({classID: GONK_SMSSERVICE_CID,
                                    contractID: GONK_SMSSERVICE_CONTRACTID,
                                    classDescription: "SmsService",
                                    interfaces: [Ci.nsISmsService,
                                                 Ci.nsIGonkSmsService],
                                    flags: Ci.nsIClassInfo.SINGLETON}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISmsService,
                                         Ci.nsIGonkSmsService,
                                         Ci.nsIObserver]),

  _updateDebugFlag: function() {
    try {
      DEBUG = RIL.DEBUG_RIL ||
              Services.prefs.getBoolPref(kPrefRilDebuggingEnabled);
    } catch (e) {}
  },

  _getDefaultServiceId: function() {
    let id = Services.prefs.getIntPref(kPrefDefaultServiceId);
    let numRil = Services.prefs.getIntPref(kPrefRilNumRadioInterfaces);

    if (id >= numRil || id < 0) {
      id = 0;
    }

    return id;
  },

  _getPhoneNumber: function(aServiceId) {
    let number;
    // Get the proper IccInfo based on the current card type.
    try {
      let iccInfo = null;
      let baseIccInfo = this._getIccInfo(aServiceId);
      if (baseIccInfo.iccType === 'ruim' || baseIccInfo.iccType === 'csim') {
        iccInfo = baseIccInfo.QueryInterface(Ci.nsICdmaIccInfo);
        number = iccInfo.mdn;
      } else {
        iccInfo = baseIccInfo.QueryInterface(Ci.nsIGsmIccInfo);
        number = iccInfo.msisdn;
      }
    } catch (e) {
      if (DEBUG) {
       debug("Exception - QueryInterface failed on iccinfo for GSM/CDMA info");
      }
      return null;
    }

    return number;
  },

  _getIccInfo: function(aServiceId) {
    let icc = gIccService.getIccByServiceId(aServiceId);
    return icc ? icc.iccInfo : null;
  },

  _getCardState: function(aServiceId) {
    let icc = gIccService.getIccByServiceId(aServiceId);
    return icc ? icc.cardState : Ci.nsIIcc.CARD_STATE_UNKNOWN;
  },

  _getIccId: function(aServiceId) {
    let iccInfo = this._getIccInfo(aServiceId);

    if (!iccInfo) {
      return null;
    }

    return iccInfo.iccid;
  },

  // The following attributes/functions are used for acquiring/releasing the
  // CPU wake lock when the RIL handles the received SMS. Note that we need
  // a timer to bound the lock's life cycle to avoid exhausting the battery.
  _smsHandledWakeLock: null,
  _smsHandledWakeLockTimer: null,
  _acquireSmsHandledWakeLock: function() {
    if (!this._smsHandledWakeLock) {
      if (DEBUG) debug("Acquiring a CPU wake lock for handling SMS.");
      this._smsHandledWakeLock = gPowerManagerService.newWakeLock("cpu");
    }
    if (!this._smsHandledWakeLockTimer) {
      if (DEBUG) debug("Creating a timer for releasing the CPU wake lock.");
      this._smsHandledWakeLockTimer =
        Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }
    if (DEBUG) debug("Setting the timer for releasing the CPU wake lock.");
    this._smsHandledWakeLockTimer
        .initWithCallback(() => this._releaseSmsHandledWakeLock(),
                          SMS_HANDLED_WAKELOCK_TIMEOUT,
                          Ci.nsITimer.TYPE_ONE_SHOT);
  },

  _releaseSmsHandledWakeLock: function() {
    if (DEBUG) debug("Releasing the CPU wake lock for handling SMS.");
    if (this._smsHandledWakeLockTimer) {
      this._smsHandledWakeLockTimer.cancel();
    }
    if (this._smsHandledWakeLock) {
      this._smsHandledWakeLock.unlock();
      this._smsHandledWakeLock = null;
    }
  },

  _convertSmsMessageClassToString: function(aMessageClass) {
    return RIL.GECKO_SMS_MESSAGE_CLASSES[aMessageClass] || null;
  },

  _convertSmsMessageClass: function(aMessageClass) {
    let index = RIL.GECKO_SMS_MESSAGE_CLASSES.indexOf(aMessageClass);

    if (index < 0) {
      throw new Error("Invalid MessageClass: " + aMessageClass);
    }

    return index;
  },

  _convertSmsDelivery: function(aDelivery) {
    let index = [DOM_MOBILE_MESSAGE_DELIVERY_RECEIVED,
                 DOM_MOBILE_MESSAGE_DELIVERY_SENDING,
                 DOM_MOBILE_MESSAGE_DELIVERY_SENT,
                 DOM_MOBILE_MESSAGE_DELIVERY_ERROR].indexOf(aDelivery);

    if (index < 0) {
      throw new Error("Invalid Delivery: " + aDelivery);
    }

    return index;
  },

  _convertSmsDeliveryStatus: function(aDeliveryStatus) {
    let index = [RIL.GECKO_SMS_DELIVERY_STATUS_NOT_APPLICABLE,
                 RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS,
                 RIL.GECKO_SMS_DELIVERY_STATUS_PENDING,
                 RIL.GECKO_SMS_DELIVERY_STATUS_ERROR].indexOf(aDeliveryStatus);

    if (index < 0) {
      throw new Error("Invalid DeliveryStatus: " + aDeliveryStatus);
    }

    return index;
  },

  _notifySendingError: function(aErrorCode, aSendingMessage, aSilent, aRequest) {
    if (aSilent || aErrorCode === Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR) {
      // There is no way to modify nsIDOMMozSmsMessage attributes as they
      // are read only so we just create a new sms instance to send along
      // with the notification.
      aRequest.notifySendMessageFailed(aErrorCode,
        gMobileMessageService.createSmsMessage(aSendingMessage.id,
                                               aSendingMessage.threadId,
                                               aSendingMessage.iccId,
                                               DOM_MOBILE_MESSAGE_DELIVERY_ERROR,
                                               RIL.GECKO_SMS_DELIVERY_STATUS_ERROR,
                                               aSendingMessage.sender,
                                               aSendingMessage.receiver,
                                               aSendingMessage.body,
                                               aSendingMessage.messageClass,
                                               aSendingMessage.timestamp,
                                               0,
                                               0,
                                               aSendingMessage.read));

      if (!aSilent) {
        Services.obs.notifyObservers(aSendingMessage, kSmsFailedObserverTopic, null);
      }
      return;
    }

    gMobileMessageDatabaseService
      .setMessageDeliveryByMessageId(aSendingMessage.id,
                                     null,
                                     DOM_MOBILE_MESSAGE_DELIVERY_ERROR,
                                     RIL.GECKO_SMS_DELIVERY_STATUS_ERROR,
                                     null,
                                     (aRv, aDomMessage) => {
      // TODO bug 832140 handle !Components.isSuccessCode(aRv)
      this._broadcastSmsSystemMessage(
        Ci.nsISmsMessenger.NOTIFICATION_TYPE_SENT_FAILED, aDomMessage);
      aRequest.notifySendMessageFailed(aErrorCode, aDomMessage);
      Services.obs.notifyObservers(aDomMessage, kSmsFailedObserverTopic, null);
    });
  },

  /**
   * Schedule the sending request.
   */
  _scheduleSending: function(aServiceId, aDomMessage, aSilent, aOptions, aRequest) {
    gSmsSendingSchedulers.getSchedulerByServiceId(aServiceId)
      .schedule({
        messageId: aDomMessage.id,
        onSend: () => {
          if (DEBUG) {
            debug("onSend: messageId=" + aDomMessage.id +
                  ", serviceId=" + aServiceId);
          }
          this._sendToTheAir(aServiceId,
                             aDomMessage,
                             aSilent,
                             aOptions,
                             aRequest);
        },
        onCancel: (aErrorCode) => {
          if (DEBUG) debug("onCancel: " + aErrorCode);
          this._notifySendingError(aErrorCode, aDomMessage, aSilent, aRequest);
        }
      });
  },

  /**
   * Send a SMS message to the modem.
   */
  _sendToTheAir: function(aServiceId, aDomMessage, aSilent, aOptions, aRequest) {
    // Keep current SMS message info for sent/delivered notifications
    let sentMessage = aDomMessage;
    let requestStatusReport = aOptions.requestStatusReport;

    // Retry count for GECKO_ERROR_SMS_SEND_FAIL_RETRY
    if (!aOptions.retryCount) {
      aOptions.retryCount = 0;
    }

    gRadioInterfaces[aServiceId].sendWorkerMessage("sendSMS",
                                                   aOptions,
                                                   (aResponse) => {
      // Failed to send SMS out.
      if (aResponse.errorMsg) {
        let error = Ci.nsIMobileMessageCallback.UNKNOWN_ERROR;
        if (aResponse.errorMsg === RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE) {
          error = Ci.nsIMobileMessageCallback.NO_SIGNAL_ERROR;
        } else if (aResponse.errorMsg === RIL.GECKO_ERROR_FDN_CHECK_FAILURE) {
          error = Ci.nsIMobileMessageCallback.FDN_CHECK_ERROR;
        } else if (aResponse.errorMsg === RIL.GECKO_ERROR_SMS_SEND_FAIL_RETRY &&
          aOptions.retryCount < RIL.SMS_RETRY_MAX) {
          aOptions.retryCount++;
          this._scheduleSending(aServiceId,
                                aDomMessage,
                                aSilent,
                                aOptions,
                                aRequest);
          return;
        }

        this._notifySendingError(error, sentMessage, aSilent, aRequest);
        return false;
      } // End of send failure.

      // Message was sent to SMSC.
      if (!aResponse.deliveryStatus) {
        if (aSilent) {
          // There is no way to modify nsIDOMMozSmsMessage attributes as they
          // are read only so we just create a new sms instance to send along
          // with the notification.
          aRequest.notifyMessageSent(
            gMobileMessageService.createSmsMessage(sentMessage.id,
                                                   sentMessage.threadId,
                                                   sentMessage.iccId,
                                                   DOM_MOBILE_MESSAGE_DELIVERY_SENT,
                                                   sentMessage.deliveryStatus,
                                                   sentMessage.sender,
                                                   sentMessage.receiver,
                                                   sentMessage.body,
                                                   sentMessage.messageClass,
                                                   sentMessage.timestamp,
                                                   Date.now(),
                                                   0,
                                                   sentMessage.read));
          // We don't wait for SMS-STATUS-REPORT for silent one.
          return false;
        }

        gMobileMessageDatabaseService
          .setMessageDeliveryByMessageId(sentMessage.id,
                                         null,
                                         DOM_MOBILE_MESSAGE_DELIVERY_SENT,
                                         sentMessage.deliveryStatus,
                                         null,
                                         (aRv, aDomMessage) => {
          // TODO bug 832140 handle !Components.isSuccessCode(aRv)

          if (requestStatusReport) {
            // Update the sentMessage and wait for the status report.
            sentMessage = aDomMessage;
          }

          this._broadcastSmsSystemMessage(
            Ci.nsISmsMessenger.NOTIFICATION_TYPE_SENT, aDomMessage);
          aRequest.notifyMessageSent(aDomMessage);
          Services.obs.notifyObservers(aDomMessage, kSmsSentObserverTopic, null);
        });

        // Keep this callback if we have status report waiting.
        return requestStatusReport;
      } // End of Message Sent to SMSC.

      // Got valid deliveryStatus for the delivery to the remote party when
      // the status report is requested.
      gMobileMessageDatabaseService
        .setMessageDeliveryByMessageId(sentMessage.id,
                                       null,
                                       sentMessage.delivery,
                                       aResponse.deliveryStatus,
                                       null,
                                       (aRv, aDomMessage) => {
        // TODO bug 832140 handle !Components.isSuccessCode(aRv)

        let [topic, notificationType] =
          (aResponse.deliveryStatus == RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS)
            ? [kSmsDeliverySuccessObserverTopic,
               Ci.nsISmsMessenger.NOTIFICATION_TYPE_DELIVERY_SUCCESS]
            : [kSmsDeliveryErrorObserverTopic,
               Ci.nsISmsMessenger.NOTIFICATION_TYPE_DELIVERY_ERROR];

        // Broadcasting a "sms-delivery-success/sms-delivery-error" system
        // message to open apps.
        this._broadcastSmsSystemMessage(notificationType, aDomMessage);

        // Notifying observers the delivery status is updated.
        Services.obs.notifyObservers(aDomMessage, topic, null);
      });

      // Send transaction has ended completely.
      return false;
    });
  },

  /**
   * A helper to broadcast the system message to launch registered apps
   * like Costcontrol, Notification and Message app... etc.
   *
   * @param aNotificationType
   *        Ci.nsISmsMessenger.NOTIFICATION_TYPE_*.
   * @param aDomMessage
   *        The nsIDOMMozSmsMessage object.
   */
  _broadcastSmsSystemMessage: function(aNotificationType, aDomMessage) {
    if (DEBUG) debug("Broadcasting the SMS system message: " + aNotificationType);

    // Sadly we cannot directly broadcast the aDomMessage object
    // because the system message mechamism will rewrap the object
    // based on the content window, which needs to know the properties.
    try {
      gSmsMessenger.notifySms(aNotificationType,
                              aDomMessage.id,
                              aDomMessage.threadId,
                              aDomMessage.iccId,
                              this._convertSmsDelivery(
                                aDomMessage.delivery),
                              this._convertSmsDeliveryStatus(
                                aDomMessage.deliveryStatus),
                              aDomMessage.sender,
                              aDomMessage.receiver,
                              aDomMessage.body,
                              this._convertSmsMessageClass(
                                aDomMessage.messageClass),
                              aDomMessage.timestamp,
                              aDomMessage.sentTimestamp,
                              aDomMessage.deliveryTimestamp,
                              aDomMessage.read);
    } catch (e) {
      if (DEBUG) {
        debug("Failed to _broadcastSmsSystemMessage: " + e);
      }
    }
  },

  /**
   * Helper for processing received multipart SMS.
   *
   * @return null for handled segments, and an object containing full message
   *         body/data once all segments are received.
   *
   * |_receivedSmsSegmentsMap|:
   *   Hash map for received multipart sms fragments. Messages are hashed with
   *   its sender address and concatenation reference number. Three additional
   *   attributes `segmentMaxSeq`, `receivedSegments`, `segments` are inserted.
   */
  _receivedSmsSegmentsMap: null,
  _processReceivedSmsSegment: function(aSegment) {
    // Directly replace full message body for single SMS.
    if (!(aSegment.segmentMaxSeq && (aSegment.segmentMaxSeq > 1))) {
      if (aSegment.encoding == Ci.nsIGonkSmsService.SMS_MESSAGE_ENCODING_8BITS_ALPHABET) {
        aSegment.fullData = aSegment.data;
      } else {
        aSegment.fullBody = aSegment.body;
      }
      return aSegment;
    }

    // Handle Concatenation for Class 0 SMS
    let hash = aSegment.sender + ":" +
               aSegment.segmentRef + ":" +
               aSegment.segmentMaxSeq;
    let seq = aSegment.segmentSeq;

    let options = this._receivedSmsSegmentsMap[hash];
    if (!options) {
      options = aSegment;
      this._receivedSmsSegmentsMap[hash] = options;

      options.receivedSegments = 0;
      options.segments = [];
    } else if (options.segments[seq]) {
      if (options.encoding == Ci.nsIGonkSmsService.SMS_MESSAGE_ENCODING_8BITS_ALPHABET &&
          options.encoding == aSegment.encoding &&
          options.segments[seq].length == aSegment.data.length &&
          options.segments[seq].every(function(aElement, aIndex) {
            return aElement == aSegment.data[aIndex];
          })) {
        if (DEBUG) {
          debug("Got duplicated binary segment no: " + seq);
        }
        return null;
      }

      if (options.encoding != Ci.nsIGonkSmsService.SMS_MESSAGE_ENCODING_8BITS_ALPHABET &&
          aSegment.encoding != Ci.nsIGonkSmsService.SMS_MESSAGE_ENCODING_8BITS_ALPHABET &&
          options.segments[seq] == aSegment.body) {
        if (DEBUG) {
          debug("Got duplicated text segment no: " + seq);
        }
        return null;
      }

      // Update mandatory properties to ensure that the segments could be
      // concatenated properly.
      options.encoding = aSegment.encoding;
      options.originatorPort = aSegment.originatorPort;
      options.destinationPort = aSegment.destinationPort;
      options.teleservice = aSegment.teleservice;
      // Decrease the counter for this collided segment.
      options.receivedSegments--;
    }

    if (options.receivedSegments > 0) {
      // Update received timestamp.
      options.timestamp = aSegment.timestamp;
    }

    if (options.encoding == Ci.nsIGonkSmsService.SMS_MESSAGE_ENCODING_8BITS_ALPHABET) {
      options.segments[seq] = aSegment.data;
    } else {
      options.segments[seq] = aSegment.body;
    }
    options.receivedSegments++;

    // The port information is only available in 1st segment for CDMA WAP Push.
    // If the segments of a WAP Push are not received in sequence
    // (e.g., SMS with seq == 1 is not the 1st segment received by the device),
    // we have to retrieve the port information from 1st segment and
    // save it into the cached options.
    if (aSegment.teleservice === RIL.PDU_CDMA_MSG_TELESERIVCIE_ID_WAP
        && seq === 1) {
      if (options.originatorPort === Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID
          && aSegment.originatorPort !== Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID) {
        options.originatorPort = aSegment.originatorPort;
      }

      if (options.destinationPort === Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID
          && aSegment.destinationPort !== Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID) {
        options.destinationPort = aSegment.destinationPort;
      }
    }

    if (options.receivedSegments < options.segmentMaxSeq) {
      if (DEBUG) {
        debug("Got segment no." + seq + " of a multipart SMS: " +
                           JSON.stringify(options));
      }
      return null;
    }

    // Remove from map
    delete this._receivedSmsSegmentsMap[hash];

    // Rebuild full body
    if (options.encoding == Ci.nsIGonkSmsService.SMS_MESSAGE_ENCODING_8BITS_ALPHABET) {
      // Uint8Array doesn't have `concat`, so we have to merge all segements
      // by hand.
      let fullDataLen = 0;
      for (let i = 1; i <= options.segmentMaxSeq; i++) {
        fullDataLen += options.segments[i].length;
      }

      options.fullData = new Uint8Array(fullDataLen);
      for (let d= 0, i = 1; i <= options.segmentMaxSeq; i++) {
        let data = options.segments[i];
        for (let j = 0; j < data.length; j++) {
          options.fullData[d++] = data[j];
        }
      }
    } else {
      options.fullBody = options.segments.join("");
    }

    // Remove handy fields after completing the concatenation.
    delete options.receivedSegments;
    delete options.segments;

    if (DEBUG) {
      debug("Got full multipart SMS: " + JSON.stringify(options));
    }

    return options;
  },

  /**
   * Helper to purge complete message.
   *
   * We remove unnessary fields after completing the concatenation.
   */
  _purgeCompleteSmsMessage: function(aMessage) {
    // Purge concatenation info
    delete aMessage.segmentRef;
    delete aMessage.segmentSeq;
    delete aMessage.segmentMaxSeq;

    // Purge partial message body
    delete aMessage.data;
    delete aMessage.body;
  },

  /**
   * Handle WDP port push PDU. Constructor WDP bearer information and deliver
   * to WapPushManager.
   *
   * @param aMessage
   *        A SMS message.
   */
  _handleSmsWdpPortPush: function(aMessage, aServiceId) {
    if (aMessage.encoding != Ci.nsIGonkSmsService.SMS_MESSAGE_ENCODING_8BITS_ALPHABET) {
      if (DEBUG) {
        debug("Got port addressed SMS but not encoded in 8-bit alphabet." +
                   " Drop!");
      }
      return;
    }

    let options = {
      bearer: gWAP.WDP_BEARER_GSM_SMS_GSM_MSISDN,
      sourceAddress: aMessage.sender,
      sourcePort: aMessage.originatorPort,
      destinationAddress: this._getPhoneNumber(aServiceId),
      destinationPort: aMessage.destinationPort,
      serviceId: aServiceId
    };
    gWAP.WapPushManager.receiveWdpPDU(aMessage.fullData, aMessage.fullData.length,
                                     0, options);
  },

  _handleCellbroadcastMessageReceived: function(aMessage, aServiceId) {
    gCellBroadcastService
      .notifyMessageReceived(aServiceId,
                             Ci.nsICellBroadcastService.GSM_GEOGRAPHICAL_SCOPE_INVALID,
                             aMessage.messageCode,
                             aMessage.messageId,
                             aMessage.language,
                             aMessage.fullBody,
                             Ci.nsICellBroadcastService.GSM_MESSAGE_CLASS_NORMAL,
                             Date.now(),
                             aMessage.serviceCategory,
                             false,
                             Ci.nsICellBroadcastService.GSM_ETWS_WARNING_INVALID,
                             false,
                             false);
  },

  _handleMwis: function(aMwi, aServiceId) {
    let service = Cc["@mozilla.org/voicemail/voicemailservice;1"]
                  .getService(Ci.nsIGonkVoicemailService);
    service.notifyStatusChanged(aServiceId, aMwi.active, aMwi.msgCount,
                                aMwi.returnNumber, aMwi.returnMessage);

    gRadioInterfaces[aServiceId].sendWorkerMessage("updateMwis", { mwi: aMwi });
  },

  _portAddressedSmsApps: null,
  _handleSmsReceived: function(aMessage, aServiceId) {
    if (DEBUG) debug("_handleSmsReceived: " + JSON.stringify(aMessage));

    if (aMessage.messageType == RIL.PDU_CDMA_MSG_TYPE_BROADCAST) {
      this._handleCellbroadcastMessageReceived(aMessage, aServiceId);
      return true;
    }

    // Dispatch to registered handler if application port addressing is
    // available. Note that the destination port can possibly be zero when
    // representing a UDP/TCP port.
    if (aMessage.destinationPort !== Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID) {
      let handler = this._portAddressedSmsApps[aMessage.destinationPort];
      if (handler) {
        handler(aMessage, aServiceId);
      }
      return true;
    }

    if (aMessage.encoding == Ci.nsIGonkSmsService.SMS_MESSAGE_ENCODING_8BITS_ALPHABET) {
      // Don't know how to handle binary data yet.
      return true;
    }

    aMessage.type = "sms";
    aMessage.sender = aMessage.sender || null;
    aMessage.receiver = this._getPhoneNumber(aServiceId);
    aMessage.body = aMessage.fullBody = aMessage.fullBody || null;

    if (this._isSilentNumber(aMessage.sender)) {
      aMessage.id = -1;
      aMessage.threadId = 0;
      aMessage.delivery = DOM_MOBILE_MESSAGE_DELIVERY_RECEIVED;
      aMessage.deliveryStatus = RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS;
      aMessage.read = false;

      let domMessage =
        gMobileMessageService.createSmsMessage(aMessage.id,
                                               aMessage.threadId,
                                               aMessage.iccId,
                                               aMessage.delivery,
                                               aMessage.deliveryStatus,
                                               aMessage.sender,
                                               aMessage.receiver,
                                               aMessage.body,
                                               aMessage.messageClass,
                                               aMessage.timestamp,
                                               aMessage.sentTimestamp,
                                               0,
                                               aMessage.read);

      Services.obs.notifyObservers(domMessage,
                                   kSilentSmsReceivedObserverTopic,
                                   null);
      return true;
    }

    if (aMessage.mwiPresent) {
      let mwi = {
        discard: aMessage.mwiDiscard,
        msgCount: aMessage.mwiMsgCount,
        active: aMessage.mwiActive,
        returnNumber: aMessage.sender || null,
        returnMessage: aMessage.fullBody || null
      };

      this._handleMwis(mwi, aServiceId);

      // Dicarded MWI comes without text body.
      // Hence, we discard it here after notifying the MWI status.
      if (aMessage.mwiDiscard) {
        return true;
      }
    }

    let notifyReceived = (aRv, aDomMessage) => {
      let success = Components.isSuccessCode(aRv);

      this._sendAckSms(aRv, aMessage, aServiceId);

      if (!success) {
        // At this point we could send a message to content to notify the user
        // that storing an incoming SMS failed, most likely due to a full disk.
        if (DEBUG) {
          debug("Could not store SMS, error code " + aRv);
        }
        return;
      }

      this._broadcastSmsSystemMessage(
        Ci.nsISmsMessenger.NOTIFICATION_TYPE_RECEIVED, aDomMessage);
      Services.obs.notifyObservers(aDomMessage, kSmsReceivedObserverTopic, null);
    };

    if (aMessage.messageClass != RIL.GECKO_SMS_MESSAGE_CLASSES[RIL.PDU_DCS_MSG_CLASS_0]) {
      gMobileMessageDatabaseService.saveReceivedMessage(aMessage,
                                                        notifyReceived);
    } else {
      aMessage.id = -1;
      aMessage.threadId = 0;
      aMessage.delivery = DOM_MOBILE_MESSAGE_DELIVERY_RECEIVED;
      aMessage.deliveryStatus = RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS;
      aMessage.read = false;

      let domMessage =
        gMobileMessageService.createSmsMessage(aMessage.id,
                                               aMessage.threadId,
                                               aMessage.iccId,
                                               aMessage.delivery,
                                               aMessage.deliveryStatus,
                                               aMessage.sender,
                                               aMessage.receiver,
                                               aMessage.body,
                                               aMessage.messageClass,
                                               aMessage.timestamp,
                                               aMessage.sentTimestamp,
                                               0,
                                               aMessage.read);

      notifyReceived(Cr.NS_OK, domMessage);
    }

    // SMS ACK will be sent in notifyReceived. Return false here.
    return false;
  },

  /**
   * Handle ACK response of received SMS.
   */
  _sendAckSms: function(aRv, aMessage, aServiceId) {
    if (aMessage.messageClass === RIL.GECKO_SMS_MESSAGE_CLASSES[RIL.PDU_DCS_MSG_CLASS_2]) {
      return;
    }

    let result = RIL.PDU_FCS_OK;
    if (!Components.isSuccessCode(aRv)) {
      if (DEBUG) debug("Failed to handle received sms: " + aRv);
      result = (aRv === Cr.NS_ERROR_FILE_NO_DEVICE_SPACE)
                ? RIL.PDU_FCS_MEMORY_CAPACITY_EXCEEDED
                : RIL.PDU_FCS_UNSPECIFIED;
    }

    gRadioInterfaces[aServiceId]
      .sendWorkerMessage("ackSMS", { result: result });

  },

  /**
   * Report SMS storage status to modem.
   *
   * Note: GonkDiskSpaceWatcher repeats the notification every 5 seconds when
   *       storage is full.
   *       Report status to modem only when the availability is changed.
   *       Set |_smsStorageAvailable| to |null| to ensure the first run after
   *       bootup.
   */
  _smsStorageAvailable: null,
  _reportSmsMemoryStatus: function(aIsAvailable) {
    if (this._smsStorageAvailable !== aIsAvailable) {
      this._smsStorageAvailable = aIsAvailable;
      for (let serviceId = 0; serviceId < gRadioInterfaces.length; serviceId++) {
        gRadioInterfaces[serviceId]
          .sendWorkerMessage("reportSmsMemoryStatus", { isAvailable: aIsAvailable });
      }
    }
  },

  // An array of silent numbers.
  _silentNumbers: null,
  _isSilentNumber: function(aNumber) {
    return this._silentNumbers.indexOf(aNumber) >= 0;
  },

  /**
   * nsISmsService interface
   */
  smsDefaultServiceId: 0,

  getSegmentInfoForText: function(aText, aRequest) {
    let strict7BitEncoding;
    try {
      strict7BitEncoding = Services.prefs.getBoolPref("dom.sms.strict7BitEncoding");
    } catch (e) {
      strict7BitEncoding = false;
    }

    let options = gSmsSegmentHelper.fragmentText(aText, null, strict7BitEncoding);
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

    aRequest.notifySegmentInfoForTextGot(options.segmentMaxSeq,
                                         options.segmentChars,
                                         options.segmentChars - charsInLastSegment);
  },

  send: function(aServiceId, aNumber, aMessage, aSilent, aRequest) {
    if (aServiceId > (gRadioInterfaces.length - 1)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let strict7BitEncoding;
    try {
      strict7BitEncoding = Services.prefs.getBoolPref("dom.sms.strict7BitEncoding");
    } catch (e) {
      strict7BitEncoding = false;
    }

    let options = gSmsSegmentHelper.fragmentText(aMessage, null, strict7BitEncoding);
    options.number = gPhoneNumberUtils.normalize(aNumber);
    let requestStatusReport;
    try {
      requestStatusReport =
        Services.prefs.getBoolPref("dom.sms.requestStatusReport");
    } catch (e) {
      requestStatusReport = true;
    }
    options.requestStatusReport = requestStatusReport && !aSilent;

    let sendingMessage = {
      type: "sms",
      sender: this._getPhoneNumber(aServiceId),
      receiver: aNumber,
      body: aMessage,
      deliveryStatusRequested: options.requestStatusReport,
      timestamp: Date.now(),
      iccId: this._getIccId(aServiceId)
    };

    let saveSendingMessageCallback = (aRv, aSendingMessage) => {
      if (!Components.isSuccessCode(aRv)) {
        if (DEBUG) debug("Error! Fail to save sending message! aRv = " + aRv);
        this._broadcastSmsSystemMessage(
          Ci.nsISmsMessenger.NOTIFICATION_TYPE_SENT_FAILED, aSendingMessage);
        aRequest.notifySendMessageFailed(
          gMobileMessageDatabaseService.translateCrErrorToMessageCallbackError(aRv),
          aSendingMessage);
        Services.obs.notifyObservers(aSendingMessage, kSmsFailedObserverTopic, null);
        return;
      }

      if (!aSilent) {
        Services.obs.notifyObservers(aSendingMessage, kSmsSendingObserverTopic, null);
      }

      let connection =
        gMobileConnectionService.getItemByServiceId(aServiceId);
      // If the radio is disabled or the SIM card is not ready, just directly
      // return with the corresponding error code.
      let errorCode;
      let radioState = connection && connection.radioState;
      if (!gPhoneNumberUtils.isPlainPhoneNumber(options.number)) {
        if (DEBUG) debug("Error! Address is invalid when sending SMS: " + options.number);
        errorCode = Ci.nsIMobileMessageCallback.INVALID_ADDRESS_ERROR;
      } else if (radioState == Ci.nsIMobileConnection.MOBILE_RADIO_STATE_UNKNOWN ||
                 radioState == Ci.nsIMobileConnection.MOBILE_RADIO_STATE_DISABLED) {
        if (DEBUG) debug("Error! Radio is disabled when sending SMS.");
        errorCode = Ci.nsIMobileMessageCallback.RADIO_DISABLED_ERROR;
      } else if (this._getCardState(aServiceId) != Ci.nsIIcc.CARD_STATE_READY) {
        if (DEBUG) debug("Error! SIM card is not ready when sending SMS.");
        errorCode = Ci.nsIMobileMessageCallback.NO_SIM_CARD_ERROR;
      }
      if (errorCode) {
        this._notifySendingError(errorCode, aSendingMessage, aSilent, aRequest);
        return;
      }

      this._scheduleSending(aServiceId, aSendingMessage, aSilent, options,
        aRequest);
    }; // End of |saveSendingMessageCallback|.

    // Don't save message into DB for silent SMS.
    if (aSilent) {
      let delivery = DOM_MOBILE_MESSAGE_DELIVERY_SENDING;
      let deliveryStatus = RIL.GECKO_SMS_DELIVERY_STATUS_PENDING;
      let domMessage =
        gMobileMessageService.createSmsMessage(-1, // id
                                               0,  // threadId
                                               sendingMessage.iccId,
                                               delivery,
                                               deliveryStatus,
                                               sendingMessage.sender,
                                               sendingMessage.receiver,
                                               sendingMessage.body,
                                               "normal", // message class
                                               sendingMessage.timestamp,
                                               0,
                                               0,
                                               false);
      saveSendingMessageCallback(Cr.NS_OK, domMessage);
      return;
    }

    gMobileMessageDatabaseService.saveSendingMessage(
      sendingMessage, saveSendingMessageCallback);
  },

  addSilentNumber: function(aNumber) {
    if (this._isSilentNumber(aNumber)) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this._silentNumbers.push(aNumber);
  },

  removeSilentNumber: function(aNumber) {
   let index = this._silentNumbers.indexOf(aNumber);
   if (index < 0) {
     throw Cr.NS_ERROR_INVALID_ARG;
   }

   this._silentNumbers.splice(index, 1);
  },

  getSmscAddress: function(aServiceId, aRequest) {
    if (aServiceId > (gRadioInterfaces.length - 1)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    gRadioInterfaces[aServiceId].sendWorkerMessage("getSmscAddress",
                                                   null,
                                                   (aResponse) => {
      if (!aResponse.errorMsg) {
        aRequest.notifyGetSmscAddress(aResponse.smscAddress,
                                      aResponse.typeOfNumber,
                                      aResponse.numberPlanIdentification);
      } else {
        aRequest.notifyGetSmscAddressFailed(
          Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR);
      }
    });
  },

  setSmscAddress: function(aServiceId, aNumber, aTypeOfNumber,
                      aNumberPlanIdentification, aRequest) {
    if (aServiceId > (gRadioInterfaces.length - 1)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let options = {
      smscAddress: aNumber,
      typeOfNumber: aTypeOfNumber,
      numberPlanIdentification: aNumberPlanIdentification
    };

    gRadioInterfaces[aServiceId].sendWorkerMessage("setSmscAddress",
                                                   options,
                                                   (aResponse) => {
      if (!aResponse.errorMsg) {
        aRequest.notifySetSmscAddress();
      } else {
        aRequest.notifySetSmscAddressFailed(
          Ci.nsIMobileMessageCallback.INVALID_ADDRESS_ERROR);
      }
    });
  },

  /**
   * nsIGonkSmsService interface
   */
  notifyMessageReceived: function(aServiceId, aSMSC, aSentTimestamp,
                                  aSender, aPid, aEncoding, aMessageClass,
                                  aLanguage, aSegmentRef, aSegmentSeq,
                                  aSegmentMaxSeq, aOriginatorPort,
                                  aDestinationPort, aMwiPresent, aMwiDiscard,
                                  aMwiMsgCount, aMwiActive, aCdmaMessageType,
                                  aCdmaTeleservice, aCdmaServiceCategory,
                                  aBody, aData, aDataLength) {

    this._acquireSmsHandledWakeLock();

    let segment = {};
    segment.iccId = this._getIccId(aServiceId);
    segment.SMSC = aSMSC;
    segment.sentTimestamp = aSentTimestamp;
    segment.timestamp = Date.now();
    segment.sender = aSender;
    segment.pid = aPid;
    segment.encoding = aEncoding;
    segment.messageClass = this._convertSmsMessageClassToString(aMessageClass);
    segment.language = aLanguage;
    segment.segmentRef = aSegmentRef;
    segment.segmentSeq = aSegmentSeq;
    segment.segmentMaxSeq = aSegmentMaxSeq;
    segment.originatorPort = aOriginatorPort;
    segment.destinationPort = aDestinationPort;
    segment.mwiPresent = aMwiPresent;
    segment.mwiDiscard = aMwiDiscard;
    segment.mwiMsgCount = aMwiMsgCount;
    segment.mwiActive = aMwiActive;
    segment.messageType = aCdmaMessageType;
    segment.teleservice = aCdmaTeleservice;
    segment.serviceCategory = aCdmaServiceCategory;
    segment.body = aBody;
    segment.data = (aData && aDataLength > 0) ? aData : null;

    let isMultipart = (segment.segmentMaxSeq && (segment.segmentMaxSeq > 1));
    let messageClass = segment.messageClass;

    let handleReceivedAndAck = (aRvOfIncompleteMsg, aCompleteMessage) => {
      if (aCompleteMessage) {
        this._purgeCompleteSmsMessage(aCompleteMessage);
        if (this._handleSmsReceived(aCompleteMessage, aServiceId)) {
          this._sendAckSms(Cr.NS_OK, aCompleteMessage, aServiceId);
        }
        // else Ack will be sent after further process in _handleSmsReceived.
      } else {
        this._sendAckSms(aRvOfIncompleteMsg, segment, aServiceId);
      }
    };

    // No need to access SmsSegmentStore for Class 0 SMS and Single SMS.
    if (!isMultipart ||
        (messageClass == RIL.GECKO_SMS_MESSAGE_CLASSES[RIL.PDU_DCS_MSG_CLASS_0])) {
      // `When a mobile terminated message is class 0 and the MS has the
      // capability of displaying short messages, the MS shall display the
      // message immediately and send an acknowledgement to the SC when the
      // message has successfully reached the MS irrespective of whether
      // there is memory available in the (U)SIM or ME. The message shall
      // not be automatically stored in the (U)SIM or ME.`
      // ~ 3GPP 23.038 clause 4

      handleReceivedAndAck(Cr.NS_OK,  // ACK OK For Incomplete Class 0
                           this._processReceivedSmsSegment(segment));
    } else {
      gMobileMessageDatabaseService
        .saveSmsSegment(segment, (aRv, aCompleteMessage) => {
        handleReceivedAndAck(aRv,  // Ack according to the result after saving
                             aCompleteMessage);
      });
    }
  },

  /**
   * nsIObserver interface.
   */
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === kPrefRilDebuggingEnabled) {
          this._updateDebugFlag();
        }
        else if (aData === kPrefDefaultServiceId) {
          this.smsDefaultServiceId = this._getDefaultServiceId();
        }
        else if ( aData === kPrefLastKnownSimMcc) {
          gSmsSegmentHelper.enabledGsmTableTuples =
            getEnabledGsmTableTuplesFromMcc();
        }
        break;
      case kDiskSpaceWatcherObserverTopic:
        if (DEBUG) {
          debug("Observe " + kDiskSpaceWatcherObserverTopic + ": " + aData);
        }
        this._reportSmsMemoryStatus(aData != "full");
        break;
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        // Release the CPU wake lock for handling the received SMS.
        this._releaseSmsHandledWakeLock();
        Services.prefs.removeObserver(kPrefRilDebuggingEnabled, this);
        Services.prefs.removeObserver(kPrefDefaultServiceId, this);
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        Services.obs.removeObserver(this, kDiskSpaceWatcherObserverTopic);
        break;
    }
  }
};

/**
 * Get enabled GSM national language locking shift / single shift table pairs
 * for current SIM MCC.
 *
 * @return a list of pairs of national language identifiers for locking shift
 * table and single shfit table, respectively.
 */
function getEnabledGsmTableTuplesFromMcc() {
  let mcc;
  try {
    mcc = Services.prefs.getCharPref(kPrefLastKnownSimMcc);
  } catch (e) {}
  let tuples = [[RIL.PDU_NL_IDENTIFIER_DEFAULT,
    RIL.PDU_NL_IDENTIFIER_DEFAULT]];
  let extraTuples = RIL.PDU_MCC_NL_TABLE_TUPLES_MAPPING[mcc];
  if (extraTuples) {
    tuples = tuples.concat(extraTuples);
  }

  return tuples;
};

function SmsSendingScheduler(aServiceId) {
  this._serviceId = aServiceId;
  this._queue = [];

  Services.obs.addObserver(this, kSmsDeletedObserverTopic, false);
  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}
SmsSendingScheduler.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnectionListener]),

  _serviceId: 0,
  _isObservingMoboConn: false,
  _queue: null,

  /**
   * Ensure the handler is listening on MobileConnection events.
   */
  _ensureMoboConnObserverRegistration: function() {
    if (!this._isObservingMoboConn) {
      let connection =
        gMobileConnectionService.getItemByServiceId(this._serviceId);
      if (connection) {
        connection.registerListener(this);
        this._isObservingMoboConn = true;
      }
    }
  },

  /**
   * Ensure the handler is not listening on MobileConnection events.
   */
  _ensureMoboConnObserverUnregistration: function() {
    if (this._isObservingMoboConn) {
      let connection =
        gMobileConnectionService.getItemByServiceId(this._serviceId);
      if (connection) {
        connection.unregisterListener(this);
        this._isObservingMoboConn = false;
      }
    }
  },

  /**
   * Schedule a sending request.
   *
   * @param aSendingRequest
   *        An object with the following properties:
   *        - messageId
   *          The messageId in MobileMessageDB.
   *        - onSend
   *          The callback to invoke to trigger a sending or retry.
   *        - onCancel
   *          The callback to invoke when the request is canceled and won't
   *          retry.
   */
  schedule: function(aSendingRequest) {
    if (aSendingRequest) {
      if (DEBUG) {
        debug("scheduling message: messageId=" + aSendingRequest.messageId +
              ", serviceId=" + this._serviceId);
      }
      this._ensureMoboConnObserverRegistration();
      this._queue.push(aSendingRequest)

      // Keep the queue in order to guarantee the sending order matches user
      // expectation.
      this._queue.sort(function(a, b) {
        return a.messageId - b.messageId;
      });
    }

    this.send();
  },

  /**
   * Send all requests in the queue if voice connection is available.
   */
  send: function() {
    let connection =
      gMobileConnectionService.getItemByServiceId(this._serviceId);

    // If the voice connection is temporarily unavailable, pend the request.
    let voiceInfo = connection && connection.voice;
    let voiceConnected = voiceInfo && voiceInfo.connected;
    if (!voiceConnected) {
      if (DEBUG) {
        debug("Voice connection is temporarily unavailable. Skip sending.");
      }
      return;
    }

    let snapshot = this._queue;
    this._queue = [];
    let req;
    while ((req = snapshot.shift())) {
      req.onSend();
    }

    // The sending / retry could fail and being re-scheduled immediately.
    // Only unregister listener when the queue is empty after retries.
    if (this._queue.length === 0) {
      this._ensureMoboConnObserverUnregistration();
    }
  },

  /**
   * nsIObserver interface.
   */
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case kSmsDeletedObserverTopic:
        let deletedInfo = aSubject.QueryInterface(Ci.nsIDeletedMessageInfo);
        if (DEBUG) {
          debug("Observe " + kSmsDeletedObserverTopic + ": " +
            JSON.stringify(deletedInfo));
        }

        if (deletedInfo && deletedInfo.deletedMessageIds) {
          for (let i = 0; i < this._queue.length; i++) {
            let id = this._queue[i].messageId;
            if (deletedInfo.deletedMessageIds.includes(id)) {
              if (DEBUG) debug("Deleting message with id=" + id);
              this._queue.splice(i, 1)[0].onCancel(
                Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR);
            }
          }
        }
        break;
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        this._ensureMoboConnObserverUnregistration();
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        Services.obs.removeObserver(this, kSmsDeletedObserverTopic);

        // Cancel all pending requests and clear the queue.
        for (let req of this._queue) {
          req.onCancel(Ci.nsIMobileMessageCallback.NO_SIGNAL_ERROR);
        }
        this._queue = [];
        break;
    }
  },

  /**
   * nsIMobileConnectionListener implementation.
   */
  notifyVoiceChanged: function() {
    let connection = gMobileConnectionService.getItemByServiceId(this._serviceId);
    let voiceInfo = connection && connection.voice;
    let voiceConnected = voiceInfo && voiceInfo.connected;
    if (voiceConnected) {
      if (DEBUG) {
        debug("Voice connected. Resend pending requests.");
      }

      this.send();
    }
  },

  // Unused nsIMobileConnectionListener methods.
  notifyDataChanged: function() {},
  notifyDataError: function(message) {},
  notifyCFStateChanged: function(action, reason, number, timeSeconds, serviceClass) {},
  notifyEmergencyCbModeChanged: function(active, timeoutMs) {},
  notifyOtaStatusChanged: function(status) {},
  notifyRadioStateChanged: function() {},
  notifyClirModeChanged: function(mode) {},
  notifyLastKnownNetworkChanged: function() {},
  notifyLastKnownHomeNetworkChanged: function() {},
  notifyNetworkSelectionModeChanged: function() {}
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SmsService]);
