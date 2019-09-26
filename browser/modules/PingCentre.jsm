/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

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
  "ClientID",
  "resource://gre/modules/ClientID.jsm"
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
const PRODUCTION_ENDPOINT_PREF = `${PREF_BRANCH}production.endpoint`;
const STRUCTURED_INGESTION_SEND_TIMEOUT = 30 * 1000; // 30 seconds

const FHR_UPLOAD_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";
const BROWSER_SEARCH_REGION_PREF = "browser.search.region";

// Only report region for following regions, to ensure that users in countries
// with small user population (less than 10000) cannot be uniquely identified.
// See bug 1421422 for more details.
const REGION_WHITELIST = new Set([
  "AE",
  "AF",
  "AL",
  "AM",
  "AR",
  "AT",
  "AU",
  "AZ",
  "BA",
  "BD",
  "BE",
  "BF",
  "BG",
  "BJ",
  "BO",
  "BR",
  "BY",
  "CA",
  "CH",
  "CI",
  "CL",
  "CM",
  "CN",
  "CO",
  "CR",
  "CU",
  "CY",
  "CZ",
  "DE",
  "DK",
  "DO",
  "DZ",
  "EC",
  "EE",
  "EG",
  "ES",
  "ET",
  "FI",
  "FR",
  "GB",
  "GE",
  "GH",
  "GP",
  "GR",
  "GT",
  "HK",
  "HN",
  "HR",
  "HU",
  "ID",
  "IE",
  "IL",
  "IN",
  "IQ",
  "IR",
  "IS",
  "IT",
  "JM",
  "JO",
  "JP",
  "KE",
  "KH",
  "KR",
  "KW",
  "KZ",
  "LB",
  "LK",
  "LT",
  "LU",
  "LV",
  "LY",
  "MA",
  "MD",
  "ME",
  "MG",
  "MK",
  "ML",
  "MM",
  "MN",
  "MQ",
  "MT",
  "MU",
  "MX",
  "MY",
  "MZ",
  "NC",
  "NG",
  "NI",
  "NL",
  "NO",
  "NP",
  "NZ",
  "OM",
  "PA",
  "PE",
  "PH",
  "PK",
  "PL",
  "PR",
  "PS",
  "PT",
  "PY",
  "QA",
  "RE",
  "RO",
  "RS",
  "RU",
  "RW",
  "SA",
  "SD",
  "SE",
  "SG",
  "SI",
  "SK",
  "SN",
  "SV",
  "SY",
  "TG",
  "TH",
  "TN",
  "TR",
  "TT",
  "TW",
  "TZ",
  "UA",
  "UG",
  "US",
  "UY",
  "UZ",
  "VE",
  "VN",
  "ZA",
  "ZM",
  "ZW",
]);

/**
 * Observe various notifications and send them to a telemetry endpoint.
 *
 * @param {Object} options
 * @param {string} options.topic - a unique ID for users of PingCentre to distinguish
 *                  their data on the server side.
 * @param {string} options.overrideEndpointPref - optional pref for URL where the POST is sent.
 */
class PingCentre {
  constructor(options) {
    if (!options.topic) {
      throw new Error("Must specify topic.");
    }

    this._topic = options.topic;
    this._prefs = Services.prefs.getBranch("");

    this._setPingEndpoint(options.topic, options.overrideEndpointPref);

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

  /**
   * Lazily get the Telemetry id promise
   */
  get telemetryClientId() {
    Object.defineProperty(this, "telemetryClientId", {
      value: ClientID.getClientID(),
    });
    return this.telemetryClientId;
  }

  get enabled() {
    return this._enabled && this._fhrEnabled;
  }

  _setPingEndpoint(topic, overrideEndpointPref) {
    const overrideValue =
      overrideEndpointPref && this._prefs.getStringPref(overrideEndpointPref);
    if (overrideValue) {
      this._pingEndpoint = overrideValue;
    } else {
      this._pingEndpoint = this._prefs.getStringPref(PRODUCTION_ENDPOINT_PREF);
    }
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

  _createExperimentsString(activeExperiments, filter) {
    let experimentsString = "";
    for (let experimentID in activeExperiments) {
      if (
        !activeExperiments[experimentID] ||
        !activeExperiments[experimentID].branch ||
        (filter && !experimentID.includes(filter))
      ) {
        continue;
      }
      let expString = `${experimentID}:${
        activeExperiments[experimentID].branch
      }`;
      experimentsString = experimentsString.concat(`${expString};`);
    }
    return experimentsString;
  }

  _getRegion() {
    let region = "UNSET";

    if (Services.prefs.prefHasUserValue(BROWSER_SEARCH_REGION_PREF)) {
      region = Services.prefs.getStringPref(BROWSER_SEARCH_REGION_PREF);
      if (region === "") {
        region = "EMPTY";
      } else if (!REGION_WHITELIST.has(region)) {
        region = "OTHER";
      }
    }
    return region;
  }

  async _createPing(data, options) {
    let filter = options && options.filter;
    let experiments = TelemetryEnvironment.getActiveExperiments();
    let experimentsString = this._createExperimentsString(experiments, filter);

    let clientID = data.client_id || (await this.telemetryClientId);
    let locale = data.locale || Services.locale.appLocaleAsLangTag;
    let profileCreationDate =
      TelemetryEnvironment.currentEnvironment.profile.resetDate ||
      TelemetryEnvironment.currentEnvironment.profile.creationDate;
    const payload = Object.assign(
      {
        locale,
        topic: this._topic,
        client_id: clientID,
        version: AppConstants.MOZ_APP_VERSION,
        release_channel: UpdateUtils.getUpdateChannel(false),
      },
      data
    );
    if (experimentsString) {
      payload.shield_id = experimentsString;
    }
    if (profileCreationDate) {
      payload.profile_creation_date = profileCreationDate;
    }
    payload.region = this._getRegion();

    return payload;
  }

  async _createStructuredIngestionPing(data, options) {
    let filter = options && options.filter;
    let experiments = TelemetryEnvironment.getActiveExperiments();
    let experimentsString = this._createExperimentsString(experiments, filter);

    let clientID = data.client_id || (await this.telemetryClientId);
    let locale = data.locale || Services.locale.appLocaleAsLangTag;
    const payload = Object.assign(
      {
        locale,
        client_id: clientID,
        version: AppConstants.MOZ_APP_VERSION,
        release_channel: UpdateUtils.getUpdateChannel(false),
      },
      data
    );
    if (experimentsString) {
      payload.shield_id = experimentsString;
    }

    return payload;
  }

  async sendPing(data, options) {
    if (!this.enabled) {
      return Promise.resolve();
    }

    const payload = await this._createPing(data, options);

    if (this.logging) {
      // performance related pings cause a lot of logging, so we mute them
      if (data.action !== "activity_stream_performance") {
        Services.console.logStringMessage(
          `TELEMETRY PING: ${JSON.stringify(payload)}\n`
        );
      }
    }

    return fetch(this._pingEndpoint, {
      method: "POST",
      body: JSON.stringify(payload),
      credentials: "omit",
    })
      .then(response => {
        if (!response.ok) {
          Cu.reportError(
            `Ping failure with HTTP response code: ${response.status}`
          );
        }
      })
      .catch(e => {
        Cu.reportError(`Ping failure with error: ${e}`);
      });
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
   * @param {Object} options  Other options for this ping.
   */
  async sendStructuredIngestionPing(data, endpoint, options) {
    if (!this.enabled) {
      return Promise.resolve();
    }

    const ping = await this._createStructuredIngestionPing(data, options);
    const payload = JSON.stringify(ping);

    if (this.logging) {
      Services.console.logStringMessage(
        `TELEMETRY PING (STRUCTURED INGESTION): ${payload}\n`
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
  PRODUCTION_ENDPOINT_PREF,
  FHR_UPLOAD_ENABLED_PREF,
  TELEMETRY_PREF,
  LOGGING_PREF,
};
const EXPORTED_SYMBOLS = ["PingCentre", "PingCentreConstants"];
