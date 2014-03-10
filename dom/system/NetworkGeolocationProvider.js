/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

const POSITION_UNAVAILABLE = Ci.nsIDOMGeoPositionError.POSITION_UNAVAILABLE;

let gLoggingEnabled = false;

// if we don't see any wifi responses in 5 seconds, send the request.
let gTimeToWaitBeforeSending = 5000; //ms

let gWifiScanningEnabled = true;
let gWifiResults;

let gCellScanningEnabled = false;
let gCellResults;

function LOG(aMsg) {
  if (gLoggingEnabled) {
    aMsg = "*** WIFI GEO: " + aMsg + "\n";
    Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(aMsg);
    dump(aMsg);
  }
}

function WifiGeoCoordsObject(lat, lon, acc, alt, altacc) {
  this.latitude = lat;
  this.longitude = lon;
  this.accuracy = acc;
  this.altitude = alt;
  this.altitudeAccuracy = altacc;
}

WifiGeoCoordsObject.prototype = {
  QueryInterface:  XPCOMUtils.generateQI([Ci.nsIDOMGeoPositionCoords]),

  classInfo: XPCOMUtils.generateCI({interfaces: [Ci.nsIDOMGeoPositionCoords],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "wifi geo position coords object"}),
};

function WifiGeoPositionObject(lat, lng, acc) {
  this.coords = new WifiGeoCoordsObject(lat, lng, acc, 0, 0);
  this.address = null;
  this.timestamp = Date.now();
}

WifiGeoPositionObject.prototype = {
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIDOMGeoPosition]),

  // Class Info is required to be able to pass objects back into the DOM.
  classInfo: XPCOMUtils.generateCI({interfaces: [Ci.nsIDOMGeoPosition],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "wifi geo location position object"}),
};

function WifiGeoPositionProvider() {
  try {
    gLoggingEnabled = Services.prefs.getBoolPref("geo.wifi.logging.enabled");
  } catch (e) {}

  try {
    gTimeToWaitBeforeSending = Services.prefs.getIntPref("geo.wifi.timeToWaitBeforeSending");
  } catch (e) {}

  try {
    gWifiScanningEnabled = Services.prefs.getBoolPref("geo.wifi.scan");
  } catch (e) {}

  try {
    gCellScanningEnabled = Services.prefs.getBoolPref("geo.cell.scan");
  } catch (e) {}

  this.wifiService = null;
  this.timeoutTimer = null;
  this.started = false;
}

WifiGeoPositionProvider.prototype = {
  classID:          Components.ID("{77DA64D3-7458-4920-9491-86CC9914F904}"),
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIGeolocationProvider,
                                           Ci.nsIWifiListener,
                                           Ci.nsITimerCallback]),
  startup:  function() {
    if (this.started)
      return;
    this.started = true;

    if (gWifiScanningEnabled) {
      this.wifiService = Cc["@mozilla.org/wifi/monitor;1"].getService(Components.interfaces.nsIWifiMonitor);
      this.wifiService.startWatching(this);
    }
    this.timeoutTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.timeoutTimer.initWithCallback(this,
                                       gTimeToWaitBeforeSending,
                                       this.timeoutTimer.TYPE_REPEATING_SLACK);
    LOG("startup called.");
  },

  watch: function(c) {
  },

  shutdown: function() {
    LOG("shutdown called");
    if (this.started == false) {
      return;
    }

    if (this.timeoutTimer) {
      this.timeoutTimer.cancel();
      this.timeoutTimer = null;
    }

    if(this.wifiService) {
      this.wifiService.stopWatching(this);
      this.wifiService = null;
    }
    this.started = false;
  },

  setHighAccuracy: function(enable) {
  },

  onChange: function(accessPoints) {

    function isPublic(ap) {
      let mask = "_nomap"
      let result = ap.ssid.indexOf(mask, ap.ssid.length - mask.length);
      if (result != -1) {
        LOG("Filtering out " + ap.ssid + " " + result);
      }
      return result;
    };

    function sort(a, b) {
      return b.signal - a.signal;
    };

    function encode(ap) {
      return { 'macAddress': ap.mac, 'signalStrength': ap.signal };
    };

    if (accessPoints) {
      gWifiResults = accessPoints.filter(isPublic).sort(sort).map(encode);
    } else {
      gWifiResults = null;
    }
  },

  onError: function (code) {
    LOG("wifi error: " + code);
  },

  updateMobileInfo: function() {
    LOG("updateMobileInfo called");
    try {
      let radio = Cc["@mozilla.org/ril;1"]
            .getService(Ci.nsIRadioInterfaceLayer)
            .getRadioInterface(0);

      let iccInfo = radio.rilContext.iccInfo;
      let cell = radio.rilContext.voice.cell;

      LOG("mcc: " + iccInfo.mcc);
      LOG("mnc: " + iccInfo.mnc);
      LOG("cid: " + cell.gsmCellId);
      LOG("lac: " + cell.gsmLocationAreaCode);

      gCellResults = [{
        "radio": "gsm",
        "mobileCountryCode": iccInfo.mcc,
        "mobileNetworkCode": iccInfo.mnc,
        "locationAreaCode": cell.gsmLocationAreaCode,
        "cellId": cell.gsmCellId,
      }];
    } catch (e) {
      gCellResults = null;
    }
  },

  notify: function (timeoutTimer) {
    let url = Services.urlFormatter.formatURLPref("geo.wifi.uri");
    LOG("Sending request: " + url + "\n");

    let xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                        .createInstance(Ci.nsIXMLHttpRequest);

    getGeoService().locationUpdatePending();

    try {
      xhr.open("POST", url, true);
    } catch (e) {
      getGeoService().notifyError(POSITION_UNAVAILABLE);
      return;
    }
    xhr.setRequestHeader("Content-Type", "application/json; charset=UTF-8");
    xhr.responseType = "json";
    xhr.mozBackgroundRequest = true;
    xhr.channel.loadFlags = Ci.nsIChannel.LOAD_ANONYMOUS;
    xhr.onerror = function() {
      getGeoService().notifyError(POSITION_UNAVAILABLE);
    };
    xhr.onload = function() {
      LOG("gls returned status: " + xhr.status + " --> " +  JSON.stringify(xhr.response));
      if ((xhr.channel instanceof Ci.nsIHttpChannel && xhr.status != 200) ||
          !xhr.response || !xhr.response.location) {
        getGeoService().notifyError(POSITION_UNAVAILABLE);
        return;
      }

      let newLocation = new WifiGeoPositionObject(xhr.response.location.lat,
                                                  xhr.response.location.lng,
                                                  xhr.response.accuracy);

      getGeoService().update(newLocation);
    };

    if (gCellScanningEnabled) {
      this.updateMobileInfo();
    }

    let data = {};
    if (gWifiResults) {
      data.wifiAccessPoints = gWifiResults;
    }
    if (gCellResults) {
      data.cellTowers = gCellResults;
    }
    data = JSON.stringify(data);
    gWifiResults = gCellResults = null;
    LOG("sending " + data);
    xhr.send(data);
  },
};

function getGeoService() {
  return Cc["@mozilla.org/geolocation/service;1"].getService(Ci.nsIGeolocationUpdate);
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WifiGeoPositionProvider]);
