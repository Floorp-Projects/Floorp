/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "sendStandalonePing",
  "resource://gre/modules/TelemetrySend.jsm"
);

const PREF_BRANCH = "browser.ping-centre.";

const TELEMETRY_PREF = `${PREF_BRANCH}telemetry`;
const LOGGING_PREF = `${PREF_BRANCH}log`;

const FHR_UPLOAD_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";

/**
 * Observe various notifications and send them to a telemetry endpoint.
 *
 * @param {Object} options
 * @param {string} options.topic - a unique ID for users of PingCentre to distinguish
 *                  their data on the server side.
 */
class PingCentre {
  constructor(options) {
    if (!options.topic) {
      throw new Error("Must specify topic.");
    }

    this._topic = options.topic;
    this._prefs = Services.prefs.getBranch("");

    this._enabled = this._prefs.getBoolPref(TELEMETRY_PREF);
    this._onTelemetryPrefChange = this._onTelemetryPrefChange.bind(this);
    this._prefs.addObserver(TELEMETRY_PREF, this._onTelemetryPrefChange);

    this._fhrEnabled = this._prefs.getBoolPref(FHR_UPLOAD_ENABLED_PREF);
    this._onFhrPrefChange = this._onFhrPrefChange.bind(this);
    this._prefs.addObserver(FHR_UPLOAD_ENABLED_PREF, this._onFhrPrefChange);

    this.logging = this._prefs.getBoolPref(LOGGING_PREF);
    this._onLoggingPrefChange = this._onLoggingPrefChange.bind(this);
    this._prefs.addObserver(LOGGING_PREF, this._onLoggingPrefChange);
  }

  get enabled() {
    return this._enabled && this._fhrEnabled;
  }

  _onLoggingPrefChange(aSubject, aTopic, prefKey) {
    this.logging = this._prefs.getBoolPref(prefKey);
  }

  _onTelemetryPrefChange(aSubject, aTopic, prefKey) {
    this._enabled = this._prefs.getBoolPref(prefKey);
  }

  _onFhrPrefChange(aSubject, aTopic, prefKey) {
    this._fhrEnabled = this._prefs.getBoolPref(prefKey);
  }

  _createExperimentsPayload() {
    let activeExperiments = lazy.TelemetryEnvironment.getActiveExperiments();
    let experiments = {};
    for (let experimentID in activeExperiments) {
      if (
        activeExperiments[experimentID] &&
        activeExperiments[experimentID].branch
      ) {
        experiments[experimentID] = {
          branch: activeExperiments[experimentID].branch,
        };
      }
    }
    return experiments;
  }

  _createStructuredIngestionPing(data) {
    let experiments = this._createExperimentsPayload();
    let locale = data.locale || Services.locale.appLocaleAsBCP47;
    const payload = {
      experiments,
      locale,
      version: AppConstants.MOZ_APP_VERSION,
      release_channel: lazy.UpdateUtils.getUpdateChannel(false),
      ...data,
    };

    return payload;
  }

  // We route through this helper because it gets hooked in testing.
  static _sendStandalonePing(endpoint, payload) {
    return lazy.sendStandalonePing(endpoint, payload);
  }

  /**
   * Sends a ping to the Structured Ingestion telemetry pipeline.
   *
   * The payload would be compressed using gzip.
   *
   * @param {Object} data     The payload to be sent.
   * @param {String} endpoint The destination endpoint. Note that Structured Ingestion
   *                          requires a different endpoint for each ping. It's up to the
   *                          caller to provide that. See more details at
   *                          https://github.com/mozilla/gcp-ingestion/blob/master/docs/edge.md#postput-request
   */
  sendStructuredIngestionPing(data, endpoint) {
    if (!this.enabled) {
      return Promise.resolve();
    }

    const ping = this._createStructuredIngestionPing(data);
    const payload = JSON.stringify(ping);

    if (this.logging) {
      Services.console.logStringMessage(
        `TELEMETRY PING (${this._topic}): ${payload}\n`
      );
    }

    return PingCentre._sendStandalonePing(endpoint, payload).catch(event => {
      Cu.reportError(
        `Structured Ingestion ping failure with error: ${event.type}`
      );
    });
  }

  uninit() {
    try {
      this._prefs.removeObserver(TELEMETRY_PREF, this._onTelemetryPrefChange);
      this._prefs.removeObserver(LOGGING_PREF, this._onLoggingPrefChange);
      this._prefs.removeObserver(
        FHR_UPLOAD_ENABLED_PREF,
        this._onFhrPrefChange
      );
    } catch (e) {
      Cu.reportError(e);
    }
  }
}

const PingCentreConstants = {
  FHR_UPLOAD_ENABLED_PREF,
  TELEMETRY_PREF,
  LOGGING_PREF,
};
const EXPORTED_SYMBOLS = ["PingCentre", "PingCentreConstants"];
