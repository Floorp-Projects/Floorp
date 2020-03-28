/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ServiceRequest",
  "resource://gre/modules/ServiceRequest.jsm"
);

const PREF_BRANCH = "browser.ping-centre.";

const TELEMETRY_PREF = `${PREF_BRANCH}telemetry`;
const LOGGING_PREF = `${PREF_BRANCH}log`;
const STRUCTURED_INGESTION_SEND_TIMEOUT = 30 * 1000; // 30 seconds

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
    let activeExperiments = TelemetryEnvironment.getActiveExperiments();
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
      release_channel: UpdateUtils.getUpdateChannel(false),
      ...data,
    };

    return payload;
  }

  static _gzipCompressString(string) {
    let observer = {
      buffer: "",
      onStreamComplete(loader, context, status, length, result) {
        this.buffer = String.fromCharCode(...result);
      },
    };

    let scs = Cc["@mozilla.org/streamConverters;1"].getService(
      Ci.nsIStreamConverterService
    );
    let listener = Cc["@mozilla.org/network/stream-loader;1"].createInstance(
      Ci.nsIStreamLoader
    );
    listener.init(observer);
    let converter = scs.asyncConvertData(
      "uncompressed",
      "gzip",
      listener,
      null
    );
    let stringStream = Cc[
      "@mozilla.org/io/string-input-stream;1"
    ].createInstance(Ci.nsIStringInputStream);
    stringStream.data = string;
    converter.onStartRequest(null, null);
    converter.onDataAvailable(null, stringStream, 0, string.length);
    converter.onStopRequest(null, null, null);
    return observer.buffer;
  }

  static _sendInGzip(endpoint, payload) {
    return new Promise((resolve, reject) => {
      let request = new ServiceRequest({ mozAnon: true });
      request.mozBackgroundRequest = true;
      request.timeout = STRUCTURED_INGESTION_SEND_TIMEOUT;

      request.open("POST", endpoint, true);
      request.overrideMimeType("text/plain");
      request.setRequestHeader(
        "Content-Type",
        "application/json; charset=UTF-8"
      );
      request.setRequestHeader("Content-Encoding", "gzip");
      request.setRequestHeader("Date", new Date().toUTCString());

      request.onload = event => {
        if (request.status !== 200) {
          reject(event);
        } else {
          resolve(event);
        }
      };
      request.onerror = reject;
      request.onabort = reject;
      request.ontimeout = reject;

      let payloadStream = Cc[
        "@mozilla.org/io/string-input-stream;1"
      ].createInstance(Ci.nsIStringInputStream);
      payloadStream.data = PingCentre._gzipCompressString(payload);
      request.sendInputStream(payloadStream);
    });
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

    return PingCentre._sendInGzip(endpoint, payload).catch(event => {
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

this.PingCentre = PingCentre;
this.PingCentreConstants = {
  FHR_UPLOAD_ENABLED_PREF,
  TELEMETRY_PREF,
  LOGGING_PREF,
};
const EXPORTED_SYMBOLS = ["PingCentre", "PingCentreConstants"];
