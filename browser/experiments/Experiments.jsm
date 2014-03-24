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
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");
Cu.import("resource://gre/modules/Metrics.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryPing",
                                  "resource://gre/modules/TelemetryPing.jsm");
// CertUtils.jsm doesn't expose a single "CertUtils" object like a normal .jsm
// would.
XPCOMUtils.defineLazyGetter(this, "CertUtils",
  function() {
    var mod = {};
    Cu.import("resource://gre/modules/CertUtils.jsm", mod);
    return mod;
  });


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
const PREF_TELEMETRY_PRERELEASE = "enabledPreRelease";

const gPrefs = new Preferences(PREF_BRANCH);
const gPrefsTelemetry = new Preferences(PREF_BRANCH_TELEMETRY);
let gExperimentsEnabled = false;
let gExperiments = null;
let gLogAppenderDump = null;

XPCOMUtils.defineLazyGetter(this, "gLogger", function() {
  let logger = Log.repository.getLogger("Browser.Experiments");
  logger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
  return logger;
});


function configureLogging() {
  gLogger.level = gPrefs.get(PREF_LOGGING_LEVEL, 50);

  if (gPrefs.get(PREF_LOGGING_DUMP, false)) {
    gLogAppenderDump = new Log.DumpAppender(new Log.BasicFormatter());
    gLogger.addAppender(gLogAppenderDump);
  } else {
    gLogger.removeAppender(gLogAppenderDump);
    gLogAppenderDump = null;
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
  return gPrefsTelemetry.get(PREF_TELEMETRY_ENABLED, false) ||
         gPrefsTelemetry.get(PREF_TELEMETRY_PRERELEASE, false);
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
};

Experiments.Policy.prototype = {
  now: function () {
    return new Date();
  },

  random: function () {
    let pref = gPrefs.get(PREF_FORCE_SAMPLE);
    if (pref !== undefined) {
      let val = Number.parseFloat(pref);
      gLogger.debug("Experiments::Policy::random sample forced: " + val);
      if (IsNaN(val) || val < 0) {
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
  this._policy = policy;

  // This is a Map of (string -> ExperimentEntry), keyed with the experiment id.
  // It holds both the current experiments and history.
  // Map() preserves insertion order, which means we preserve the manifest order.
  this._experiments = null;
  // Holds tasks that hit the experiments list and files.
  // We need to serialize them, so only one is running at a time.
  this._pendingTasks = {
    updateManifest: null,
    loadFromCache: null,
    saveToCache: null,
    evaluateExperiments: null,
    disableExperiment: null,
  };
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
    gLogger.trace("enabled="+gExperimentsEnabled+", "+this.enabled);

    gPrefs.observe(PREF_LOGGING, configureLogging);
    gPrefs.observe(PREF_MANIFEST_URI, this.updateManifest, this);
    gPrefs.observe(PREF_ENABLED, this._toggleExperimentsEnabled, this);

    gPrefsTelemetry.observe(PREF_TELEMETRY_ENABLED, this._telemetryStatusChanged, this);
    gPrefsTelemetry.observe(PREF_TELEMETRY_PRERELEASE, this._telemetryStatusChanged, this);

    AsyncShutdown.profileBeforeChange.addBlocker("Experiments.jsm shutdown",
      this.uninit.bind(this));

    AddonManager.addAddonListener(this);

    this._experiments = new Map();
    this._loadFromCache();
  },

  /**
   * @return Promise<>
   *         The promise is fulfilled when all pending tasks are finished.
   */
  uninit: function () {
    if (!this._shutdown) {
      AddonManager.removeAddonListener(this);

      gPrefs.ignore(PREF_LOGGING, configureLogging);
      gPrefs.ignore(PREF_MANIFEST_URI, this.updateManifest, this);
      gPrefs.ignore(PREF_ENABLED, this._toggleExperimentsEnabled, this);

      gPrefsTelemetry.ignore(PREF_TELEMETRY_ENABLED, this._telemetryStatusChanged, this);
      gPrefsTelemetry.ignore(PREF_TELEMETRY_PRERELEASE, this._telemetryStatusChanged, this);

      if (this._timer) {
        this._timer.clear();
      }
    }

    this._shutdown = true;
    if (this._pendingTasks.saveToCache) {
      return this._pendingTasks.saveToCache;
    }
    return Promise.resolve();
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
    gLogger.trace("Experiments::set enabled(" + enabled + ")");
    gPrefs.set(PREF_ENABLED, enabled);
  },

  _toggleExperimentsEnabled: function (enabled) {
    gLogger.trace("Experiments::_toggleExperimentsEnabled(" + enabled + ")");
    let wasEnabled = gExperimentsEnabled;
    gExperimentsEnabled = enabled && telemetryEnabled();

    if (wasEnabled == gExperimentsEnabled) {
      return Promise.resolve();
    }

    if (gExperimentsEnabled) {
      return this.updateManifest();
    }

    let promise = this._disableExperiments();
    if (wasEnabled) {
      Services.obs.notifyObservers(null, OBSERVER_TOPIC, null);
    }
    return promise;
  },

  _telemetryStatusChanged: function () {
    _toggleExperimentsEnabled(gExperimentsEnabled);
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
    return Promise.resolve(this._experiments || this._loadFromCache()).then(
      () => {
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
      },
      () => []
    );
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

  /**
   * Fetch an updated list of experiments and trigger experiment updates.
   * Do only use when experiments are enabled.
   *
   * @return Promise<>
   *         The promise is resolved when the manifest and experiment list is updated.
   */
  updateManifest: function () {
    gLogger.trace("Experiments::updateManifest()");

    if (!gExperimentsEnabled) {
      return Promise.reject(new Error("experiments are disabled"));
    }

    if (this._shutdown) {
      return Promise.reject(Error("uninit() alrady called"));
    }

    if (this._pendingTasks.updateManifest) {
      return this._pendingTasks.updateManifest;
    }

    let uri = Services.urlFormatter.formatURLPref(PREF_BRANCH + PREF_MANIFEST_URI);

    this._pendingTasks.updateManifest = Task.spawn(function () {
      this._checkForShutdown();

      // Don't interfere while we're saving or loading the cache.
      try {
        yield this._pendingTasksDone([this._pendingTasks.updateManifest]);
      } catch (e) {
        // Ignore a failed cache load/save.
      }

      try {
        let responseText = yield this._httpGetRequest(uri);
        gLogger.trace("Experiments::updateManifest::updateTask() - responseText=\"" + responseText + "\"");

        this._checkForShutdown();

        let data = JSON.parse(responseText);
        this._updateExperiments(data);
        yield this._evaluateExperiments();
        this._scheduleExperimentEvaluation();
      } catch (e if e instanceof SyntaxError) {
        gLogger.error("Experiments::updateManifest::updateTask() - failed to parse manifest - " + e);
        throw e;
      } finally {
        this._pendingTasks.updateManifest = null;
      }

      yield this._saveToCache(this._experiments);
    }.bind(this));

    return this._pendingTasks.updateManifest;
  },

  notify: function (timer) {
    gLogger.trace("Experiments::notify()");

    this._checkForShutdown();

    if (this._pendingTasks.evaluateExperiments) {
      return;
    }

    this._pendingTasks.evaluateExperiments = new Task.spawn(function scheduledEvaluateTask() {
      gLogger.trace("Experiments::notify::scheduledEvaluateTask()");
      yield this._pendingTasksDone([this._pendingTasks.evaluateExperiments]);
      yield this._evaluateExperiments();
      this._pendingTasks.evaluateExperiments = null;
      this._scheduleExperimentEvaluation();
    }.bind(this));
  },

  onDisabled: function (addon) {
    gLogger.trace("Experiments::onDisabled() - addon id: " + addon.id)
    this.disableExperiment(addon.id);
  },

  onUninstalled: function (addon) {
    gLogger.trace("Experiments::onUninstalled() - addon id: " + addon.id);
    this.disableExperiment(addon.id);
  },

  _getExperimentByAddonId: function (addonId) {
    for (let [, entry] of this._experiments) {
      if (entry._addonId === addonId) {
        return entry;
      }
    }

    return null;
  },

  _disableExperimentByAddonId: function (addonId) {
    this._checkForShutdown();
    gLogger.trace("Experiments::disableExperimentByAddonId() - addon id: " + addonId);
    let experiment = this._getExperimentByAddonId(addonId);
    if (!experiment) {
      return;
    }

    this.disableExperiment(experiment.id);
  },

  /*
   * Helper function to make HTTP GET requests. Returns a promise that is resolved with
   * the responseText when the request is complete.
   */
  _httpGetRequest: function (url) {
    gLogger.trace("Experiments::httpGetRequest(" + url + ")");
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    try {
      xhr.open("GET", url);
    } catch (e) {
      gLogger.error("Experiments::httpGetRequest() - Error opening request to " + url + ": " + e);
      return Promise.reject(new Error("Experiments - Error opening XHR for " + url));
    }

    let deferred = Promise.defer();

    xhr.onerror = function (e) {
      gLogger.error("Experiments::httpGetRequest::onError() - Error making request to " + url + ": " + e.error);
      deferred.reject(new Error("Experiments - XHR error for " + url + " - " + e.error));
    };

    xhr.onload = function (event) {
      if (xhr.status !== 200 && xhr.state !== 0) {
        gLogger.error("Experiments::httpGetRequest::onLoad() - Request to " + url + " returned status " + xhr.status);
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
        gLogger.error("Experiments: manifest fetch failed certificate checks", [e]);
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
   * Returns a promise that is resolved when all _pendingTasks are resolved
   * or rejected.
   * exceptThese is optional and allows to specify exceptions of tasks not to
   * wait on.
   */
  _pendingTasksDone: function (exceptThese) {
    exceptThese = exceptThese || [];
    let ts = this._pendingTasks;
    let list = [ts[k] for (k of Object.keys(ts))
                  if (ts.hasOwnProperty(k) && exceptThese.indexOf(ts[k]) == -1)]
    return allResolvedOrRejected(list);
  },

  /*
   * Save the experiment data on disk. Returns a promise that
   * is resolved when the operation is done.
   */
  _saveToCache: function (data) {
    if (this._pendingTasks.saveToCache) {
      return this._pendingTasks.saveToCache;
    }

    let path = this._cacheFilePath;
    let textData = JSON.stringify({
      version: CACHE_VERSION,
      data: [e[1].toJSON() for (e of data.entries())],
    });

    this._pendingTasks.saveToCache = Task.spawn(function Experiments_saveToCache_fileTask() {
      try {
        yield this._pendingTasksDone([this._pendingTasks.saveToCache]);
        let encoder = new TextEncoder();
        let data = encoder.encode(textData);
        let options = { tmpPath: path + ".tmp", compression: "lz4" };
        yield OS.File.writeAtomic(path, data, options);
      } finally {
        this._pendingTasks.saveToCache = null;
      }
    }.bind(this)).then(
      () => gLogger.debug("Experiments::saveToCache::fileTask() saved to: " + path),
       e => gLogger.error("Experiments::saveToCache::fileTask() failed: " + e));

    return this._pendingTasks.saveToCache;
  },

  /*
   * Load the cached experiments manifest file from disk.
   * Returns a promise that is resolved when the experiments are loaded and updated.
   */
  _loadFromCache: function () {
    if (this._pendingTasks.loadFromCache) {
      return this._pendingTasks.loadFromCache;
    }

    if (this._pendingTasks.updateManifest) {
      // We're already updating the manifest, no need to load the cached version.
      return this._pendingTasks.updateManifest;
    }

    let path = this._cacheFilePath;
    this._pendingTasks.loadFromCache = Task.spawn(function () {
      try {
        yield this._pendingTasksDone([this._pendingTasks.loadFromCache]);
        let result = yield loadJSONAsync(path, { compression: "lz4" });

        this._populateFromCache(result);
        yield this._evaluateExperiments();
        this._scheduleExperimentEvaluation();
      } catch (e if e instanceof OS.File.Error && e.becauseNoSuchFile) {
        // No cached manifest yet.
      } finally {
        this._pendingTasks.loadFromCache = null;
      }
    }.bind(this)).then(null, error => {
      gLogger.error("Experiments::loadFromCache::fileTask() failed: " + error);
    });

    return this._pendingTasks.loadFromCache;
  },

  _populateFromCache: function (data) {
    gLogger.trace("Experiments::populateFromCache() - data: " + JSON.stringify(data));

    if (CACHE_VERSION !== data.version) {
      gLogger.warn("Experiments::populateFromCache() - invalid cache version");
      return false;
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
    return true;
  },

  /*
   * Update the experiment entries from the experiments
   * array in the manifest
   */
  _updateExperiments: function (manifestObject) {
    gLogger.trace("Experiments::updateExperiments() - experiments: " + JSON.stringify(manifestObject));

    if (manifestObject.version !== MANIFEST_VERSION) {
      gLogger.warning("Experiments::updateExperiments() - unsupported version " + manifestObject.version);
      return false;
    }

    let experiments = new Map(); // The new experiments map

    // Collect new and updated experiments.
    for (let data of manifestObject.experiments) {
      let entry = this._experiments.get(data.id);

      if (entry) {
        if (!entry.updateFromManifestData(data)) {
          gLogger.error("Experiments::updateExperiments() - Invalid manifest data for " + data.id);
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
      if (experiments.has(id) || !entry.startDate || !entry.shouldDiscard()) {
        continue;
      }

      experiments.set(id, experiment);
    }

    this._experiments = experiments;
    return true;
  },

  _getActiveExperiment: function () {
    let enabled = [experiment for ([,experiment] of this._experiments) if (experiment._enabled)];

    if (enabled.length == 1) {
      return enabled[0];
    }

    if (enabled.length > 1) {
      gLogger.error("Experiments::getActiveExperimentId() - should not have more than 1 active experiment");
      throw new Error("have more than 1 active experiment");
    }

    return null;
  },

  /*
   * Stop running experiments and disable further activations.
   */
  _disableExperiments: function () {
    gLogger.trace("Experiments::disableExperiments()");

    if (this._shutdown) {
      return Promise.reject(Error("uninit() alrady called"));
    }

    let active = this._getActiveExperiment();
    let promise = Promise.resolve();
    if (active) {
      promise = active.stop();
    }

    if (this._timer) {
      this._timer.clear();
    }

    return promise;
  },

  /**
   * Disable an experiment by id.
   * @param experimentId The id of the experiment.
   * @param userDisabled (optional) Whether this is disabled as a result of a user action.
   * @return Promise<> Promise that will get resolved once the task is done or failed.
   */
  disableExperiment: function (experimentId, userDisabled=true) {
    gLogger.trace("Experiments::disableExperiment() - " + experimentId);

    this._checkForShutdown();

    let experiment = this._experiments.get(experimentId);
    if (!experiment) {
      let message = "no experiment with this id";
      gLogger.warning("Experiments::disableExperiment() - " + message);
      return Promise.reject(new Error(message));
    }

    if (!experiment.enabled) {
      return Promise.reject();
    }

    return Task.spawn(function* Experiments_disableExperimentTaskOuter() {
      // We need to wait on possible previous disable tasks.
      yield this._pendingTasks.disableExperiment;

      let disableTask = Task.spawn(function* Experiments_disableExperimentTaskInner() {
        yield this._pendingTasksDone([this._pendingTasks.disableExperiment]);
        yield experiment.stop(userDisabled);
        Services.obs.notifyObservers(null, OBSERVER_TOPIC, null);
        this._pendingTasks.disableExperiment = null;
      }.bind(this));

      this._pendingTasks.disableExperiment = disableTask;
      yield disableTask;
    }.bind(this));
  },

  /*
   * Check applicability of experiments, disable the active one if needed and
   * activate the first applicable candidate.
   * @return Promise<boolean> Resolved when done, the value indicates whether
   *                          the activity status of any experiment changed.
   */
  _evaluateExperiments: function () {
    gLogger.trace("Experiments::evaluateExperiments()");

    this._checkForShutdown();

    return Task.spawn(function Experiments_evaluateExperiments_evaluateTask() {
      this._checkForShutdown();
      let activeExperiment = this._getActiveExperiment();
      let activeChanged = false;

      if (activeExperiment) {
        let wasStopped = yield activeExperiment.maybeStop();
        if (wasStopped) {
          gLogger.debug("Experiments::evaluateExperiments() - stopped experiment "
                        + activeExperiment.id);
          activeExperiment = null;
          activeChanged = true;
        } else if (activeExperiment.needsUpdate) {
          gLogger.debug("Experiments::evaluateExperiments() - updating experiment "
                        + activeExperiment.id);
          try {
            yield activeExperiment.stop();
            yield activeExperiment.start();
          } catch (e) {
            // On failure try the next experiment.
            activeExperiment = null;
          }

          activeChanged = true;
        }
      }

      if (!activeExperiment) {
        for (let [id, experiment] of this._experiments) {
          let applicable;
          yield experiment.isApplicable().then(
            result => applicable = result,
            reason => {
              applicable = false;
              // TODO telemetry: experiment rejection reason
            }
          );

          if (applicable) {
            gLogger.debug("Experiments::evaluateExperiments() - activating experiment " + id);
            try {
              yield experiment.start();
              activeChanged = true;
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

      throw new Task.Result(activeChanged);
    }.bind(this));
  },

  /*
   * Schedule the soonest re-check of experiment applicability that is needed.
   */
  _scheduleExperimentEvaluation: function () {
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

    gLogger.trace("Experiments::scheduleExperimentEvaluation() - scheduling for "+time+", now: "+now);
    this._policy.oneshotTimer(this.notify, time - now, this, "_timer");
  },
};


/*
 * Represents a single experiment.
 */

Experiments.ExperimentEntry = function (policy) {
  this._policy = policy || new Experiments.Policy();

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
      if (!(key in data)) {
        return false;
      }
    };

    if (!this._isManifestDataValid(data._manifestData)) {
      return false;
    }

    this._lastChangedDate = this._policy.now();
    this.SERIALIZE_KEYS.forEach(key => {
      this[key] = data[key];
    });

    this.DATE_KEYS.forEach(key => {
      if (key in this) {
        let date = new Date();
        date.setTime(this[key]);
        this[key] = date;
      }
    });

    return true;
  },

  /*
   * Returns a JSON representation of this object.
   */
  toJSON: function () {
    let obj = {};

    this.SERIALIZE_KEYS.forEach(key => {
      if (!this.DATE_KEYS.has(key)) {
        obj[key] = this[key];
      }
    });

    this.DATE_KEYS.forEach(key => {
      obj[key] = this[key] ? this[key].getTime() : null;
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
    let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);

    let locale = chrome.getSelectedLocale("global");
    let channel = this._policy.updatechannel();
    let data = this._manifestData;

    let now = this._policy.now() / 1000; // The manifest times are in seconds.
    let minActive = MIN_EXPERIMENT_ACTIVE_SECONDS;
    let maxActive = data.maxActiveSeconds || 0;
    let startSec = (this.startDate || 0) / 1000;

    gLogger.trace("ExperimentEntry::isApplicable() - now=" + now
                  + ", data=" + JSON.stringify(this._manifestData));

    // Not applicable if it already ran.

    if (!this.enabled && this._endDate) {
      return Promise.reject("was already active");
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
        condition: () => !data.maxStartTime || now <= (data.maxStartTime - minActive) },
      { name: "maxActiveSeconds",
        condition: () => !this._startDate || now <= (startSec + maxActive) },
      { name: "appName",
        condition: () => !data.name || data.appName.indexOf(app.name) != -1 },
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
        condition: () => !data.sample || this._randomValue <= data.sample },
      { name: "version",
        condition: () => !data.version || data.appVersion.indexOf(app.version) != -1 },
      { name: "minVersion",
        condition: () => !data.minVersion || versionCmp.compare(app.version, data.minVersion) >= 0 },
      { name: "maxVersion",
        condition: () => !data.maxVersion || versionCmp.compare(app.version, data.maxVersion) <= 0 },
    ];

    for (let check of simpleChecks) {
      let result = check.condition();
      if (!result) {
        gLogger.debug("ExperimentEntry::isApplicable() - id="
                      + data.id + " - test '" + check.name + "' failed");
        return Promise.reject(check.name);
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
    gLogger.trace("ExperimentEntry::runFilterFunction() - filter: " + jsfilter);

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
        gLogger.error("ExperimentEntry::runFilterFunction() - failed to eval jsfilter: " + e.message);
        throw "jsfilter:evalFailure";
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
        gLogger.debug("ExperimentEntry::runFilterFunction() - filter function failed: "
                      + e.message + ", " + e.stack);
        throw "jsfilter:rejected " + e.message;
      }
      finally {
        Cu.nukeSandbox(sandbox);
      }

      throw new Task.Result(result);
    }.bind(this));
  },

  /*
   * Start running the experiment.
   * @return Promise<> Resolved when the operation is complete.
   */
  start: function () {
    gLogger.trace("ExperimentEntry::start() for " + this.id);
    let deferred = Promise.defer();

    let installCallback = install => {
      let failureHandler = (install, handler) => {
        let message = "AddonInstall " + handler + " for " + this.id + ", state=" +
                      (install.state || "?") + ", error=" + install.error;
        gLogger.error("ExperimentEntry::start() - " + message);
        this._failedStart = true;

        // TODO telemetry: install failure

        deferred.reject(new Error(message));
      };

      let listener = {
        onDownloadEnded: install => {
          gLogger.trace("ExperimentEntry::start() - onDownloadEnded for " + this.id);
        },

        onInstallStarted: install => {
          gLogger.trace("ExperimentEntry::start() - onInstallStarted for " + this.id);
          // TODO: this check still needs changes in the addon manager
          //if (install.addon.type !== "experiment") {
          //  gLogger.error("ExperimentEntry::start() - wrong addon type");
          //  failureHandler({state: -1, error: -1}, "onInstallStarted");
          //}

          let addon = install.addon;
          this._name = addon.name;
          this._addonId = addon.id;
          this._description = addon.description || "";
          this._homepageURL = addon.homepageURL || "";
        },

        onInstallEnded: install => {
          gLogger.trace("ExperimentEntry::start() - install ended for " + this.id);
          this._lastChangedDate = this._policy.now();
          this._startDate = this._policy.now();
          this._enabled = true;

          // TODO telemetry: install success

          deferred.resolve();
        },
      };

      ["onDownloadCancelled", "onDownloadFailed", "onInstallCancelled", "onInstallFailed"]
        .forEach(what => {
          listener[what] = install => failureHandler(install, what)
        });

      install.addListener(listener);
      install.install();
    };

    AddonManager.getInstallForURL(this._manifestData.xpiURL,
                                  installCallback,
                                  "application/x-xpinstall",
                                  this._manifestData.xpiHash);
    return deferred.promise;
  },

  /*
   * Stop running the experiment if it is active.
   * @param userDisabled (optional) Whether this is disabled by user action, defaults to false.
   * @return Promise<> Resolved when the operation is complete.
   */
  stop: function (userDisabled=false) {
    gLogger.trace("ExperimentEntry::stop() - id=" + this.id + ", userDisabled=" + userDisabled);
    if (!this._enabled) {
      gLogger.warning("ExperimentEntry::stop() - experiment not enabled: " + id);
      return Promise.reject();
    }

    this._enabled = false;
    let deferred = Promise.defer();
    let updateDates = () => {
      let now = this._policy.now();
      this._lastChangedDate = now;
      this._endDate = now;
    };

    AddonManager.getAddonByID(this._addonId, addon => {
      if (!addon) {
        let message = "could not get Addon for " + this.id;
        gLogger.warn("ExperimentEntry::stop() - " + message);
        updateDates();
        deferred.resolve();
        return;
      }

      let listener = {};
      let handler = addon => {
        if (addon.id !== this._addonId) {
          return;
        }

        updateDates();

        AddonManager.removeAddonListener(listener);
        deferred.resolve();
      };

      listener.onUninstalled = handler;
      listener.onDisabled = handler;

      AddonManager.addAddonListener(listener);

      addon.uninstall();

      // TODO telemetry: experiment disabling, differentiate by userDisabled
    });

    return deferred.promise;
  },

  /*
   * Stop if experiment stop criteria are met.
   * @return Promise<boolean> Resolved when done stopping or checking,
   *                          the value indicates whether it was stopped.
   */
  maybeStop: function () {
    gLogger.trace("ExperimentEntry::maybeStop()");

    return Task.spawn(function ExperimentEntry_maybeStop_task() {
      let shouldStop = yield this._shouldStop();
      if (shouldStop) {
        yield this.stop();
      }
      throw new Task.Result(shouldStop);
    }.bind(this));
  },

  _shouldStop: function () {
    let data = this._manifestData;
    let now = this._policy.now() / 1000; // The manifest times are in seconds.
    let maxActiveSec = data.maxActiveSeconds || 0;

    if (!this._enabled) {
      return Promise.resolve(false);
    }

    let deferred = Promise.defer();
    this.isApplicable().then(
      () => deferred.resolve(false),
      () => deferred.resolve(true)
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
    gLogger.trace("ExperimentEntry::isManifestDataValid() - data: " + JSON.stringify(data));

    for (let key of this.MANIFEST_REQUIRED_FIELDS) {
      if (!(key in data)) {
        gLogger.error("ExperimentEntry::isManifestDataValid() - missing required key: " + key);
        return false;
      }
    }

    for (let key in data) {
      if (!this.MANIFEST_OPTIONAL_FIELDS.has(key) &&
          !this.MANIFEST_REQUIRED_FIELDS.has(key)) {
        gLogger.error("ExperimentEntry::isManifestDataValid() - unknown key: " + key);
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
