/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Experiments",
  "ExperimentsProvider",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryPing",
                                  "resource://gre/modules/TelemetryPing.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryLog",
                                  "resource://gre/modules/TelemetryLog.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");
XPCOMUtils.defineLazyModuleGetter(this, "Metrics",
                                  "resource://gre/modules/Metrics.jsm");

// CertUtils.jsm doesn't expose a single "CertUtils" object like a normal .jsm
// would.
XPCOMUtils.defineLazyGetter(this, "CertUtils",
  function() {
    var mod = {};
    Cu.import("resource://gre/modules/CertUtils.jsm", mod);
    return mod;
  });

#ifdef MOZ_CRASHREPORTER
XPCOMUtils.defineLazyServiceGetter(this, "gCrashReporter",
                                   "@mozilla.org/xre/app-info;1",
                                   "nsICrashReporter");
#endif

const FILE_CACHE                = "experiments.json";
const OBSERVER_TOPIC            = "experiments-changed";
const MANIFEST_VERSION          = 1;
const CACHE_VERSION             = 1;

const KEEP_HISTORY_N_DAYS       = 180;
const MIN_EXPERIMENT_ACTIVE_SECONDS = 60;

const PREF_BRANCH               = "experiments.";
const PREF_ENABLED              = "enabled"; // experiments.enabled
const PREF_LOGGING              = "logging";
const PREF_LOGGING_LEVEL        = PREF_LOGGING + ".level"; // experiments.logging.level
const PREF_LOGGING_DUMP         = PREF_LOGGING + ".dump"; // experiments.logging.dump
const PREF_MANIFEST_URI         = "manifest.uri"; // experiments.logging.manifest.uri
const PREF_MANIFEST_CHECKCERT   = "manifest.cert.checkAttributes"; // experiments.manifest.cert.checkAttributes
const PREF_MANIFEST_REQUIREBUILTIN = "manifest.cert.requireBuiltin"; // experiments.manifest.cert.requireBuiltin
const PREF_FORCE_SAMPLE = "force-sample-value"; // experiments.force-sample-value

const PREF_HEALTHREPORT_ENABLED = "datareporting.healthreport.service.enabled";

const PREF_BRANCH_TELEMETRY     = "toolkit.telemetry.";
const PREF_TELEMETRY_ENABLED    = "enabled";

const TELEMETRY_LOG = {
  // log(key, [kind, experimentId, details])
  ACTIVATION_KEY: "EXPERIMENT_ACTIVATION",
  ACTIVATION: {
    ACTIVATED: "ACTIVATED",             // successfully activated
    INSTALL_FAILURE: "INSTALL_FAILURE", // failed to install the extension
    REJECTED: "REJECTED",               // experiment was rejected because of it's conditions,
                                        // provides details on which
  },

  // log(key, [kind, experimentId, optionalDetails...])
  TERMINATION_KEY: "EXPERIMENT_TERMINATION",
  TERMINATION: {
    USERDISABLED: "USERDISABLED", // the user disabled this experiment
    FROM_API: "FROM_API",         // the experiment disabled itself
    EXPIRED: "EXPIRED",           // experiment expired e.g. by exceeding the end-date
    RECHECK: "RECHECK",           // disabled after re-evaluating conditions,
                                  // provides details on which
  },
};

const gPrefs = new Preferences(PREF_BRANCH);
const gPrefsTelemetry = new Preferences(PREF_BRANCH_TELEMETRY);
let gExperimentsEnabled = false;
let gExperiments = null;
let gLogAppenderDump = null;
let gPolicyCounter = 0;
let gExperimentsCounter = 0;
let gExperimentEntryCounter = 0;

// Tracks active AddonInstall we know about so we can deny external
// installs.
let gActiveInstallURLs = new Set();

let gLogger;
let gLogDumping = false;

function configureLogging() {
  if (!gLogger) {
    gLogger = Log.repository.getLogger("Browser.Experiments");
    gLogger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
  }
  gLogger.level = gPrefs.get(PREF_LOGGING_LEVEL, Log.Level.Warn);

  let logDumping = gPrefs.get(PREF_LOGGING_DUMP, false);
  if (logDumping != gLogDumping) {
    if (logDumping) {
      gLogAppenderDump = new Log.DumpAppender(new Log.BasicFormatter());
      gLogger.addAppender(gLogAppenderDump);
    } else {
      gLogger.removeAppender(gLogAppenderDump);
      gLogAppenderDump = null;
    }
    gLogDumping = logDumping;
  }
}

// Takes an array of promises and returns a promise that is resolved once all of
// them are rejected or resolved.
function allResolvedOrRejected(promises) {
  if (!promises.length) {
    return Promise.resolve([]);
  }

  let countdown = promises.length;
  let deferred = Promise.defer();

  for (let p of promises) {
    let helper = () => {
      if (--countdown == 0) {
        deferred.resolve();
      }
    };
    Promise.resolve(p).then(helper, helper);
  }

  return deferred.promise;
}

// Loads a JSON file using OS.file. file is a string representing the path
// of the file to be read, options contains additional options to pass to
// OS.File.read.
// Returns a Promise resolved with the json payload or rejected with
// OS.File.Error or JSON.parse() errors.
function loadJSONAsync(file, options) {
  return Task.spawn(function() {
    let rawData = yield OS.File.read(file, options);
    // Read json file into a string
    let data;
    try {
      // Obtain a converter to read from a UTF-8 encoded input stream.
      let converter = new TextDecoder();
      data = JSON.parse(converter.decode(rawData));
    } catch (ex) {
      gLogger.error("Experiments: Could not parse JSON: " + file + " " + ex);
      throw ex;
    }
    throw new Task.Result(data);
  });
}

function telemetryEnabled() {
  return gPrefsTelemetry.get(PREF_TELEMETRY_ENABLED, false);
}

// Returns a promise that is resolved with the AddonInstall for that URL.
function addonInstallForURL(url, hash) {
  let deferred = Promise.defer();
  AddonManager.getInstallForURL(url, install => deferred.resolve(install),
                                "application/x-xpinstall", hash);
  return deferred.promise;
}

// Returns a promise that is resolved with an Array<Addon> of the installed
// experiment addons.
function installedExperimentAddons() {
  let deferred = Promise.defer();
  AddonManager.getAddonsByTypes(["experiment"],
                                addons => deferred.resolve(addons));
  return deferred.promise;
}

// Takes an Array<Addon> and returns a promise that is resolved when the
// addons are uninstalled.
function uninstallAddons(addons) {
  let ids = new Set([a.id for (a of addons)]);
  let deferred = Promise.defer();

  let listener = {};
  listener.onUninstalled = addon => {
    if (!ids.has(addon.id)) {
      return;
    }

    ids.delete(addon.id);
    if (ids.size == 0) {
      AddonManager.removeAddonListener(listener);
      deferred.resolve();
    }
  };

  AddonManager.addAddonListener(listener);

  for (let addon of addons) {
    // Disabling the add-on before uninstalling is necessary to cause tests to
    // pass. This might be indicative of a bug in XPIProvider.
    // TODO follow up in bug 992396.
    addon.userDisabled = true;
    addon.uninstall();
  }

  return deferred.promise;
}

/**
 * The experiments module.
 */

let Experiments = {
  /**
   * Provides access to the global `Experiments.Experiments` instance.
   */
  instance: function () {
    if (!gExperiments) {
      gExperiments = new Experiments.Experiments();
    }

    return gExperiments;
  },
};

/*
 * The policy object allows us to inject fake enviroment data from the
 * outside by monkey-patching.
 */

Experiments.Policy = function () {
  this._log = Log.repository.getLoggerWithMessagePrefix(
    "Browser.Experiments.Policy",
    "Policy #" + gPolicyCounter++ + "::");
};

Experiments.Policy.prototype = {
  now: function () {
    return new Date();
  },

  random: function () {
    let pref = gPrefs.get(PREF_FORCE_SAMPLE);
    if (pref !== undefined) {
      let val = Number.parseFloat(pref);
      this._log.debug("random sample forced: " + val);
      if (isNaN(val) || val < 0) {
        return 0;
      }
      if (val > 1) {
        return 1;
      }
      return val;
    }
    return Math.random();
  },

  futureDate: function (offset) {
    return new Date(this.now().getTime() + offset);
  },

  oneshotTimer: function (callback, timeout, thisObj, name) {
    return CommonUtils.namedTimer(callback, timeout, thisObj, name);
  },

  updatechannel: function () {
    return UpdateChannel.get();
  },

  locale: function () {
    let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
    return chrome.getSelectedLocale("global");
  },

  /*
   * @return Promise<> Resolved with the payload data.
   */
  healthReportPayload: function () {
    return Task.spawn(function*() {
      let reporter = Cc["@mozilla.org/datareporting/service;1"]
            .getService(Ci.nsISupports)
            .wrappedJSObject
            .healthReporter;
      yield reporter.onInit();
      let payload = yield reporter.collectAndObtainJSONPayload();
      throw new Task.Result(payload);
    });
  },

  telemetryPayload: function () {
    return TelemetryPing.getPayload();
  },
};

/**
 * Manages the experiments and provides an interface to control them.
 */

Experiments.Experiments = function (policy=new Experiments.Policy()) {
  this._log = Log.repository.getLoggerWithMessagePrefix(
    "Browser.Experiments.Experiments",
    "Experiments #" + gExperimentsCounter++ + "::");

  this._policy = policy;

  // This is a Map of (string -> ExperimentEntry), keyed with the experiment id.
  // It holds both the current experiments and history.
  // Map() preserves insertion order, which means we preserve the manifest order.
  // This is null until we've successfully completed loading the cache from
  // disk the first time.
  this._experiments = null;
  this._refresh = false;
  this._terminateReason = null; // or TELEMETRY_LOG.TERMINATION....
  this._dirty = false;

  // Loading the cache happens once asynchronously on startup
  this._loadTask = null;

  // Ignore addon-manager notifications for addons that we are uninstalling ourself
  this._pendingUninstall = null;

  // The _main task handles all other actions:
  // * refreshing the manifest off the network (if _refresh)
  // * disabling/enabling experiments
  // * saving the cache (if _dirty)
  this._mainTask = null;

  // Timer for re-evaluating experiment status.
  this._timer = null;

  this._shutdown = false;

  this.init();
};

Experiments.Experiments.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback, Ci.nsIObserver]),

  init: function () {
    configureLogging();

    gExperimentsEnabled = gPrefs.get(PREF_ENABLED, false);
    this._log.trace("enabled=" + gExperimentsEnabled + ", " + this.enabled);

    gPrefs.observe(PREF_LOGGING, configureLogging);
    gPrefs.observe(PREF_MANIFEST_URI, this.updateManifest, this);
    gPrefs.observe(PREF_ENABLED, this._toggleExperimentsEnabled, this);

    gPrefsTelemetry.observe(PREF_TELEMETRY_ENABLED, this._telemetryStatusChanged, this);

    AsyncShutdown.profileBeforeChange.addBlocker("Experiments.jsm shutdown",
      this.uninit.bind(this));

    this._startWatchingAddons();

    this._loadTask = Task.spawn(this._loadFromCache.bind(this));
    this._loadTask.then(
      () => {
        this._log.trace("_loadTask finished ok");
        this._loadTask = null;
        this._run();
      },
      (e) => {
        this._log.error("_loadFromCache caught error: " + e);
      }
    );
  },

  /**
   * @return Promise<>
   *         The promise is fulfilled when all pending tasks are finished.
   */
  uninit: function () {
    if (!this._shutdown) {
      this._stopWatchingAddons();

      gPrefs.ignore(PREF_LOGGING, configureLogging);
      gPrefs.ignore(PREF_MANIFEST_URI, this.updateManifest, this);
      gPrefs.ignore(PREF_ENABLED, this._toggleExperimentsEnabled, this);

      gPrefsTelemetry.ignore(PREF_TELEMETRY_ENABLED, this._telemetryStatusChanged, this);

      if (this._timer) {
        this._timer.clear();
      }
    }

    this._shutdown = true;
    if (this._mainTask) {
      return this._mainTask;
    }
    return Promise.resolve();
  },

  _startWatchingAddons: function () {
    AddonManager.addAddonListener(this);
    AddonManager.addInstallListener(this);
  },

  _stopWatchingAddons: function () {
    AddonManager.removeInstallListener(this);
    AddonManager.removeAddonListener(this);
  },

  /**
   * Throws an exception if we've already shut down.
   */
  _checkForShutdown: function() {
    if (this._shutdown) {
      throw Error("uninit() already called");
    }
  },

  /**
   * Whether the experiments feature is enabled.
   */
  get enabled() {
    return gExperimentsEnabled;
  },

  /**
   * Toggle whether the experiments feature is enabled or not.
   */
  set enabled(enabled) {
    this._log.trace("set enabled(" + enabled + ")");
    gPrefs.set(PREF_ENABLED, enabled);
  },

  _toggleExperimentsEnabled: function (enabled) {
    this._log.trace("_toggleExperimentsEnabled(" + enabled + ")");
    let wasEnabled = gExperimentsEnabled;
    gExperimentsEnabled = enabled && telemetryEnabled();

    if (wasEnabled == gExperimentsEnabled) {
      return;
    }

    if (gExperimentsEnabled) {
      this.updateManifest();
    } else {
      this.disableExperiment();
      if (this._timer) {
        this._timer.clear();
      }
    }
  },

  _telemetryStatusChanged: function () {
    this._toggleExperimentsEnabled(gExperimentsEnabled);
  },

  /**
   * Returns a promise that is resolved with an array of `ExperimentInfo` objects,
   * which provide info on the currently and recently active experiments.
   * The array is in chronological order.
   *
   * The experiment info is of the form:
   * {
   *   id: <string>,
   *   name: <string>,
   *   description: <string>,
   *   active: <boolean>,
   *   endDate: <integer>, // epoch ms
   *   detailURL: <string>,
   *   ... // possibly extended later
   * }
   *
   * @return Promise<Array<ExperimentInfo>> Array of experiment info objects.
   */
  getExperiments: function () {
    return Task.spawn(function*() {
      yield this._loadTask;
      let list = [];

      for (let [id, experiment] of this._experiments) {
        if (!experiment.startDate) {
          // We only collect experiments that are or were active.
          continue;
        }

        list.push({
          id: id,
          name: experiment._name,
          description: experiment._description,
          active: experiment.enabled,
          endDate: experiment.endDate.getTime(),
          detailURL: experiment._homepageURL,
        });
      }

      // Sort chronologically, descending.
      list.sort((a, b) => b.endDate - a.endDate);
      return list;
    }.bind(this));
  },

  /**
   * Determine whether another date has the same UTC day as now().
   */
  _dateIsTodayUTC: function (d) {
    let now = this._policy.now();

    return stripDateToMidnight(now).getTime() == stripDateToMidnight(d).getTime();
  },

  /**
   * Obtain the entry of the most recent active experiment that was active
   * today.
   *
   * If no experiment was active today, this resolves to nothing.
   *
   * Assumption: Only a single experiment can be active at a time.
   *
   * @return Promise<object>
   */
  lastActiveToday: function () {
    return Task.spawn(function* getMostRecentActiveExperimentTask() {
      let experiments = yield this.getExperiments();

      // Assumption: Ordered chronologically, descending, with active always
      // first.
      for (let experiment of experiments) {
        if (experiment.active) {
          return experiment;
        }

        if (experiment.endDate && this._dateIsTodayUTC(experiment.endDate)) {
          return experiment;
        }
      }
      return null;
    }.bind(this));
  },

  _run: function() {
    this._log.trace("_run");
    this._checkForShutdown();
    if (!this._mainTask) {
      this._mainTask = Task.spawn(this._main.bind(this));
      this._mainTask.then(
        () => {
          this._log.trace("_main finished, scheduling next run");
          this._mainTask = null;
          this._scheduleNextRun();
        },
        (e) => {
          this._log.error("_main caught error: " + e);
          this._mainTask = null;
        }
      );
    }
    return this._mainTask;
  },

  _main: function*() {
    do {
      this._log.trace("_main iteration");
      yield this._loadTask;
      if (this._refresh) {
        yield this._loadManifest();
      }
      yield this._evaluateExperiments();
      if (this._dirty) {
        yield this._saveToCache();
      }
      // If somebody called .updateManifest() or disableExperiment()
      // while we were running, go again right now.
    }
    while (this._refresh || this._terminateReason);
  },

  _loadManifest: function*() {
    this._log.trace("_loadManifest");
    let uri = Services.urlFormatter.formatURLPref(PREF_BRANCH + PREF_MANIFEST_URI);

    this._checkForShutdown();

    this._refresh = false;
    try {
      let responseText = yield this._httpGetRequest(uri);
      this._log.trace("_loadManifest() - responseText=\"" + responseText + "\"");

      if (this._shutdown) {
        return;
      }

      let data = JSON.parse(responseText);
      this._updateExperiments(data);
    } catch (e) {
      this._log.error("_loadManifest - failure to fetch/parse manifest (continuing anyway): " + e);
    }
  },

  /**
   * Fetch an updated list of experiments and trigger experiment updates.
   * Do only use when experiments are enabled.
   *
   * @return Promise<>
   *         The promise is resolved when the manifest and experiment list is updated.
   */
  updateManifest: function () {
    this._log.trace("updateManifest()");

    if (!gExperimentsEnabled) {
      return Promise.reject(new Error("experiments are disabled"));
    }

    if (this._shutdown) {
      return Promise.reject(Error("uninit() alrady called"));
    }

    this._refresh = true;
    return this._run();
  },

  notify: function (timer) {
    this._log.trace("notify()");
    this._checkForShutdown();
    return this._run();
  },

  // START OF ADD-ON LISTENERS

  onDisabled: function (addon) {
    this._log.trace("onDisabled() - addon id: " + addon.id);
    if (addon.id == this._pendingUninstall) {
      return;
    }
    let activeExperiment = this._getActiveExperiment();
    if (!activeExperiment || activeExperiment._addonId != addon.id) {
      return;
    }
    this.disableExperiment();
  },

  onUninstalled: function (addon) {
    this._log.trace("onUninstalled() - addon id: " + addon.id);
    if (addon.id == this._pendingUninstall) {
      this._log.trace("matches pending uninstall");
      return;
    }
    let activeExperiment = this._getActiveExperiment();
    if (!activeExperiment || activeExperiment._addonId != addon.id) {
      return;
    }
    this.disableExperiment();
  },

  onInstallStarted: function (install) {
    if (install.addon.type != "experiment") {
      return;
    }

    // We want to be in control of all experiment add-ons: reject installs
    // for add-ons that we don't know about.

    // We have a race condition of sorts to worry about here. We have 2
    // onInstallStarted listeners. This one (the global one) and the one
    // created as part of ExperimentEntry._installAddon. Because of the order
    // they are registered in, this one likely executes first. Unfortunately,
    // this means that the add-on ID is not yet set on the ExperimentEntry.
    // So, we can't just look at this._trackedAddonIds because the new experiment
    // will have its add-on ID set to null. We work around this by storing a
    // identifying field - the source URL of the install - in a module-level
    // variable (so multiple Experiments instances doesn't cancel each other
    // out).

    if (this._trackedAddonIds.has(install.addon.id)) {
      this._log.info("onInstallStarted allowing install because add-on ID " +
                     "tracked by us.");
      return;
    }

    if (gActiveInstallURLs.has(install.sourceURI.spec)) {
      this._log.info("onInstallStarted allowing install because install " +
                     "tracked by us.");
      return;
    }

    this._log.warn("onInstallStarted cancelling install of unknown " +
                   "experiment add-on: " + install.addon.id);
    return false;
  },

  // END OF ADD-ON LISTENERS.

  _getExperimentByAddonId: function (addonId) {
    for (let [, entry] of this._experiments) {
      if (entry._addonId === addonId) {
        return entry;
      }
    }

    return null;
  },

  /*
   * Helper function to make HTTP GET requests. Returns a promise that is resolved with
   * the responseText when the request is complete.
   */
  _httpGetRequest: function (url) {
    this._log.trace("httpGetRequest(" + url + ")");
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    try {
      xhr.open("GET", url);
    } catch (e) {
      this._log.error("httpGetRequest() - Error opening request to " + url + ": " + e);
      return Promise.reject(new Error("Experiments - Error opening XHR for " + url));
    }

    let deferred = Promise.defer();

    let log = this._log;
    xhr.onerror = function (e) {
      log.error("httpGetRequest::onError() - Error making request to " + url + ": " + e.error);
      deferred.reject(new Error("Experiments - XHR error for " + url + " - " + e.error));
    };

    xhr.onload = function (event) {
      if (xhr.status !== 200 && xhr.state !== 0) {
        log.error("httpGetRequest::onLoad() - Request to " + url + " returned status " + xhr.status);
        deferred.reject(new Error("Experiments - XHR status for " + url + " is " + xhr.status));
        return;
      }

      let certs = null;
      if (gPrefs.get(PREF_MANIFEST_CHECKCERT, true)) {
        certs = CertUtils.readCertPrefs(PREF_BRANCH + "manifest.certs.");
      }
      try {
        let allowNonBuiltin = !gPrefs.get(PREF_MANIFEST_REQUIREBUILTIN, true);
        CertUtils.checkCert(xhr.channel, allowNonBuiltin, certs);
      }
      catch (e) {
        log.error("manifest fetch failed certificate checks", [e]);
        deferred.reject(new Error("Experiments - manifest fetch failed certificate checks: " + e));
        return;
      }

      deferred.resolve(xhr.responseText);
    };

    if (xhr.channel instanceof Ci.nsISupportsPriority) {
      xhr.channel.priority = Ci.nsISupportsPriority.PRIORITY_LOWEST;
    }

    xhr.send(null);
    return deferred.promise;
  },

  /*
   * Path of the cache file we use in the profile.
   */
  get _cacheFilePath() {
    return OS.Path.join(OS.Constants.Path.profileDir, FILE_CACHE);
  },

  /*
   * Part of the main task to save the cache to disk, called from _main.
   */
  _saveToCache: function* () {
    this._log.trace("_saveToCache");
    let path = this._cacheFilePath;
    let textData = JSON.stringify({
      version: CACHE_VERSION,
      data: [e[1].toJSON() for (e of this._experiments.entries())],
    });

    let encoder = new TextEncoder();
    let data = encoder.encode(textData);
    let options = { tmpPath: path + ".tmp", compression: "lz4" };
    yield OS.File.writeAtomic(path, data, options);
    this._dirty = false;
    this._log.debug("_saveToCache saved to " + path);
  },

  /*
   * Task function, load the cached experiments manifest file from disk.
   */
  _loadFromCache: function*() {
    this._log.trace("_loadFromCache");
    let path = this._cacheFilePath;
    try {
      let result = yield loadJSONAsync(path, { compression: "lz4" });
      this._populateFromCache(result);
    } catch (e if e instanceof OS.File.Error && e.becauseNoSuchFile) {
      // No cached manifest yet.
      this._experiments = new Map();
    }
  },

  _populateFromCache: function (data) {
    this._log.trace("populateFromCache() - data: " + JSON.stringify(data));

    // If the user has a newer cache version than we can understand, we fail
    // hard; no experiments should be active in this older client.
    if (CACHE_VERSION !== data.version) {
      throw new Error("Experiments::_populateFromCache() - invalid cache version");
    }

    let experiments = new Map();
    for (let item of data.data) {
      let entry = new Experiments.ExperimentEntry(this._policy);
      if (!entry.initFromCacheData(item)) {
        continue;
      }
      experiments.set(entry.id, entry);
    }

    this._experiments = experiments;
  },

  /*
   * Update the experiment entries from the experiments
   * array in the manifest
   */
  _updateExperiments: function (manifestObject) {
    this._log.trace("_updateExperiments() - experiments: " + JSON.stringify(manifestObject));

    if (manifestObject.version !== MANIFEST_VERSION) {
      this._log.warning("updateExperiments() - unsupported version " + manifestObject.version);
    }

    let experiments = new Map(); // The new experiments map

    // Collect new and updated experiments.
    for (let data of manifestObject.experiments) {
      let entry = this._experiments.get(data.id);

      if (entry) {
        if (!entry.updateFromManifestData(data)) {
          this._log.error("updateExperiments() - Invalid manifest data for " + data.id);
          continue;
        }
      } else {
        entry = new Experiments.ExperimentEntry(this._policy);
        if (!entry.initFromManifestData(data)) {
          continue;
        }
      }

      if (entry.shouldDiscard()) {
        continue;
      }

      experiments.set(entry.id, entry);
    }

    // Make sure we keep experiments that are or were running.
    // We remove them after KEEP_HISTORY_N_DAYS.
    for (let [id, entry] of this._experiments) {
      if (experiments.has(id) || !entry.startDate || entry.shouldDiscard()) {
        this._log.trace("updateExperiments() - discarding entry for " + id);
        continue;
      }

      experiments.set(id, entry);
    }

    this._experiments = experiments;
    this._dirty = true;
  },

  _getActiveExperiment: function () {
    let enabled = [experiment for ([,experiment] of this._experiments) if (experiment._enabled)];

    if (enabled.length == 1) {
      return enabled[0];
    }

    if (enabled.length > 1) {
      this._log.error("getActiveExperimentId() - should not have more than 1 active experiment");
      throw new Error("have more than 1 active experiment");
    }

    return null;
  },

  /**
   * Disable an experiment by id.
   * @param experimentId The id of the experiment.
   * @param userDisabled (optional) Whether this is disabled as a result of a user action.
   * @return Promise<> Promise that will get resolved once the task is done or failed.
   */
  disableExperiment: function (userDisabled=true) {
    this._log.trace("disableExperiment()");

    this._terminateReason = userDisabled ? TELEMETRY_LOG.TERMINATION.USERDISABLED : TELEMETRY_LOG.TERMINATION.FROM_API;
    return this._run();
  },

  /**
   * The Set of add-on IDs that we know about from manifests.
   */
  get _trackedAddonIds() {
    if (!this._experiments) {
      return new Set();
    }

    return new Set([e._addonId for ([,e] of this._experiments) if (e._addonId)]);
  },

  /*
   * Task function to check applicability of experiments, disable the active
   * experiment if needed and activate the first applicable candidate.
   */
  _evaluateExperiments: function*() {
    this._log.trace("_evaluateExperiments");

    this._checkForShutdown();

    // The first thing we do is reconcile our state against what's in the
    // Addon Manager. It's possible that the Addon Manager knows of experiment
    // add-ons that we don't. This could happen if an experiment gets installed
    // when we're not listening or if there is a bug in our synchronization
    // code.
    //
    // We have a few options of what to do with unknown experiment add-ons
    // coming from the Addon Manager. Ideally, we'd convert these to
    // ExperimentEntry instances and stuff them inside this._experiments.
    // However, since ExperimentEntry contain lots of metadata from the
    // manifest and trying to make up data could be error prone, it's safer
    // to not try. Furthermore, if an experiment really did come from us, we
    // should have some record of it. In the end, we decide to discard all
    // knowledge for these unknown experiment add-ons.
    let installedExperiments = yield installedExperimentAddons();
    let expectedAddonIds = this._trackedAddonIds;
    let unknownAddons = [a for (a of installedExperiments) if (!expectedAddonIds.has(a.id))];
    if (unknownAddons.length) {
      this._log.warn("_evaluateExperiments() - unknown add-ons in AddonManager: " +
                     [a.id for (a of unknownAddons)].join(", "));

      yield uninstallAddons(unknownAddons);
    }

    let activeExperiment = this._getActiveExperiment();
    let activeChanged = false;
    let now = this._policy.now();

    if (activeExperiment) {
      this._pendingUninstall = activeExperiment._addonId;
      try {
        let wasStopped;
        if (this._terminateReason) {
          yield activeExperiment.stop(this._terminateReason);
          wasStopped = true;
        } else {
          wasStopped = yield activeExperiment.maybeStop();
        }
        if (wasStopped) {
          this._dirty = true;
          this._log.debug("evaluateExperiments() - stopped experiment "
                        + activeExperiment.id);
          activeExperiment = null;
          activeChanged = true;
        } else if (activeExperiment.needsUpdate) {
          this._log.debug("evaluateExperiments() - updating experiment "
                        + activeExperiment.id);
          try {
            yield activeExperiment.stop();
            yield activeExperiment.start();
          } catch (e) {
            this._log.error(e);
            // On failure try the next experiment.
            activeExperiment = null;
          }
          this._dirty = true;
          activeChanged = true;
        } else {
          yield activeExperiment.ensureActive();
        }
      } finally {
        this._pendingUninstall = null;
      }
    }
    this._terminateReason = null;

    if (!activeExperiment) {
      for (let [id, experiment] of this._experiments) {
        let applicable;
        let reason = null;
        try {
          applicable = yield experiment.isApplicable();
        }
        catch (e) {
          applicable = false;
          reason = e;
        }

        if (!applicable && reason && reason[0] != "was-active") {
          // Report this from here to avoid over-reporting.
          let desc = TELEMETRY_LOG.ACTIVATION;
          let data = [TELEMETRY_LOG.ACTIVATION.REJECTED, id];
          data = data.concat(reason);
          TelemetryLog.log(TELEMETRY_LOG.ACTIVATION_KEY, data);
        }

        if (applicable) {
          this._log.debug("evaluateExperiments() - activating experiment " + id);
          try {
            yield experiment.start();
            activeChanged = true;
            activeExperiment = experiment;
            this._dirty = true;
            break;
          } catch (e) {
            // On failure try the next experiment.
          }
        }
      }
    }

    if (activeChanged) {
      Services.obs.notifyObservers(null, OBSERVER_TOPIC, null);
    }

#ifdef MOZ_CRASHREPORTER
    if (activeExperiment) {
      gCrashReporter.annotateCrashReport("ActiveExperiment", activeExperiment.id);
    }
#endif
  },

  /*
   * Schedule the soonest re-check of experiment applicability that is needed.
   */
  _scheduleNextRun: function () {
    this._checkForShutdown();

    if (this._timer) {
      this._timer.clear();
    }

    if (!gExperimentsEnabled || this._experiments.length == 0) {
      return;
    }

    let time = null;
    let now = this._policy.now().getTime();

    for (let [id, experiment] of this._experiments) {
      let scheduleTime = experiment.getScheduleTime();
      if (scheduleTime > now) {
        if (time !== null) {
          time = Math.min(time, scheduleTime);
        } else {
          time = scheduleTime;
        }
      }
    }

    if (time === null) {
      // No schedule time found.
      return;
    }

    this._log.trace("scheduleExperimentEvaluation() - scheduling for "+time+", now: "+now);
    this._policy.oneshotTimer(this.notify, time - now, this, "_timer");
  },
};


/*
 * Represents a single experiment.
 */

Experiments.ExperimentEntry = function (policy) {
  this._policy = policy || new Experiments.Policy();
  this._log = Log.repository.getLoggerWithMessagePrefix(
    "Browser.Experiments.Experiments",
    "ExperimentEntry #" + gExperimentEntryCounter++ + "::");

  // Is this experiment running?
  this._enabled = false;
  // When this experiment was started, if ever.
  this._startDate = null;
  // When this experiment was ended, if ever.
  this._endDate = null;
  // The condition data from the manifest.
  this._manifestData = null;
  // For an active experiment, signifies whether we need to update the xpi.
  this._needsUpdate = false;
  // A random sample value for comparison against the manifest conditions.
  this._randomValue = null;
  // When this entry was last changed for respecting history retention duration.
  this._lastChangedDate = null;
  // Has this experiment failed to activate before?
  this._failedStart = false;

  // We grab these from the addon after download.
  this._name = null;
  this._description = null;
  this._homepageURL = null;
  this._addonId = null;
};

Experiments.ExperimentEntry.prototype = {
  MANIFEST_REQUIRED_FIELDS: new Set([
    "id",
    "xpiURL",
    "xpiHash",
    "startTime",
    "endTime",
    "maxActiveSeconds",
    "appName",
    "channel",
  ]),

  MANIFEST_OPTIONAL_FIELDS: new Set([
    "maxStartTime",
    "minVersion",
    "maxVersion",
    "version",
    "minBuildID",
    "maxBuildID",
    "buildIDs",
    "os",
    "locale",
    "sample",
    "disabled",
    "frozen",
    "jsfilter",
  ]),

  SERIALIZE_KEYS: new Set([
    "_enabled",
    "_manifestData",
    "_needsUpdate",
    "_randomValue",
    "_failedStart",
    "_name",
    "_description",
    "_homepageURL",
    "_addonId",
    "_startDate",
    "_endDate",
  ]),

  DATE_KEYS: new Set([
    "_startDate",
    "_endDate",
  ]),

  /*
   * Initialize entry from the manifest.
   * @param data The experiment data from the manifest.
   * @return boolean Whether initialization succeeded.
   */
  initFromManifestData: function (data) {
    if (!this._isManifestDataValid(data)) {
      return false;
    }

    this._manifestData = data;

    this._randomValue = this._policy.random();
    this._lastChangedDate = this._policy.now();

    return true;
  },

  get enabled() {
    return this._enabled;
  },

  get id() {
    return this._manifestData.id;
  },

  get startDate() {
    return this._startDate;
  },

  get endDate() {
    if (!this._startDate) {
      return null;
    }

    let endTime = 0;

    if (!this._enabled) {
      return this._endDate;
    }

    let maxActiveMs = 1000 * this._manifestData.maxActiveSeconds;
    endTime = Math.min(1000 * this._manifestData.endTime,
                       this._startDate.getTime() + maxActiveMs);

    return new Date(endTime);
  },

  get needsUpdate() {
    return this._needsUpdate;
  },

  /*
   * Initialize entry from the cache.
   * @param data The entry data from the cache.
   * @return boolean Whether initialization succeeded.
   */
  initFromCacheData: function (data) {
    for (let key of this.SERIALIZE_KEYS) {
      if (!(key in data) && !this.DATE_KEYS.has(key)) {
        this._log.error("initFromCacheData() - missing required key " + key);
        return false;
      }
    };

    if (!this._isManifestDataValid(data._manifestData)) {
      return false;
    }

    // Dates are restored separately from epoch ms, everything else is just
    // copied in.

    this.SERIALIZE_KEYS.forEach(key => {
      if (!this.DATE_KEYS.has(key)) {
        this[key] = data[key];
      }
    });

    this.DATE_KEYS.forEach(key => {
      if (key in data) {
        let date = new Date();
        date.setTime(data[key]);
        this[key] = date;
      }
    });

    this._lastChangedDate = this._policy.now();

    return true;
  },

  /*
   * Returns a JSON representation of this object.
   */
  toJSON: function () {
    let obj = {};

    // Dates are serialized separately as epoch ms.

    this.SERIALIZE_KEYS.forEach(key => {
      if (!this.DATE_KEYS.has(key)) {
        obj[key] = this[key];
      }
    });

    this.DATE_KEYS.forEach(key => {
      if (this[key]) {
        obj[key] = this[key].getTime();
      }
    });

    return obj;
  },

  /*
   * Update from the experiment data from the manifest.
   * @param data The experiment data from the manifest.
   * @return boolean Whether updating succeeded.
   */
  updateFromManifestData: function (data) {
    let old = this._manifestData;

    if (!this._isManifestDataValid(data)) {
      return false;
    }

    if (this._enabled) {
      if (old.xpiHash !== data.xpiHash) {
        // A changed hash means we need to update active experiments.
        this._needsUpdate = true;
      }
    } else if (this._failedStart &&
               (old.xpiHash !== data.xpiHash) ||
               (old.xpiURL !== data.xpiURL)) {
      // Retry installation of previously invalid experiments
      // if hash or url changed.
      this._failedStart = false;
    }

    this._manifestData = data;
    this._lastChangedDate = this._policy.now();

    return true;
  },

  /*
   * Is this experiment applicable?
   * @return Promise<> Resolved if the experiment is applicable.
   *                   If it is not applicable it is rejected with
   *                   a Promise<string> which contains the reason.
   */
  isApplicable: function () {
    let versionCmp = Cc["@mozilla.org/xpcom/version-comparator;1"]
                              .getService(Ci.nsIVersionComparator);
    let app = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
    let runtime = Cc["@mozilla.org/xre/app-info;1"]
                    .getService(Ci.nsIXULRuntime);

    let locale = this._policy.locale();
    let channel = this._policy.updatechannel();
    let data = this._manifestData;

    let now = this._policy.now() / 1000; // The manifest times are in seconds.
    let minActive = MIN_EXPERIMENT_ACTIVE_SECONDS;
    let maxActive = data.maxActiveSeconds || 0;
    let startSec = (this.startDate || 0) / 1000;

    this._log.trace("isApplicable() - now=" + now
                    + ", randomValue=" + this._randomValue
                    + ", data=" + JSON.stringify(this._manifestData));

    // Not applicable if it already ran.

    if (!this.enabled && this._endDate) {
      return Promise.reject(["was-active"]);
    }

    // Define and run the condition checks.

    let simpleChecks = [
      { name: "failedStart",
        condition: () => !this._failedStart },
      { name: "disabled",
        condition: () => !data.disabled },
      { name: "frozen",
        condition: () => !data.frozen || this._enabled },
      { name: "startTime",
        condition: () => now >= data.startTime },
      { name: "endTime",
        condition: () => now < data.endTime },
      { name: "maxStartTime",
        condition: () => !data.maxStartTime || now <= data.maxStartTime },
      { name: "maxActiveSeconds",
        condition: () => !this._startDate || now <= (startSec + maxActive) },
      { name: "appName",
        condition: () => !data.appName || data.appName.indexOf(app.name) != -1 },
      { name: "minBuildID",
        condition: () => !data.minBuildID || app.platformBuildID >= data.minBuildID },
      { name: "maxBuildID",
        condition: () => !data.maxBuildID || app.platformBuildID <= data.maxBuildID },
      { name: "buildIDs",
        condition: () => !data.buildIDs || data.buildIDs.indexOf(app.platformBuildID) != -1 },
      { name: "os",
        condition: () => !data.os || data.os.indexOf(runtime.OS) != -1 },
      { name: "channel",
        condition: () => !data.channel || data.channel.indexOf(channel) != -1 },
      { name: "locale",
        condition: () => !data.locale || data.locale.indexOf(locale) != -1 },
      { name: "sample",
        condition: () => data.sample === undefined || this._randomValue <= data.sample },
      { name: "version",
        condition: () => !data.version || data.version.indexOf(app.version) != -1 },
      { name: "minVersion",
        condition: () => !data.minVersion || versionCmp.compare(app.version, data.minVersion) >= 0 },
      { name: "maxVersion",
        condition: () => !data.maxVersion || versionCmp.compare(app.version, data.maxVersion) <= 0 },
    ];

    for (let check of simpleChecks) {
      let result = check.condition();
      if (!result) {
        this._log.debug("isApplicable() - id="
                        + data.id + " - test '" + check.name + "' failed");
        return Promise.reject([check.name]);
      }
    }

    if (data.jsfilter) {
      return this._runFilterFunction(data.jsfilter);
    }

    return Promise.resolve(true);
  },

  /*
   * Run the jsfilter function from the manifest in a sandbox and return the
   * result (forced to boolean).
   */
  _runFilterFunction: function (jsfilter) {
    this._log.trace("runFilterFunction() - filter: " + jsfilter);

    return Task.spawn(function ExperimentEntry_runFilterFunction_task() {
      const nullprincipal = Cc["@mozilla.org/nullprincipal;1"].createInstance(Ci.nsIPrincipal);
      let options = {
        sandboxName: "telemetry experiments jsfilter sandbox",
        wantComponents: false,
      };

      let sandbox = Cu.Sandbox(nullprincipal);
      let context = {};
      context.healthReportPayload = yield this._policy.healthReportPayload();
      context.telemetryPayload    = yield this._policy.telemetryPayload();

      try {
        Cu.evalInSandbox(jsfilter, sandbox);
      } catch (e) {
        this._log.error("runFilterFunction() - failed to eval jsfilter: " + e.message);
        throw ["jsfilter-evalfailed"];
      }

      // You can't insert arbitrarily complex objects into a sandbox, so
      // we serialize everything through JSON.
      sandbox._hr = JSON.stringify(yield this._policy.healthReportPayload());
      Object.defineProperty(sandbox, "_t",
        { get: () => JSON.stringify(this._policy.telemetryPayload()) });

      let result = false;
      try {
        result = !!Cu.evalInSandbox("filter({healthReportPayload: JSON.parse(_hr), telemetryPayload: JSON.parse(_t)})", sandbox);
      }
      catch (e) {
        this._log.debug("runFilterFunction() - filter function failed: "
                      + e.message + ", " + e.stack);
        throw ["jsfilter-threw", e.message];
      }
      finally {
        Cu.nukeSandbox(sandbox);
      }

      if (!result) {
        throw ["jsfilter-false"];
      }

      throw new Task.Result(true);
    }.bind(this));
  },

  /*
   * Start running the experiment.
   * @return Promise<> Resolved when the operation is complete.
   */
  start: function () {
    this._log.trace("start() for " + this.id);

    return Task.spawn(function* ExperimentEntry_start_task() {
      let addons = yield installedExperimentAddons();
      if (addons.length > 0) {
        this._log.error("start() - there are already "
                        + addons.length + " experiment addons installed");
        yield uninstallAddons(addons);
      }

      yield this._installAddon();
    }.bind(this));
  },

  // Async install of the addon for this experiment, part of the start task above.
  _installAddon: function* () {
    let deferred = Promise.defer();

    let install = yield addonInstallForURL(this._manifestData.xpiURL,
                                           this._manifestData.xpiHash);
    gActiveInstallURLs.add(install.sourceURI.spec);

    let failureHandler = (install, handler) => {
      let message = "AddonInstall " + handler + " for " + this.id + ", state=" +
                   (install.state || "?") + ", error=" + install.error;
      this._log.error("_installAddon() - " + message);
      this._failedStart = true;
      gActiveInstallURLs.delete(install.sourceURI.spec);

      TelemetryLog.log(TELEMETRY_LOG.ACTIVATION_KEY,
                      [TELEMETRY_LOG.ACTIVATION.INSTALL_FAILURE, this.id]);

      deferred.reject(new Error(message));
    };

    let listener = {
      _expectedID: null,

      onDownloadEnded: install => {
        this._log.trace("_installAddon() - onDownloadEnded for " + this.id);

        if (install.existingAddon) {
          this._log.warn("_installAddon() - onDownloadEnded, addon already installed");
        }

        if (install.addon.type !== "experiment") {
          this._log.error("_installAddon() - onDownloadEnded, wrong addon type");
          install.cancel();
        }
      },

      onInstallStarted: install => {
        this._log.trace("_installAddon() - onInstallStarted for " + this.id);

        if (install.existingAddon) {
          this._log.warn("_installAddon() - onInstallStarted, addon already installed");
        }

        if (install.addon.type !== "experiment") {
          this._log.error("_installAddon() - onInstallStarted, wrong addon type");
          return false;
        }
      },

      onInstallEnded: install => {
        this._log.trace("_installAddon() - install ended for " + this.id);
        gActiveInstallURLs.delete(install.sourceURI.spec);

        this._lastChangedDate = this._policy.now();
        this._startDate = this._policy.now();
        this._enabled = true;

        TelemetryLog.log(TELEMETRY_LOG.ACTIVATION_KEY,
                       [TELEMETRY_LOG.ACTIVATION.ACTIVATED, this.id]);

        let addon = install.addon;
        this._name = addon.name;
        this._addonId = addon.id;
        this._description = addon.description || "";
        this._homepageURL = addon.homepageURL || "";

        // Experiment add-ons default to userDisabled=true. Enable if needed.
        if (addon.userDisabled) {
          this._log.trace("Add-on is disabled. Enabling.");
          listener._expectedID = addon.id;
          AddonManager.addAddonListener(listener);
          addon.userDisabled = false;
        } else {
          this._log.trace("Add-on is enabled. start() completed.");
          deferred.resolve();
        }
      },

      onEnabled: addon => {
        this._log.info("onEnabled() for " + addon.id);

        if (addon.id != listener._expectedID) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        deferred.resolve();
      },
    };

    ["onDownloadCancelled", "onDownloadFailed", "onInstallCancelled", "onInstallFailed"]
      .forEach(what => {
        listener[what] = install => failureHandler(install, what)
      });

    install.addListener(listener);
    install.install();

    return deferred.promise;
  },

  /*
   * Stop running the experiment if it is active.
   * @param terminationKind (optional) The termination kind, e.g. USERDISABLED or EXPIRED.
   * @param terminationReason (optional) The termination reason details for
   *                          termination kind RECHECK.
   * @return Promise<> Resolved when the operation is complete.
   */
  stop: function (terminationKind, terminationReason) {
    this._log.trace("stop() - id=" + this.id + ", terminationKind=" + terminationKind);
    if (!this._enabled) {
      this._log.warning("stop() - experiment not enabled: " + id);
      return Promise.reject();
    }

    this._enabled = false;
    let deferred = Promise.defer();
    let updateDates = () => {
      let now = this._policy.now();
      this._lastChangedDate = now;
      this._endDate = now;
    };

    this._getAddon().then((addon) => {
      if (!addon) {
        let message = "could not get Addon for " + this.id;
        this._log.warn("stop() - " + message);
        updateDates();
        deferred.resolve();
        return;
      }

      updateDates();
      this._logTermination(terminationKind, terminationReason);
      deferred.resolve(uninstallAddons([addon]));
    });

    return deferred.promise;
  },

  /**
   * Try to ensure this experiment is active.
   *
   * The returned promise will be resolved if the experiment is active
   * in the Addon Manager or rejected if it isn't.
   *
   * @return Promise<>
   */
  ensureActive: Task.async(function* () {
    this._log.trace("ensureActive() for " + this.id);

    let addon = yield this._getAddon();
    if (!addon) {
      this._log.warn("Experiment is not installed: " + this._addonId);
      throw new Error("Experiment is not installed: " + this._addonId);
    }

    // User disabled likely means the experiment is disabled at startup,
    // since the permissions don't allow it to be disabled by the user.
    if (!addon.userDisabled) {
      return;
    }

    let deferred = Promise.defer();

    let listener = {
      onEnabled: enabledAddon => {
        if (enabledAddon.id != addon.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        deferred.resolve();
      },
    };

    this._log.info("Activating add-on: " + addon.id);
    AddonManager.addAddonListener(listener);
    addon.userDisabled = false;
    yield deferred.promise;
  }),

  /**
   * Obtain the underlying Addon from the Addon Manager.
   *
   * @return Promise<Addon|null>
   */
  _getAddon: function () {
    let deferred = Promise.defer();

    AddonManager.getAddonByID(this._addonId, deferred.resolve);

    return deferred.promise;
  },

  _logTermination: function (terminationKind, terminationReason) {
    if (terminationKind === undefined) {
      return;
    }

    if (!(terminationKind in TELEMETRY_LOG.TERMINATION)) {
      this._log.warn("stop() - unknown terminationKind " + terminationKind);
      return;
    }

    let data = [terminationKind, this.id];
    if (terminationReason) {
      data = data.concat(terminationReason);
    }

    TelemetryLog.log(TELEMETRY_LOG.TERMINATION_KEY, data);
  },

  /*
   * Stop if experiment stop criteria are met.
   * @return Promise<boolean> Resolved when done stopping or checking,
   *                          the value indicates whether it was stopped.
   */
  maybeStop: function () {
    this._log.trace("maybeStop()");

    return Task.spawn(function ExperimentEntry_maybeStop_task() {
      let result = yield this._shouldStop();
      if (result.shouldStop) {
        let expireReasons = ["endTime", "maxActiveSeconds"];
        if (expireReasons.indexOf(result.reason[0]) != -1) {
          yield this.stop(TELEMETRY_LOG.TERMINATION.EXPIRED);
        } else {
          yield this.stop(TELEMETRY_LOG.TERMINATION.RECHECK, result.reason);
        }
      }

      throw new Task.Result(result.shouldStop);
    }.bind(this));
  },

  _shouldStop: function () {
    let data = this._manifestData;
    let now = this._policy.now() / 1000; // The manifest times are in seconds.
    let maxActiveSec = data.maxActiveSeconds || 0;

    if (!this._enabled) {
      return Promise.resolve({shouldStop: false});
    }

    let deferred = Promise.defer();
    this.isApplicable().then(
      () => deferred.resolve({shouldStop: false}),
      reason => deferred.resolve({shouldStop: true, reason: reason})
    );

    return deferred.promise;
  },

  /*
   * Should this be discarded from the cache due to age?
   */
  shouldDiscard: function () {
    let limit = this._policy.now();
    limit.setDate(limit.getDate() - KEEP_HISTORY_N_DAYS);
    return (this._lastChangedDate < limit);
  },

  /*
   * Get next date (in epoch-ms) to schedule a re-evaluation for this.
   * Returns 0 if it doesn't need one.
   */
  getScheduleTime: function () {
    if (this._enabled) {
      let now = this._policy.now();
      let startTime = this._startDate.getTime();
      let maxActiveTime = startTime + 1000 * this._manifestData.maxActiveSeconds;
      return Math.min(1000 * this._manifestData.endTime,  maxActiveTime);
    }

    if (this._endDate) {
      return this._endDate.getTime();
    }

    return 1000 * this._manifestData.startTime;
  },

  /*
   * Perform sanity checks on the experiment data.
   */
  _isManifestDataValid: function (data) {
    this._log.trace("isManifestDataValid() - data: " + JSON.stringify(data));

    for (let key of this.MANIFEST_REQUIRED_FIELDS) {
      if (!(key in data)) {
        this._log.error("isManifestDataValid() - missing required key: " + key);
        return false;
      }
    }

    for (let key in data) {
      if (!this.MANIFEST_OPTIONAL_FIELDS.has(key) &&
          !this.MANIFEST_REQUIRED_FIELDS.has(key)) {
        this._log.error("isManifestDataValid() - unknown key: " + key);
        return false;
      }
    }

    return true;
  },
};



/**
 * Strip a Date down to its UTC midnight.
 *
 * This will return a cloned Date object. The original is unchanged.
 */
let stripDateToMidnight = function (d) {
  let m = new Date(d);
  m.setUTCHours(0, 0, 0, 0);

  return m;
};

function ExperimentsLastActiveMeasurement1() {
  Metrics.Measurement.call(this);
}

const FIELD_DAILY_LAST_TEXT = {type: Metrics.Storage.FIELD_DAILY_LAST_TEXT};

ExperimentsLastActiveMeasurement1.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "info",
  version: 1,

  fields: {
    lastActive: FIELD_DAILY_LAST_TEXT,
  }
});

this.ExperimentsProvider = function () {
  Metrics.Provider.call(this);

  this._experiments = null;
};

ExperimentsProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.experiments",

  measurementTypes: [
    ExperimentsLastActiveMeasurement1,
  ],

  _OBSERVERS: [
    OBSERVER_TOPIC,
  ],

  postInit: function () {
    this._experiments = Experiments.instance();

    for (let o of this._OBSERVERS) {
      Services.obs.addObserver(this, o, false);
    }

    return Promise.resolve();
  },

  onShutdown: function () {
    for (let o of this._OBSERVERS) {
      Services.obs.removeObserver(this, o);
    }

    return Promise.resolve();
  },

  observe: function (subject, topic, data) {
    switch (topic) {
      case OBSERVER_TOPIC:
        this.recordLastActiveExperiment();
        break;
    }
  },

  collectDailyData: function () {
    return this.recordLastActiveExperiment();
  },

  recordLastActiveExperiment: function () {
    let m = this.getMeasurement(ExperimentsLastActiveMeasurement1.prototype.name,
                                ExperimentsLastActiveMeasurement1.prototype.version);

    return this.enqueueStorageOperation(() => {
      return Task.spawn(function* recordTask() {
        let todayActive = yield this._experiments.lastActiveToday();
        if (!todayActive) {
          this._log.info("No active experiment on this day: " +
                         this._experiments._policy.now());
          return;
        }

        this._log.info("Recording last active experiment: " + todayActive.id);
        yield m.setDailyLastText("lastActive", todayActive.id,
                                 this._experiments._policy.now());
      }.bind(this));
    });
  },
});
