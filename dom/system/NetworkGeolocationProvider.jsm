/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  LocationHelper: "resource://gre/modules/LocationHelper.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

// GeolocationPositionError has no interface object, so we can't use that here.
const POSITION_UNAVAILABLE = 2;

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gLoggingEnabled",
  "geo.provider.network.logging.enabled",
  false
);

function LOG(aMsg) {
  if (lazy.gLoggingEnabled) {
    dump("*** WIFI GEO: " + aMsg + "\n");
  }
}

function CachedRequest(loc, cellInfo, wifiList) {
  this.location = loc;

  let wifis = new Set();
  if (wifiList) {
    for (let i = 0; i < wifiList.length; i++) {
      wifis.add(wifiList[i].macAddress);
    }
  }

  // Use only these values for equality
  // (the JSON will contain additional values in future)
  function makeCellKey(cell) {
    return (
      "" +
      cell.radio +
      ":" +
      cell.mobileCountryCode +
      ":" +
      cell.mobileNetworkCode +
      ":" +
      cell.locationAreaCode +
      ":" +
      cell.cellId
    );
  }

  let cells = new Set();
  if (cellInfo) {
    for (let i = 0; i < cellInfo.length; i++) {
      cells.add(makeCellKey(cellInfo[i]));
    }
  }

  this.hasCells = () => cells.size > 0;

  this.hasWifis = () => wifis.size > 0;

  // if fields match
  this.isCellEqual = function(cellInfo) {
    if (!this.hasCells()) {
      return false;
    }

    let len1 = cells.size;
    let len2 = cellInfo.length;

    if (len1 != len2) {
      LOG("cells not equal len");
      return false;
    }

    for (let i = 0; i < len2; i++) {
      if (!cells.has(makeCellKey(cellInfo[i]))) {
        return false;
      }
    }
    return true;
  };

  // if 50% of the SSIDS match
  this.isWifiApproxEqual = function(wifiList) {
    if (!this.hasWifis()) {
      return false;
    }

    // if either list is a 50% subset of the other, they are equal
    let common = 0;
    for (let i = 0; i < wifiList.length; i++) {
      if (wifis.has(wifiList[i].macAddress)) {
        common++;
      }
    }
    let kPercentMatch = 0.5;
    return common >= Math.max(wifis.size, wifiList.length) * kPercentMatch;
  };

  this.isGeoip = function() {
    return !this.hasCells() && !this.hasWifis();
  };

  this.isCellAndWifi = function() {
    return this.hasCells() && this.hasWifis();
  };

  this.isCellOnly = function() {
    return this.hasCells() && !this.hasWifis();
  };

  this.isWifiOnly = function() {
    return this.hasWifis() && !this.hasCells();
  };
}

var gCachedRequest = null;
var gDebugCacheReasoning = ""; // for logging the caching logic

// This function serves two purposes:
// 1) do we have a cached request
// 2) is the cached request better than what newCell and newWifiList will obtain
// If the cached request exists, and we know it to have greater accuracy
// by the nature of its origin (wifi/cell/geoip), use its cached location.
//
// If there is more source info than the cached request had, return false
// In other cases, MLS is known to produce better/worse accuracy based on the
// inputs, so base the decision on that.
function isCachedRequestMoreAccurateThanServerRequest(newCell, newWifiList) {
  gDebugCacheReasoning = "";
  let isNetworkRequestCacheEnabled = true;
  try {
    // Mochitest needs this pref to simulate request failure
    isNetworkRequestCacheEnabled = Services.prefs.getBoolPref(
      "geo.provider.network.debug.requestCache.enabled"
    );
    if (!isNetworkRequestCacheEnabled) {
      gCachedRequest = null;
    }
  } catch (e) {}

  if (!gCachedRequest || !isNetworkRequestCacheEnabled) {
    gDebugCacheReasoning = "No cached data";
    return false;
  }

  if (!newCell && !newWifiList) {
    gDebugCacheReasoning = "New req. is GeoIP.";
    return true;
  }

  if (
    newCell &&
    newWifiList &&
    (gCachedRequest.isCellOnly() || gCachedRequest.isWifiOnly())
  ) {
    gDebugCacheReasoning = "New req. is cell+wifi, cache only cell or wifi.";
    return false;
  }

  if (newCell && gCachedRequest.isWifiOnly()) {
    // In order to know if a cell-only request should trump a wifi-only request
    // need to know if wifi is low accuracy. >5km would be VERY low accuracy,
    // it is worth trying the cell
    var isHighAccuracyWifi = gCachedRequest.location.coords.accuracy < 5000;
    gDebugCacheReasoning =
      "Req. is cell, cache is wifi, isHigh:" + isHighAccuracyWifi;
    return isHighAccuracyWifi;
  }

  let hasEqualCells = false;
  if (newCell) {
    hasEqualCells = gCachedRequest.isCellEqual(newCell);
  }

  let hasEqualWifis = false;
  if (newWifiList) {
    hasEqualWifis = gCachedRequest.isWifiApproxEqual(newWifiList);
  }

  gDebugCacheReasoning =
    "EqualCells:" + hasEqualCells + " EqualWifis:" + hasEqualWifis;

  if (gCachedRequest.isCellOnly()) {
    gDebugCacheReasoning += ", Cell only.";
    if (hasEqualCells) {
      return true;
    }
  } else if (gCachedRequest.isWifiOnly() && hasEqualWifis) {
    gDebugCacheReasoning += ", Wifi only.";
    return true;
  } else if (gCachedRequest.isCellAndWifi()) {
    gDebugCacheReasoning += ", Cache has Cell+Wifi.";
    if (
      (hasEqualCells && hasEqualWifis) ||
      (!newWifiList && hasEqualCells) ||
      (!newCell && hasEqualWifis)
    ) {
      return true;
    }
  }

  return false;
}

function NetworkGeoCoordsObject(lat, lon, acc) {
  this.latitude = lat;
  this.longitude = lon;
  this.accuracy = acc;

  // Neither GLS nor MLS return the following properties, so set them to NaN
  // here. nsGeoPositionCoords will convert NaNs to null for optional properties
  // of the JavaScript Coordinates object.
  this.altitude = NaN;
  this.altitudeAccuracy = NaN;
  this.heading = NaN;
  this.speed = NaN;
}

NetworkGeoCoordsObject.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIDOMGeoPositionCoords"]),
};

function NetworkGeoPositionObject(lat, lng, acc) {
  this.coords = new NetworkGeoCoordsObject(lat, lng, acc);
  this.address = null;
  this.timestamp = Date.now();
}

NetworkGeoPositionObject.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIDOMGeoPosition"]),
};

function NetworkGeolocationProvider() {
  /*
    The _wifiMonitorTimeout controls how long we wait on receiving an update
    from the Wifi subsystem.  If this timer fires, we believe the Wifi scan has
    had a problem and we no longer can use Wifi to position the user this time
    around (we will continue to be hopeful that Wifi will recover).

    This timeout value is also used when Wifi scanning is disabled (see
    isWifiScanningEnabled).  In this case, we use this timer to collect cell/ip
    data and xhr it to the location server.
  */
  XPCOMUtils.defineLazyPreferenceGetter(
    this,
    "_wifiMonitorTimeout",
    "geo.provider.network.timeToWaitBeforeSending",
    5000
  );

  XPCOMUtils.defineLazyPreferenceGetter(
    this,
    "_wifiScanningEnabled",
    "geo.provider.network.scan",
    true
  );

  this.wifiService = null;
  this.timer = null;
  this.started = false;
}

NetworkGeolocationProvider.prototype = {
  classID: Components.ID("{77DA64D3-7458-4920-9491-86CC9914F904}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsIGeolocationProvider",
    "nsIWifiListener",
    "nsITimerCallback",
    "nsIObserver",
  ]),
  listener: null,

  get isWifiScanningEnabled() {
    return Cc["@mozilla.org/wifi/monitor;1"] && this._wifiScanningEnabled;
  },

  resetTimer() {
    if (this.timer) {
      this.timer.cancel();
      this.timer = null;
    }
    // Wifi thread triggers NetworkGeolocationProvider to proceed. With no wifi,
    // do manual timeout.
    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.timer.initWithCallback(
      this,
      this._wifiMonitorTimeout,
      this.timer.TYPE_REPEATING_SLACK
    );
  },

  startup() {
    if (this.started) {
      return;
    }

    this.started = true;

    if (this.isWifiScanningEnabled) {
      if (this.wifiService) {
        this.wifiService.stopWatching(this);
      }
      this.wifiService = Cc["@mozilla.org/wifi/monitor;1"].getService(
        Ci.nsIWifiMonitor
      );
      this.wifiService.startWatching(this);
    }

    this.resetTimer();
    LOG("startup called.");
  },

  watch(c) {
    this.listener = c;
  },

  shutdown() {
    LOG("shutdown called");
    if (!this.started) {
      return;
    }

    // Without clearing this, we could end up using the cache almost indefinitely
    // TODO: add logic for cache lifespan, for now just be safe and clear it
    gCachedRequest = null;

    if (this.timer) {
      this.timer.cancel();
      this.timer = null;
    }

    if (this.wifiService) {
      this.wifiService.stopWatching(this);
      this.wifiService = null;
    }

    this.listener = null;
    this.started = false;
  },

  setHighAccuracy(enable) {},

  onChange(accessPoints) {
    // we got some wifi data, rearm the timer.
    this.resetTimer();

    let wifiData = null;
    if (accessPoints) {
      wifiData = lazy.LocationHelper.formatWifiAccessPoints(accessPoints);
    }
    this.sendLocationRequest(wifiData);
  },

  onError(code) {
    LOG("wifi error: " + code);
    this.sendLocationRequest(null);
  },

  onStatus(err, statusMessage) {
    if (!this.listener) {
      return;
    }
    LOG("onStatus called." + statusMessage);

    if (statusMessage && this.listener.notifyStatus) {
      this.listener.notifyStatus(statusMessage);
    }

    if (err && this.listener.notifyError) {
      this.listener.notifyError(POSITION_UNAVAILABLE, statusMessage);
    }
  },

  notify(timer) {
    this.onStatus(false, "wifi-timeout");
    this.sendLocationRequest(null);
  },

  /**
   * After wifi (and possible cell tower) data has been gathered, this method is
   * invoked to perform the request to network geolocation provider.
   * The result of each request is sent to all registered listener (@see watch)
   * by invoking its respective `update`, `notifyError` or `notifyStatus`
   * callbacks.
   * `update` is called upon a successful request with its response data; this will be a `NetworkGeoPositionObject` instance.
   * `notifyError` is called whenever the request gets an error from the local
   * network subsystem, the server or simply times out.
   * `notifyStatus` is called for each status change of the request that may be
   * of interest to the consumer of this class. Currently the following status
   * changes are reported: 'xhr-start', 'xhr-timeout', 'xhr-error' and
   * 'xhr-empty'.
   *
   * @param  {Array} wifiData Optional set of publicly available wifi networks
   *                          in the following structure:
   *                          <code>
   *                          [
   *                            { macAddress: <mac1>, signalStrength: <signal1> },
   *                            { macAddress: <mac2>, signalStrength: <signal2> }
   *                          ]
   *                          </code>
   */
  async sendLocationRequest(wifiData) {
    let data = { cellTowers: undefined, wifiAccessPoints: undefined };
    if (wifiData && wifiData.length >= 2) {
      data.wifiAccessPoints = wifiData;
    }

    let useCached = isCachedRequestMoreAccurateThanServerRequest(
      data.cellTowers,
      data.wifiAccessPoints
    );

    LOG("Use request cache:" + useCached + " reason:" + gDebugCacheReasoning);

    if (useCached) {
      gCachedRequest.location.timestamp = Date.now();
      if (this.listener) {
        this.listener.update(gCachedRequest.location);
      }
      return;
    }

    // From here on, do a network geolocation request //
    let url = Services.urlFormatter.formatURLPref("geo.provider.network.url");
    LOG("Sending request");

    let result;
    try {
      result = await this.makeRequest(url, wifiData);
      LOG(
        `geo provider reported: ${result.location.lng}:${result.location.lat}`
      );
      let newLocation = new NetworkGeoPositionObject(
        result.location.lat,
        result.location.lng,
        result.accuracy
      );

      if (this.listener) {
        this.listener.update(newLocation);
      }

      gCachedRequest = new CachedRequest(
        newLocation,
        data.cellTowers,
        data.wifiAccessPoints
      );
    } catch (err) {
      LOG("Location request hit error: " + err.name);
      Cu.reportError(err);
      if (err.name == "AbortError") {
        this.onStatus(true, "xhr-timeout");
      } else {
        this.onStatus(true, "xhr-error");
      }
    }
  },

  async makeRequest(url, wifiData) {
    this.onStatus(false, "xhr-start");

    let fetchController = new AbortController();
    let fetchOpts = {
      method: "POST",
      headers: { "Content-Type": "application/json; charset=UTF-8" },
      credentials: "omit",
      signal: fetchController.signal,
    };

    if (wifiData) {
      fetchOpts.body = JSON.stringify({ wifiAccessPoints: wifiData });
    }

    let timeoutId = lazy.setTimeout(
      () => fetchController.abort(),
      Services.prefs.getIntPref("geo.provider.network.timeout")
    );

    let req = await fetch(url, fetchOpts);
    lazy.clearTimeout(timeoutId);
    let result = req.json();
    return result;
  },
};

var EXPORTED_SYMBOLS = ["NetworkGeolocationProvider"];
