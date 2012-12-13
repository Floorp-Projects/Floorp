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
const kPrefenceChangedObserverTopic      = "nsPref:changed";

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

const TIME_TO_BUFFER_MMS_REQUESTS    = 30000;

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

XPCOMUtils.defineLazyGetter(this, "gRIL", function () {
  return Cc["@mozilla.org/telephony/system-worker-manager;1"].
           getService(Ci.nsIInterfaceRequestor).
           getInterface(Ci.nsIRadioInterfaceLayer);
});

/**
 * MmsService
 */
function MmsService() {
  Services.obs.addObserver(this, kXpcomShutdownObserverTopic, false);
  Services.obs.addObserver(this, kNetworkInterfaceStateChangedTopic, false);
  this.mmsProxySettings.forEach(function(name) {
    Services.prefs.addObserver(name, this, false);
  }, this);

  try {
    this.mmsc = Services.prefs.getCharPref("ril.mms.mmsc");
    this.mmsProxy = Services.prefs.getCharPref("ril.mms.mmsproxy");
    this.mmsPort = Services.prefs.getIntPref("ril.mms.mmsport");
    this.updateMmsProxyInfo();
  } catch (e) {
    debug("Unable to initialize the MMS proxy settings from the preference. " +
          "This could happen at the first-run. Should be available later.");
    this.clearMmsProxySettings();
  }

  try {
    this.urlUAProf = Services.prefs.getCharPref('wap.UAProf.url');
  } catch (e) {
    this.urlUAProf = "";
  }
  try {
    this.tagnameUAProf = Services.prefs.getCharPref('wap.UAProf.tagname');
  } catch (e) {
    this.tagnameUAProf = "x-wap-profile";
  }
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

  /** MMS proxy settings. */
  mmsc: null,
  mmsProxy: null,
  mmsPort: null,
  mmsProxyInfo: null,
  mmsProxySettings: ["ril.mms.mmsc",
                     "ril.mms.mmsproxy",
                     "ril.mms.mmsport"],
  mmsNetworkConnected: false,

  /** MMS network connection reference count. */
  mmsConnRefCount: 0,

  // WebMMS
  urlUAProf: null,
  tagnameUAProf: null,

  // A queue to buffer the MMS HTTP requests when the MMS network
  // is not yet connected. The buffered requests will be cleared
  // if the MMS network fails to be connected within a timer.
  mmsRequestQueue: [],
  timerToClearQueue: Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer),

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
   * Acquire the MMS network connection.
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
   *
   * @return true if the MMS network connection is acquired; false otherwise.
   */
  acquireMmsConnection: function acquireMmsConnection(method, url, istream, callback) {
    // If the MMS network is not yet connected, buffer the
    // MMS request and try to setup the MMS network first.
    if (!this.mmsNetworkConnected) {
      debug("acquireMmsConnection: " +
            "buffer the MMS request and setup the MMS data call.");
      this.mmsRequestQueue.push({method: method,
                                 url: url,
                                 istream: istream,
                                 callback: callback});
      gRIL.setupDataCallByType("mms");

      // Set a timer to clear the buffered MMS requests if the
      // MMS network fails to be connected within a time period.
      this.timerToClearQueue.initWithCallback(function timerToClearQueueCb() {
        debug("timerToClearQueueCb: clear the buffered MMS requests due to " +
              "the timeout: number: " + this.mmsRequestQueue.length);
        while (this.mmsRequestQueue.length) {
          let mmsRequest = this.mmsRequestQueue.shift();
          if (mmsRequest.callback) {
            mmsRequest.callback(0, null);
          }
        }
      }.bind(this), TIME_TO_BUFFER_MMS_REQUESTS, Ci.nsITimer.TYPE_ONE_SHOT);
      return false;
    }

    if (!this.mmsConnRefCount) {
      debug("acquireMmsConnection: register the MMS proxy filter.");
      gpps.registerFilter(this, 0);
    }
    this.mmsConnRefCount++;
    return true;
  },

  /**
   * Release the MMS network connection.
   */
  releaseMmsConnection: function releaseMmsConnection() {
    this.mmsConnRefCount--;
    if (this.mmsConnRefCount <= 0) {
      this.mmsConnRefCount = 0;

      debug("releaseMmsConnection: " +
            "unregister the MMS proxy filter and deactivate the MMS data call.");
      gpps.unregisterFilter(this);
      gRIL.deactivateDataCallByType("mms");
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
    debug("sendMmsRequest: method: " + method + "url: " + url +
          "istream: " + istream + "callback: " + callback);

    if (!this.acquireMmsConnection(method, url, istream, callback)) {
      return;
    }

    let that = this;
    function releaseMmsConnectionAndCallback(status, data) {
      // Always release the MMS network connection before callback.
      that.releaseMmsConnection();
      if (callback) {
        callback(status, data);
      }
    }

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

      if(this.urlUAProf !== "") {
        xhr.setRequestHeader(this.tagnameUAProf, this.urlUAProf);
      }

      // Setup event listeners
      xhr.onerror = function () {
        debug("xhr error, response headers: " + xhr.getAllResponseHeaders());
        releaseMmsConnectionAndCallback(xhr.status, null);
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

        releaseMmsConnectionAndCallback(xhr.status, data);
      }

      // Send request
      xhr.send(istream);
    } catch (e) {
      debug("xhr error, can't send: " + e.message);
      releaseMmsConnectionAndCallback(0, null);
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
    this.sendMmsRequest("POST", this.mmsc, istream);
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
      if (status != HTTP_STATUS_OK) {
        callback(MMS.MMS_PDU_ERROR_TRANSIENT_FAILURE, null);
      } else if (!data) {
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
    } else if (this.isTransientError(status)) {
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
      if (this.isTransientError(status)) {
        return;
      }

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
      if (status != HTTP_STATUS_OK) {
        callback.call(this, MMS.MMS_PDU_ERROR_TRANSIENT_FAILURE, null);
      } else if (!data) {
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
   * Update the MMS proxy info.
   */
  updateMmsProxyInfo: function updateMmsProxyInfo() {
    if (this.mmsProxy === null || this.mmsPort === null) {
      debug("updateMmsProxyInfo: mmsProxy or mmsPort is not yet decided." );
      return;
    }

    this.mmsProxyInfo =
      gpps.newProxyInfo("http",
                        this.mmsProxy,
                        this.mmsPort,
                        Ci.nsIProxyInfo.TRANSPARENT_PROXY_RESOLVES_HOST,
                        -1, null);
    debug("updateMmsProxyInfo: " + JSON.stringify(this.mmsProxyInfo));
  },

  /**
   * Clear the MMS proxy settings.
   */
  clearMmsProxySettings: function clearMmsProxySettings() {
    this.mmsc = null;
    this.mmsProxy = null;
    this.mmsPort = null;
    this.mmsProxyInfo = null;
  },

  /**
   * @param status
   *        The MMS error type.
   *
   * @return true if it's a type of transient error; false otherwise.
   */
  isTransientError: function isTransientError(status) {
    return (status >= MMS.MMS_PDU_ERROR_TRANSIENT_FAILURE &&
            status < MMS.MMS_PDU_ERROR_PERMANENT_FAILURE);
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
        this.mmsNetworkConnected =
          gRIL.getDataCallStateByType("mms") ==
            Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED;

        if (!this.mmsNetworkConnected) {
          return;
        }

        debug("Got the MMS network connected! Resend the buffered " +
              "MMS requests: number: " + this.mmsRequestQueue.length);
        this.timerToClearQueue.cancel();
        while (this.mmsRequestQueue.length) {
          let mmsRequest = this.mmsRequestQueue.shift();
          this.sendMmsRequest(mmsRequest.method,
                              mmsRequest.url,
                              mmsRequest.istream,
                              mmsRequest.callback);
        }
        break;
      }
      case kXpcomShutdownObserverTopic: {
        Services.obs.removeObserver(this, kXpcomShutdownObserverTopic);
        Services.obs.removeObserver(this, kNetworkInterfaceStateChangedTopic);
        this.mmsProxySettings.forEach(function(name) {
          Services.prefs.removeObserver(name, this);
        }, this);
        this.timerToClearQueue.cancel();
        while (this.mmsRequestQueue.length) {
          let mmsRequest = this.mmsRequestQueue.shift();
          if (mmsRequest.callback) {
            mmsRequest.callback(0, null);
          }
        }
        break;
      }
      case kPrefenceChangedObserverTopic: {
        try {
          switch (data) {
            case "ril.mms.mmsc":
              this.mmsc = Services.prefs.getCharPref("ril.mms.mmsc");
              break;
            case "ril.mms.mmsproxy":
              this.mmsProxy = Services.prefs.getCharPref("ril.mms.mmsproxy");
              this.updateMmsProxyInfo();
              break;
            case "ril.mms.mmsport":
              this.mmsPort = Services.prefs.getIntPref("ril.mms.mmsport");
              this.updateMmsProxyInfo();
              break;
            default:
              break;
          }
        } catch (e) {
          debug("Failed to update the MMS proxy settings from the preference.");
          this.clearMmsProxySettings();
        }
        break;
      }
    }
  },

  // nsIProtocolProxyFilter

  applyFilter: function applyFilter(service, uri, proxyInfo) {
    if (!this.mmsNetworkConnected) {
      debug("applyFilter: the MMS network is not connected.");
      return proxyInfo;
     }

    if (this.mmsc === null || uri.prePath != this.mmsc) {
      debug("applyFilter: MMSC is not matched.");
      return proxyInfo;
    }

    if (this.mmsProxyInfo === null) {
      debug("applyFilter: MMS proxy info is not yet decided.");
      return proxyInfo;
    }

    // Fall-through, reutrn the MMS proxy info.
    debug("applyFilter: MMSC is matched: " +
          JSON.stringify({ mmsc: this.mmsc,
                           mmsProxyInfo: this.mmsProxyInfo }));
    return this.mmsProxyInfo;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MmsService]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-@- MmsService: " + s + "\n");
  };
} else {
  debug = function (s) {};
}

