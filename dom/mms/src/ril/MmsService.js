/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const RIL_MMSSERVICE_CONTRACTID = "@mozilla.org/mms/rilmmsservice;1";
const RIL_MMSSERVICE_CID = Components.ID("{217ddd76-75db-4210-955d-8806cd8d87f9}");

const DEBUG = false;

const kNetworkInterfaceStateChangedTopic = "network-interface-state-changed";
const kXpcomShutdownObserverTopic        = "xpcom-shutdown";

// HTTP status codes:
// @see http://tools.ietf.org/html/rfc2616#page-39
const HTTP_STATUS_OK = 200;

XPCOMUtils.defineLazyServiceGetter(this, "gpps",
                                   "@mozilla.org/network/protocol-proxy-service;1",
                                   "nsIProtocolProxyService");

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

  proxyInfo: null,
  MMSC: null,

  /** MMS proxy filter reference count. */
  proxyFilterRefCount: 0,

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
   * @param callback
   *        A callback function that takes two arguments: one for http status,
   *        the other for wrapped PDU data for further parsing.
   */
  sendMmsRequest: function sendMmsRequest(method, url, callback) {
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
      xhr.setRequestHeader("Content-Length", 0);

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
      xhr.send();
    } catch (e) {
      debug("xhr error, can't send: " + e.message);
      releaseProxyFilterAndCallback(0, null);
    }
  },

  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   */
  parseStreamAndDispatch: function parseStreamAndDispatch(data) {
    let msg = MMS.PduHelper.parse(data, null);
    if (!msg) {
      return;
    }
    debug("parseStreamAndDispatch: msg = " + JSON.stringify(msg));

    switch (msg.type) {
      case MMS.MMS_PDU_TYPE_NOTIFICATION_IND:
        this.handleNotificationIndication(msg);
        break;
      default:
        debug("Unsupported X-MMS-Message-Type: " + msg.type);
        break;
    }
  },

  /**
   * Handle incoming M-Notification.ind PDU.
   *
   * @param msg
   *        The MMS message object.
   */
  handleNotificationIndication: function handleNotificationIndication(msg) {
    let url = msg.headers["x-mms-content-location"].uri;
    this.sendMmsRequest("GET", url, (function (status, data) {
      if (data) {
        this.parseStreamAndDispatch(data);
      }
    }).bind(this));
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

