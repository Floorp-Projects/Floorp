/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.importGlobalProperties(["fetch"]);

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/Console.jsm");

// This is intentionally a different pref-branch than the SDK-based add-on
// used, to avoid extra weirdness for people who happen to have the SDK-based
// installed.  Though maybe we should just forcibly disable the old add-on?
const PREF_BRANCH = "browser.newtabpage.activity-stream.";

const ENDPOINT_PREF = `${PREF_BRANCH}telemetry.ping.endpoint`;
const TELEMETRY_PREF = `${PREF_BRANCH}telemetry`;
const LOGGING_PREF = `${PREF_BRANCH}telemetry.log`;

const FHR_UPLOAD_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";

/**
 * Observe various notifications and send them to a telemetry endpoint.
 *
 * @param {Object} args - optional arguments
 * @param {Function} args.prefInitHook - if present, will be called back
 *                   inside the Prefs constructor. Typically used from tests
 *                   to save off a pointer to a fake Prefs instance so that
 *                   stubs and spies can be inspected by the test code.
 */
function TelemetrySender(args) {
  let prefArgs = {};
  if (args) {
    if ("prefInitHook" in args) {
      prefArgs.initHook = args.prefInitHook;
    }
  }

  this._prefs = new Preferences(prefArgs);

  this._enabled = this._prefs.get(TELEMETRY_PREF);
  this._onTelemetryPrefChange = this._onTelemetryPrefChange.bind(this);
  this._prefs.observe(TELEMETRY_PREF, this._onTelemetryPrefChange);

  this._fhrEnabled = this._prefs.get(FHR_UPLOAD_ENABLED_PREF);
  this._onFhrPrefChange = this._onFhrPrefChange.bind(this);
  this._prefs.observe(FHR_UPLOAD_ENABLED_PREF, this._onFhrPrefChange);

  this.logging = this._prefs.get(LOGGING_PREF);
  this._onLoggingPrefChange = this._onLoggingPrefChange.bind(this);
  this._prefs.observe(LOGGING_PREF, this._onLoggingPrefChange);

  this._pingEndpoint = this._prefs.get(ENDPOINT_PREF);
}

TelemetrySender.prototype = {
  get enabled() {
    return this._enabled && this._fhrEnabled;
  },

  _onLoggingPrefChange(prefVal) {
    this.logging = prefVal;
  },

  _onTelemetryPrefChange(prefVal) {
    this._enabled = prefVal;
  },

  _onFhrPrefChange(prefVal) {
    this._fhrEnabled = prefVal;
  },

  sendPing(data) {
    if (this.logging) {
      // performance related pings cause a lot of logging, so we mute them
      if (data.action !== "activity_stream_performance") {
        console.log(`TELEMETRY PING: ${JSON.stringify(data)}\n`); // eslint-disable-line no-console
      }
    }
    if (!this.enabled) {
      return Promise.resolve();
    }
    return fetch(this._pingEndpoint, {method: "POST", body: JSON.stringify(data)}).then(response => {
      if (!response.ok) {
        Cu.reportError(`Ping failure with HTTP response code: ${response.status}`);
      }
    }).catch(e => {
      Cu.reportError(`Ping failure with error: ${e}`);
    });
  },

  uninit() {
    try {
      this._prefs.ignore(TELEMETRY_PREF, this._onTelemetryPrefChange);
      this._prefs.ignore(LOGGING_PREF, this._onLoggingPrefChange);
      this._prefs.ignore(FHR_UPLOAD_ENABLED_PREF, this._onFhrPrefChange);
    } catch (e) {
      Cu.reportError(e);
    }
  }
};

this.TelemetrySender = TelemetrySender;
this.TelemetrySenderConstants = {
  ENDPOINT_PREF,
  FHR_UPLOAD_ENABLED_PREF,
  TELEMETRY_PREF,
  LOGGING_PREF
};
this.EXPORTED_SYMBOLS = ["TelemetrySender", "TelemetrySenderConstants"];
