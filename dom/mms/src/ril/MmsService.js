/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

const RIL_MMSSERVICE_CONTRACTID = "@mozilla.org/mms/rilmmsservice;1";
const RIL_MMSSERVICE_CID = Components.ID("{217ddd76-75db-4210-955d-8806cd8d87f9}");

const DEBUG = false;

const kNetworkInterfaceStateChangedTopic = "network-interface-state-changed";
const kXpcomShutdownObserverTopic        = "xpcom-shutdown";

// File modes for saving MMS attachments.
const FILE_OPEN_MODE = FileUtils.MODE_CREATE
                     | FileUtils.MODE_WRONLY
                     | FileUtils.MODE_TRUNCATE;

// Size of each segment in a nsIStorageStream. Must be a power of two.
const STORAGE_STREAM_SEGMENT_SIZE = 4096;

// HTTP status codes:
// @see http://tools.ietf.org/html/rfc2616#page-39
const HTTP_STATUS_OK = 200;

const CONFIG_SEND_REPORT_NEVER       = 0;
const CONFIG_SEND_REPORT_DEFAULT_NO  = 1;
const CONFIG_SEND_REPORT_DEFAULT_YES = 2;
const CONFIG_SEND_REPORT_ALWAYS      = 3;

XPCOMUtils.defineLazyServiceGetter(this, "gpps",
                                   "@mozilla.org/network/protocol-proxy-service;1",
                                   "nsIProtocolProxyService");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyGetter(this, "MMS", function () {
  let MMS = {};
  Cu.import("resource://gre/modules/MmsPduHelper.jsm", MMS);
  return MMS;
});

/**
 * MmsService
 */
function MmsService() {
  Services.obs.addObserver(this, kXpcomShutdownObserverTopic, false);
  Services.obs.addObserver(this, kNetworkInterfaceStateChangedTopic, false);
}
MmsService.prototype = {

  classID:   RIL_MMSSERVICE_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMmsService,
                                         Ci.nsIWapPushApplication,
                                         Ci.nsIObserver,
                                         Ci.nsIProtocolProxyFilter]),

  /**
   * Whether or not should we enable X-Mms-Report-Allowed in M-NotifyResp.ind
   * and M-Acknowledge.ind PDU.
   */
  confSendDeliveryReport: CONFIG_SEND_REPORT_DEFAULT_YES,

  proxyInfo: null,
  MMSC: null,

  /** MMS proxy filter reference count. */
  proxyFilterRefCount: 0,

  /**
   * Calculate Whether or not should we enable X-Mms-Report-Allowed.
   *
   * @param config
   *        Current config value.
   * @param wish
   *        Sender wish. Could be undefined, false, or true.
   */
  getReportAllowed: function getReportAllowed(config, wish) {
    if ((config == CONFIG_SEND_REPORT_DEFAULT_NO)
        || (config == CONFIG_SEND_REPORT_DEFAULT_YES)) {
      if (wish != null) {
        config += (wish ? 1 : -1);
      }
    }
    return config >= CONFIG_SEND_REPORT_DEFAULT_YES;
  },

  /**
   * Acquire referece-counted MMS proxy filter.
   */
  acquireProxyFilter: function acquireProxyFilter() {
    if (!this.proxyFilterRefCount) {
      debug("Register proxy filter");
      gpps.registerFilter(this, 0);
    }
    this.proxyFilterRefCount++;
  },

  /**
   * Release referece-counted MMS proxy filter.
   */
  releaseProxyFilter: function releaseProxyFilter() {
    this.proxyFilterRefCount--;
    if (this.proxyFilterRefCount <= 0) {
      this.proxyFilterRefCount = 0;

      debug("Unregister proxy filter");
      gpps.unregisterFilter(this);
    }
  },

  /**
   * Send MMS request to MMSC.
   *
   * @param method
   *        "GET" or "POST".
   * @param url
   *        Target url string.
   * @param istream [optional]
   *        An nsIInputStream instance as data source to be sent.
   * @param callback
   *        A callback function that takes two arguments: one for http status,
   *        the other for wrapped PDU data for further parsing.
   */
  sendMmsRequest: function sendMmsRequest(method, url, istream, callback) {
    let that = this;
    function releaseProxyFilterAndCallback(status, data) {
      // Always release proxy filter before callback.
      that.releaseProxyFilter(false);
      if (callback) {
        callback(status, data);
      }
    }

    this.acquireProxyFilter();

    try {
      let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);

      // Basic setups
      xhr.open(method, url, true);
      xhr.responseType = "arraybuffer";
      if (istream) {
        xhr.setRequestHeader("Content-Type", "application/vnd.wap.mms-message");
        xhr.setRequestHeader("Content-Length", istream.available());
      } else {
        xhr.setRequestHeader("Content-Length", 0);
      }

      // Setup event listeners
      xhr.onerror = function () {
        debug("xhr error, response headers: " + xhr.getAllResponseHeaders());
        releaseProxyFilterAndCallback(xhr.status, null);
      };
      xhr.onreadystatechange = function () {
        if (xhr.readyState != Ci.nsIXMLHttpRequest.DONE) {
          return;
        }

        let data = null;
        switch (xhr.status) {
          case HTTP_STATUS_OK: {
            debug("xhr success, response headers: " + xhr.getAllResponseHeaders());

            let array = new Uint8Array(xhr.response);
            if (false) {
              for (let begin = 0; begin < array.length; begin += 20) {
                debug("res: " + JSON.stringify(array.subarray(begin, begin + 20)));
              }
            }

            data = {array: array, offset: 0};
            break;
          }
          default: {
            debug("xhr done, but status = " + xhr.status);
            break;
          }
        }

        releaseProxyFilterAndCallback(xhr.status, data);
      }

      // Send request
      xhr.send(istream);
    } catch (e) {
      debug("xhr error, can't send: " + e.message);
      releaseProxyFilterAndCallback(0, null);
    }
  },

  /**
   * Send M-NotifyResp.ind back to MMSC.
   *
   * @param tid
   *        X-Mms-Transaction-ID of the message.
   * @param status
   *        X-Mms-Status of the response.
   * @param ra
   *        X-Mms-Report-Allowed of the response.
   *
   * @see OMA-TS-MMS_ENC-V1_3-20110913-A section 6.2
   */
  sendNotificationResponse: function sendNotificationResponse(tid, status, ra) {
    debug("sendNotificationResponse: tid = " + tid + ", status = " + status
          + ", reportAllowed = " + ra);

    let headers = {};

    // Mandatory fields
    headers["x-mms-message-type"] = MMS.MMS_PDU_TYPE_NOTIFYRESP_IND;
    headers["x-mms-transaction-id"] = tid;
    headers["x-mms-mms-version"] = MMS.MMS_VERSION;
    headers["x-mms-status"] = status;
    // Optional fields
    headers["x-mms-report-allowed"] = ra;

    let istream = MMS.PduHelper.compose(null, {headers: headers});
    this.sendMmsRequest("POST", this.MMSC, istream);
  },

  /**
   * Send M-Send.req to MMSC
   */
  sendSendRequest: function sendSendRequest(msg, callback) {
    msg.headers["x-mms-message-type"] = MMS.MMS_PDU_TYPE_SEND_REQ;
    if (!msg.headers["x-mms-transaction-id"]) {
      // Create an unique transaction id
      let tid = gUUIDGenerator.generateUUID().toString();
      msg.headers["x-mms-transaction-id"] = tid;
    }
    msg.headers["x-mms-mms-version"] = MMS.MMS_VERSION;

    // Let MMS Proxy Relay insert from address automatically for us
    msg.headers["from"] = null;

    msg.headers["date"] = new Date();
    msg.headers["x-mms-message-class"] = "personal";
    msg.headers["x-mms-expiry"] = 7 * 24 * 60 * 60;
    msg.headers["x-mms-priority"] = 129;
    msg.headers["x-mms-read-report"] = true;
    msg.headers["x-mms-delivery-report"] = true;

    let messageSize = 0;

    if (msg.content) {
      messageSize = msg.content.length;
    } else if (msg.parts) {
      for (let i = 0; i < msg.parts.length; i++) {
        messageSize += msg.parts[i].content.length;
      }

      let contentType = {
        params: {
          // `The type parameter must be specified and its value is the MIME
          // media type of the "root" body part.` ~ RFC 2387 clause 3.1
          type: msg.parts[0].headers["content-type"].media,
        },
      };

      // `The Content-Type in M-Send.req and M-Retrieve.conf SHALL be
      // application/vnd.wap.multipart.mixed when there is no presentation, and
      // application/vnd.wap.multipart.related SHALL be used when there is SMIL
      // presentation available.` ~ OMA-TS-MMS_CONF-V1_3-20110913-A clause 10.2.1
      if (contentType.params.type === "application/smil") {
        contentType.media = "application/vnd.wap.multipart.related";

        // `The start parameter, if given, is the content-ID of the compound
        // object's "root".` ~ RFC 2387 clause 3.2
        contentType.params.start = msg.parts[0].headers["content-id"];
      } else {
        contentType.media = "application/vnd.wap.multipart.mixed";
      }

      // Assign to Content-Type
      msg.headers["content-type"] = contentType;
    }

    // Assign to X-Mms-Message-Size
    msg.headers["x-mms-message-size"] = messageSize;

    debug("msg: " + JSON.stringify(msg));

    let istream = MMS.PduHelper.compose(null, msg);
    if (!istream) {
      debug("sendSendRequest: failed to compose M-Send.ind PDU");
      callback(MMS.MMS_PDU_ERROR_PERMANENT_FAILURE, null);
      return;
    }

    this.sendMmsRequest("POST", this.MMSC, istream, (function (status, data) {
      if (!data) {
        callback(MMS.MMS_PDU_ERROR_PERMANENT_FAILURE, null);
      } else if (!this.parseStreamAndDispatch(data, {msg: msg, callback: callback})) {
        callback(MMS.MMS_PDU_RESPONSE_ERROR_UNSUPPORTED_MESSAGE, null);
      }
    }).bind(this));
  },

  /**
   * @param file
   *        A nsIFile object indicating where to save the data.
   * @param data
   *        An array of raw octets.
   * @param callback
   *        Callback function when I/O is done.
   *
   * @return An nsIRequest representing the copy operation returned by
   *         NetUtil.asyncCopy().
   */
  saveContentToFile: function saveContentToFile(file, data, callback) {
    // Write to a StorageStream for NetUtil.asyncCopy()
    let sstream = Cc["@mozilla.org/storagestream;1"]
                  .createInstance(Ci.nsIStorageStream);
    sstream.init(STORAGE_STREAM_SEGMENT_SIZE, data.length, null);
    let bostream = Cc["@mozilla.org/binaryoutputstream;1"]
                   .createInstance(Ci.nsIBinaryOutputStream);
    bostream.setOutputStream(sstream.getOutputStream(0));
    bostream.writeByteArray(data, data.length);
    bostream.close();

    // Write message body to file
    let ofstream = FileUtils.openSafeFileOutputStream(file, FILE_OPEN_MODE);
    return NetUtil.asyncCopy(sstream.newInputStream(0), ofstream, callback);
  },

  /**
   * @param msg
   *        A MMS message object.
   * @param callback
   *        A callback function that accepts one argument as retrieved message.
   */
  saveMessageContent: function saveMessageContent(msg, callback) {
    function saveCallback(obj, counter, status) {
      obj.saved = Components.isSuccessCode(status);
      debug("saveMessageContent: " + obj.file.path + ", saved: " + obj.saved);

      // The async copy callback may not be invoked in order, so we only
      // callback after all of them were done.
      counter.count++;
      if (counter.count >= counter.max) {
        if (callback) {
          callback(msg);
        }
      }
    }

    let tid = msg.headers["x-mms-transaction-id"];
    if (msg.parts) {
      let counter = {max: msg.parts.length, count: 0};

      msg.parts.forEach((function (part, index) {
        part.file = FileUtils.getFile("ProfD", ["mms", tid, index], true);
        if (!part.content) {
          saveCallback(part, counter, Cr.NS_ERROR_NOT_AVAILABLE);
        } else {
          this.saveContentToFile(part.file, part.content,
                                 saveCallback.bind(null, part, counter));
        }
      }).bind(this));
    } else if (msg.content) {
      msg.file = FileUtils.getFile("ProfD", ["mms", tid, "content"], true);
      this.saveContentToFile(msg.file, msg.content,
                             saveCallback.bind(null, msg, {max: 1, count: 0}));
    } else {
      // Nothing to save here.
      if (callback) {
        callback(msg);
      }
    }
  },

  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param options
   *        Additional options to be passed to corresponding PDU handler.
   *
   * @return true if incoming data parsed successfully and passed to PDU
   *         handler; false otherwise.
   */
  parseStreamAndDispatch: function parseStreamAndDispatch(data, options) {
    let msg = MMS.PduHelper.parse(data, null);
    if (!msg) {
      return false;
    }
    debug("parseStreamAndDispatch: msg = " + JSON.stringify(msg));

    switch (msg.type) {
      case MMS.MMS_PDU_TYPE_SEND_CONF:
        this.handleSendConfirmation(msg, options);
        break;
      case MMS.MMS_PDU_TYPE_NOTIFICATION_IND:
        this.handleNotificationIndication(msg, options);
        break;
      case MMS.MMS_PDU_TYPE_RETRIEVE_CONF:
        this.handleRetrieveConfirmation(msg, options);
        break;
      case MMS.MMS_PDU_TYPE_DELIVERY_IND:
        this.handleDeliveryIndication(msg, options);
        break;
      default:
        debug("Unsupported X-MMS-Message-Type: " + msg.type);
        return false;
    }

    return true;
  },

  /**
   * Handle incoming M-Send.conf PDU.
   *
   * @param msg
   *        The M-Send.conf message object.
   */
  handleSendConfirmation: function handleSendConfirmation(msg, options) {
    let status = msg.headers["x-mms-response-status"];
    if (status == null) {
      return;
    }

    if (status == MMS.MMS_PDU_ERROR_OK) {
      // `This ID SHALL always be present after the MMS Proxy-Relay accepted
      // the corresponding M-Send.req PDU. The ID enables a MMS Client to match
      // delivery reports or read-report PDUs with previously sent MM.`
      let messageId = msg.headers["message-id"];
      options.msg.headers["message-id"] = messageId;
    } else if ((status >= MMS.MMS_PDU_ERROR_TRANSIENT_FAILURE)
               && (status < MMS.MMS_PDU_ERROR_PERMANENT_FAILURE)) {
      return;
    }

    if (options.callback) {
      options.callback(status, msg);
    }
  },

  /**
   * Handle incoming M-Notification.ind PDU.
   *
   * @param msg
   *        The MMS message object.
   */
  handleNotificationIndication: function handleNotificationIndication(msg) {
    function callback(status, retr) {
      let tid = msg.headers["x-mms-transaction-id"];

      // For X-Mms-Report-Allowed
      let wish = msg.headers["x-mms-delivery-report"];
      // `The absence of the field does not indicate any default value.`
      // So we go checking the same field in retrieved message instead.
      if ((wish == null) && retr) {
        wish = retr.headers["x-mms-delivery-report"];
      }
      let ra = this.getReportAllowed(this.confSendDeliveryReport, wish);

      this.sendNotificationResponse(tid, status, ra);
    }

    function retrCallback(error, retr) {
      callback.call(this, MMS.translatePduErrorToStatus(error), retr);
    }

    let url = msg.headers["x-mms-content-location"].uri;
    this.sendMmsRequest("GET", url, null, (function (status, data) {
      if (!data) {
        callback.call(this, MMS.MMS_PDU_STATUS_DEFERRED, null);
      } else if (!this.parseStreamAndDispatch(data, retrCallback.bind(this))) {
        callback.call(this, MMS.MMS_PDU_STATUS_UNRECOGNISED, null);
      }
    }).bind(this));
  },

  /**
   * Handle incoming M-Retrieve.conf PDU.
   *
   * @param msg
   *        The MMS message object.
   * @param callback
   *        A callback function that accepts one argument as retrieved message.
   */
  handleRetrieveConfirmation: function handleRetrieveConfirmation(msg, callback) {
    function callbackIfValid(status, msg) {
      if (callback) {
        callback(status, msg)
      }
    }

    // Fix default header field values.
    if (msg.headers["x-mms-delivery-report"] == null) {
      msg.headers["x-mms-delivery-report"] = false;
    }

    let status = msg.headers["x-mms-retrieve-status"];
    if ((status != null) && (status != MMS.MMS_PDU_ERROR_OK)) {
      callbackIfValid(status, msg);
      return;
    }

    this.saveMessageContent(msg, callbackIfValid.bind(null, MMS.MMS_PDU_ERROR_OK));
  },

  /**
   * Handle incoming M-Delivery.ind PDU.
   */
  handleDeliveryIndication: function handleDeliveryIndication(msg) {
    let messageId = msg.headers["message-id"];
    debug("handleDeliveryIndication: got delivery report for " + messageId);
  },

  /**
   * Update proxyInfo & MMSC from preferences.
   *
   * @param enabled
   *        Enable or disable MMS proxy.
   */
  updateProxyInfo: function updateProxyInfo(enabled) {
    try {
      if (enabled) {
        this.MMSC = Services.prefs.getCharPref("ril.data.mmsc");
        this.proxyInfo = gpps.newProxyInfo("http",
                                           Services.prefs.getCharPref("ril.data.mmsproxy"),
                                           Services.prefs.getIntPref("ril.data.mmsport"),
                                           Ci.nsIProxyInfo.TRANSPARENT_PROXY_RESOLVES_HOST,
                                           -1, null);
        debug("updateProxyInfo: "
              + JSON.stringify({MMSC: this.MMSC, proxyInfo: this.proxyInfo}));
        return;
      }
    } catch (e) {
      // Failed to refresh proxy info from settings. Fallback to disable.
    }

    this.MMSC = null;
    this.proxyInfo = null;
  },

  // nsIMmsService

  hasSupport: function hasSupport() {
    return true;
  },

  // nsIWapPushApplication

  receiveWapPush: function receiveWapPush(array, length, offset, options) {
    this.parseStreamAndDispatch({array: array, offset: offset});
  },

  // nsIObserver

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case kNetworkInterfaceStateChangedTopic: {
        let iface = subject.QueryInterface(Ci.nsINetworkInterface);
        if ((iface.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE)
            || (iface.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS)) {
          this.updateProxyInfo(iface.state == Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED);
        }
        break;
      }
      case kXpcomShutdownObserverTopic: {
        Services.obs.removeObserver(this, kXpcomShutdownObserverTopic);
        Services.obs.removeObserver(this, kNetworkInterfaceStateChangedTopic);
        break;
      }
    }
  },

  // nsIProtocolProxyFilter

  applyFilter: function applyFilter(service, uri, proxyInfo) {
    if (uri.prePath == this.MMSC) {
      debug("applyFilter: match " + uri.spec);
      return this.proxyInfo;
    }

    return proxyInfo;
  },
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([MmsService]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-@- MmsService: " + s + "\n");
  };
} else {
  debug = function (s) {};
}

