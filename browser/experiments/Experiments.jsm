/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Experiments",
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

XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                  "resource://gre/modules/UpdateUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManagerPrivate",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryEnvironment",
                                  "resource://gre/modules/TelemetryEnvironment.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryLog",
                                  "resource://gre/modules/TelemetryLog.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryUtils",
                                  "resource://gre/modules/TelemetryUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");

XPCOMUtils.defineLazyServiceGetter(this, "gCrashReporter",
                                   "@mozilla.org/xre/app-info;1",
                                   "nsICrashReporter");

const FILE_CACHE                = "experiments.json";
const EXPERIMENTS_CHANGED_TOPIC = "experiments-changed";
const MANIFEST_VERSION          = 1;
const CACHE_VERSION             = 1;

const KEEP_HISTORY_N_DAYS       = 180;

const PREF_BRANCH               = "experiments.";
const PREF_ENABLED              = "enabled"; // experiments.enabled
const PREF_ACTIVE_EXPERIMENT    = "activeExperiment"; // whether we have an active experiment
const PREF_LOGGING              = "logging";
const PREF_LOGGING_LEVEL        = PREF_LOGGING + ".level"; // experiments.logging.level
const PREF_LOGGING_DUMP         = PREF_LOGGING + ".dump"; // experiments.logging.dump
const PREF_MANIFEST_URI         = "manifest.uri"; // experiments.logging.manifest.uri
const PREF_FORCE_SAMPLE         = "force-sample-value"; // experiments.force-sample-value

const PREF_BRANCH_TELEMETRY     = "toolkit.telemetry.";
const PREF_TELEMETRY_ENABLED    = "enabled";

const URI_EXTENSION_STRINGS     = "chrome://mozapps/locale/extensions/extensions.properties";
const STRING_TYPE_NAME          = "type.%ID%.name";

const CACHE_WRITE_RETRY_DELAY_SEC = 60 * 3;
const MANIFEST_FETCH_TIMEOUT_MSEC = 60 * 3 * 1000; // 3 minutes

const TELEMETRY_LOG = {
  // log(key, [kind, experimentId, details])
  ACTIVATION_KEY: "EXPERIMENT_ACTIVATION",
  ACTIVATION: {
    // Successfully activated.
    ACTIVATED: "ACTIVATED",
    // Failed to install the add-on.
    INSTALL_FAILURE: "INSTALL_FAILURE",
    // Experiment does not meet activation requirements. Details will
    // be provided.
    REJECTED: "REJECTED",
  },

  // log(key, [kind, experimentId, optionalDetails...])
  TERMINATION_KEY: "EXPERIMENT_TERMINATION",
  TERMINATION: {
    // The Experiments service was disabled.
    SERVICE_DISABLED: "SERVICE_DISABLED",
    // Add-on uninstalled.
    ADDON_UNINSTALLED: "ADDON_UNINSTALLED",
    // The experiment disabled itself.
    FROM_API: "FROM_API",
    // The experiment expired (e.g. by exceeding the end date).
    EXPIRED: "EXPIRED",
    // Disabled after re-evaluating conditions. If this is specified,
    // details will be provided.
    RECHECK: "RECHECK",
  },
};
XPCOMUtils.defineConstant(this, "TELEMETRY_LOG", TELEMETRY_LOG);

const gPrefs = new Preferences(PREF_BRANCH);
const gPrefsTelemetry = new Preferences(PREF_BRANCH_TELEMETRY);
var gExperimentsEnabled = false;
var gAddonProvider = null;
var gExperiments = null;
var gLogAppenderDump = null;
var gPolicyCounter = 0;
var gExperimentsCounter = 0;
var gExperimentEntryCounter = 0;
var gPreviousProviderCounter = 0;

// Tracks active AddonInstall we know about so we can deny external
// installs.
var gActiveInstallURLs = new Set();

// Tracks add-on IDs that are being uninstalled by us. This allows us
// to differentiate between expected uninstalled and user-driven uninstalls.
var gActiveUninstallAddonIDs = new Set();

var gLogger;
var gLogDumping = false;

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

// Loads a JSON file using OS.file. file is a string representing the path
// of the file to be read, options contains additional options to pass to
// OS.File.read.
// Returns a Promise resolved with the json payload or rejected with
// OS.File.Error or JSON.parse() errors.
function loadJSONAsync(file, options) {
  return Task.spawn(function*() {
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
    return data;
  });
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
  AddonManager.getAddonsByTypes(["experiment"], (addons) => {
    deferred.resolve(addons.filter(a => !a.appDisabled));
  });
  return deferred.promise;
}

// Takes an Array<Addon> and returns a promise that is resolved when the
// addons are uninstalled.
function uninstallAddons(addons) {
  let ids = new Set(addons.map(addon => addon.id));
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

var Experiments = {
  /**
   * Provides access to the global `Experiments.Experiments` instance.
   */
  instance: function() {
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

Experiments.Policy = function() {
  this._log = Log.repository.getLoggerWithMessagePrefix(
    "Browser.Experiments.Policy",
    "Policy #" + gPolicyCounter++ + "::");

  // Set to true to ignore hash verification on downloaded XPIs. This should
  // not be used outside of testing.
  this.ignoreHashes = false;
};

Experiments.Policy.prototype = {
  now: function() {
    return new Date();
  },

  random: function() {
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

  futureDate: function(offset) {
    return new Date(this.now().getTime() + offset);
  },

  oneshotTimer: function(callback, timeout, thisObj, name) {
    return CommonUtils.namedTimer(callback, timeout, thisObj, name);
  },

  updatechannel: function() {
    return UpdateUtils.UpdateChannel;
  },

  locale: function() {
    let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
    return chrome.getSelectedLocale("global");
  },

  /**
   * For testing a race condition, one of the tests delays the callback of
   * writing the cache by replacing this policy function.
   */
  delayCacheWrite: function(promise) {
    return promise;
  },
};

function AlreadyShutdownError(message = "already shut down") {
  Error.call(this, message);
  let error = new Error();
  this.name = "AlreadyShutdownError";
  this.message = message;
  this.stack = error.stack;
}
AlreadyShutdownError.prototype = Object.create(Error.prototype);
AlreadyShutdownError.prototype.constructor = AlreadyShutdownError;

function CacheWriteError(message = "Error writing cache file") {
  Error.call(this, message);
  let error = new Error();
  this.name = "CacheWriteError";
  this.message = message;
  this.stack = error.stack;
}
CacheWriteError.prototype = Object.create(Error.prototype);
CacheWriteError.prototype.constructor = CacheWriteError;

/**
 * Manages the experiments and provides an interface to control them.
 */

Experiments.Experiments = function(policy = new Experiments.Policy()) {
  let log = Log.repository.getLoggerWithMessagePrefix(
      "Browser.Experiments.Experiments",
      "Experiments #" + gExperimentsCounter++ + "::");

  // At the time of this writing, Experiments.jsm has severe
  // crashes. For forensics purposes, keep the last few log
  // messages in memory and upload them in case of crash.
  this._forensicsLogs = [];
  this._forensicsLogs.length = 30;
  this._log = Object.create(log);
  this._log.log = (level, string, params) => {
    this._addToForensicsLog("Experiments", string);
    log.log(level, string, params);
  };

  this._log.trace("constructor");

  // Capture the latest error, for forensics purposes.
  this._latestError = null;


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

  // The _main task handles all other actions:
  // * refreshing the manifest off the network (if _refresh)
  // * disabling/enabling experiments
  // * saving the cache (if _dirty)
  this._mainTask = null;

  // Timer for re-evaluating experiment status.
  this._timer = null;

  this._shutdown = false;
  this._networkRequest = null;

  // We need to tell when we first evaluated the experiments to fire an
  // experiments-changed notification when we only loaded completed experiments.
  this._firstEvaluate = true;

  this.init();
};

Experiments.Experiments.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback, Ci.nsIObserver]),

  /**
   * `true` if the experiments manager is currently setup (has been fully initialized
   * and not uninitialized yet).
   */
  get isReady() {
    return !this._shutdown;
  },

  init: function() {
    this._shutdown = false;
    configureLogging();

    gExperimentsEnabled = gPrefs.get(PREF_ENABLED, false) && TelemetryUtils.isTelemetryEnabled;
    this._log.trace("enabled=" + gExperimentsEnabled + ", " + this.enabled);

    gPrefs.observe(PREF_LOGGING, configureLogging);
    gPrefs.observe(PREF_MANIFEST_URI, this.updateManifest, this);
    gPrefs.observe(PREF_ENABLED, this._toggleExperimentsEnabled, this);

    gPrefsTelemetry.observe(PREF_TELEMETRY_ENABLED, this._telemetryStatusChanged, this);

    AddonManager.shutdown.addBlocker("Experiments.jsm shutdown",
      this.uninit.bind(this),
      this._getState.bind(this)
    );

    this._registerWithAddonManager();

    this._loadTask = this._loadFromCache();

    return this._loadTask.then(
      () => {
        this._log.trace("_loadTask finished ok");
        this._loadTask = null;
        return this._run();
      },
      (e) => {
        this._log.error("_loadFromCache caught error: " + e);
        this._latestError = e;
        throw e;
      }
    );
  },

  /**
   * Uninitialize this instance.
   *
   * This function is susceptible to race conditions. If it is called multiple
   * times before the previous uninit() has completed or if it is called while
   * an init() operation is being performed, the object may get in bad state
   * and/or deadlock could occur.
   *
   * @return Promise<>
   *         The promise is fulfilled when all pending tasks are finished.
   */
  uninit: Task.async(function* () {
    this._log.trace("uninit: started");
    yield this._loadTask;
    this._log.trace("uninit: finished with _loadTask");

    if (!this._shutdown) {
      this._log.trace("uninit: no previous shutdown");
      this._unregisterWithAddonManager();

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
      if (this._networkRequest) {
        try {
          this._log.trace("Aborting pending network request: " + this._networkRequest);
          this._networkRequest.abort();
        } catch (e) {
          // pass
        }
      }
      try {
        this._log.trace("uninit: waiting on _mainTask");
        yield this._mainTask;
      } catch (e) {
        // We error out of tasks after shutdown via this exception.
        this._log.trace(`uninit: caught error - ${e}`);
        if (!(e instanceof AlreadyShutdownError)) {
          this._latestError = e;
          throw e;
        }
      }
    }

    this._log.info("Completed uninitialization.");
  }),

  // Return state information, for debugging purposes.
  _getState: function() {
    let activeExperiment = this._getActiveExperiment();
    let state = {
      isShutdown: this._shutdown,
      isEnabled: gExperimentsEnabled,
      isRefresh: this._refresh,
      isDirty: this._dirty,
      isFirstEvaluate: this._firstEvaluate,
      hasLoadTask: !!this._loadTask,
      hasMainTask: !!this._mainTask,
      hasTimer: !!this._hasTimer,
      hasAddonProvider: !!gAddonProvider,
      latestLogs: this._forensicsLogs,
      experiments: this._experiments ? [...this._experiments.keys()] : null,
      terminateReason: this._terminateReason,
      activeExperiment: activeExperiment ? activeExperiment.id : null,
    };
    if (this._latestError) {
      if (typeof this._latestError == "object") {
        state.latestError = {
          message: this._latestError.message,
          stack: this._latestError.stack
        };
      } else {
        state.latestError = "" + this._latestError;
      }
    }
    return state;
  },

  _addToForensicsLog: function(what, string) {
    this._forensicsLogs.shift();
    let timeInSec = Math.floor(Services.telemetry.msSinceProcessStart() / 1000);
    this._forensicsLogs.push(`${timeInSec}: ${what} - ${string}`);
  },

  _registerWithAddonManager: function(previousExperimentsProvider) {
    this._log.trace("Registering instance with Addon Manager.");

    AddonManager.addAddonListener(this);
    AddonManager.addInstallListener(this);

    if (!gAddonProvider) {
      // The properties of this AddonType should be kept in sync with the
      // experiment AddonType registered in XPIProvider.
      this._log.trace("Registering previous experiment add-on provider.");
      gAddonProvider = previousExperimentsProvider || new Experiments.PreviousExperimentProvider(this);
      AddonManagerPrivate.registerProvider(gAddonProvider, [
          new AddonManagerPrivate.AddonType("experiment",
                                            URI_EXTENSION_STRINGS,
                                            STRING_TYPE_NAME,
                                            AddonManager.VIEW_TYPE_LIST,
                                            11000,
                                            AddonManager.TYPE_UI_HIDE_EMPTY),
      ]);
    }

  },

  _unregisterWithAddonManager: function() {
    this._log.trace("Unregistering instance with Addon Manager.");

    this._log.trace("Removing install listener from add-on manager.");
    AddonManager.removeInstallListener(this);
    this._log.trace("Removing addon listener from add-on manager.");
    AddonManager.removeAddonListener(this);
    this._log.trace("Finished unregistering with addon manager.");

    if (gAddonProvider) {
      this._log.trace("Unregistering previous experiment add-on provider.");
      AddonManagerPrivate.unregisterProvider(gAddonProvider);
      gAddonProvider = null;
    }
  },

  /*
   * Change the PreviousExperimentsProvider that this instance uses.
   * For testing only.
   */
  _setPreviousExperimentsProvider: function(provider) {
    this._unregisterWithAddonManager();
    this._registerWithAddonManager(provider);
  },

  /**
   * Throws an exception if we've already shut down.
   */
  _checkForShutdown: function() {
    if (this._shutdown) {
      throw new AlreadyShutdownError("uninit() already called");
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

  _toggleExperimentsEnabled: Task.async(function* (enabled) {
    this._log.trace("_toggleExperimentsEnabled(" + enabled + ")");
    let wasEnabled = gExperimentsEnabled;
    gExperimentsEnabled = enabled && TelemetryUtils.isTelemetryEnabled;

    if (wasEnabled == gExperimentsEnabled) {
      return;
    }

    if (gExperimentsEnabled) {
      yield this.updateManifest();
    } else {
      yield this.disableExperiment(TELEMETRY_LOG.TERMINATION.SERVICE_DISABLED);
      if (this._timer) {
        this._timer.clear();
      }
    }
  }),

  _telemetryStatusChanged: function() {
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
  getExperiments: function() {
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
          branch: experiment.branch,
        });
      }

      // Sort chronologically, descending.
      list.sort((a, b) => b.endDate - a.endDate);
      return list;
    }.bind(this));
  },

  /**
   * Returns the ExperimentInfo for the active experiment, or null
   * if there is none.
   */
  getActiveExperiment: function() {
    let experiment = this._getActiveExperiment();
    if (!experiment) {
      return null;
    }

    let info = {
      id: experiment.id,
      name: experiment._name,
      description: experiment._description,
      active: experiment.enabled,
      endDate: experiment.endDate.getTime(),
      detailURL: experiment._homepageURL,
    };

    return info;
  },

  /**
   * Experiment "branch" support. If an experiment has multiple branches, it
   * can record the branch with the experiment system and it will
   * automatically be included in data reporting (FHR/telemetry payloads).
   */

  /**
   * Set the experiment branch for the specified experiment ID.
   * @returns Promise<>
   */
  setExperimentBranch: Task.async(function*(id, branchstr) {
    yield this._loadTask;
    let e = this._experiments.get(id);
    if (!e) {
      throw new Error("Experiment not found");
    }
    e.branch = String(branchstr);
    this._log.trace("setExperimentBranch(" + id + ", " + e.branch + ") _dirty=" + this._dirty);
    this._dirty = true;
    Services.obs.notifyObservers(null, EXPERIMENTS_CHANGED_TOPIC, null);
    yield this._run();
  }),
  /**
   * Get the branch of the specified experiment. If the experiment is unknown,
   * throws an error.
   *
   * @param id The ID of the experiment. Pass null for the currently running
   *           experiment.
   * @returns Promise<string|null>
   * @throws Error if the specified experiment ID is unknown, or if there is no
   *         current experiment.
   */
  getExperimentBranch: Task.async(function*(id = null) {
    yield this._loadTask;
    let e;
    if (id) {
      e = this._experiments.get(id);
      if (!e) {
        throw new Error("Experiment not found");
      }
    } else {
      e = this._getActiveExperiment();
      if (e === null) {
        throw new Error("No active experiment");
      }
    }
    return e.branch;
  }),

  /**
   * Determine whether another date has the same UTC day as now().
   */
  _dateIsTodayUTC: function(d) {
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
  lastActiveToday: function() {
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
      this._mainTask = Task.spawn(function*() {
        try {
          yield this._main();
        } catch (e) {
          // In the CacheWriteError case we want to reschedule
          if (!(e instanceof CacheWriteError)) {
            this._log.error("_main caught error: " + e);
            return;
          }
        } finally {
          this._mainTask = null;
        }
        this._log.trace("_main finished, scheduling next run");
        try {
          yield this._scheduleNextRun();
        } catch (ex) {
          // We error out of tasks after shutdown via this exception.
          if (!(ex instanceof AlreadyShutdownError)) {
            throw ex;
          }
        }
      }.bind(this));
    }
    return this._mainTask;
  },

  _main: function*() {
    do {
      this._log.trace("_main iteration");
      yield this._loadTask;
      if (!gExperimentsEnabled) {
        this._refresh = false;
      }

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
    while (this._refresh || this._terminateReason || this._dirty);
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
  updateManifest: function() {
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

  notify: function(timer) {
    this._log.trace("notify()");
    this._checkForShutdown();
    return this._run();
  },

  // START OF ADD-ON LISTENERS

  onUninstalled: function(addon) {
    this._log.trace("onUninstalled() - addon id: " + addon.id);
    if (gActiveUninstallAddonIDs.has(addon.id)) {
      this._log.trace("matches pending uninstall");
      return;
    }
    let activeExperiment = this._getActiveExperiment();
    if (!activeExperiment || activeExperiment._addonId != addon.id) {
      return;
    }

    this.disableExperiment(TELEMETRY_LOG.TERMINATION.ADDON_UNINSTALLED);
  },

  /**
   * @returns {Boolean} returns false when we cancel the install.
   */
  onInstallStarted: function(install) {
    if (install.addon.type != "experiment") {
      return true;
    }

    this._log.trace("onInstallStarted() - " + install.addon.id);
    if (install.addon.appDisabled) {
      // This is a PreviousExperiment
      return true;
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
      return true;
    }

    if (gActiveInstallURLs.has(install.sourceURI.spec)) {
      this._log.info("onInstallStarted allowing install because install " +
                     "tracked by us.");
      return true;
    }

    this._log.warn("onInstallStarted cancelling install of unknown " +
                   "experiment add-on: " + install.addon.id);
    return false;
  },

  // END OF ADD-ON LISTENERS.

  _getExperimentByAddonId: function(addonId) {
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
  _httpGetRequest: function(url) {
    this._log.trace("httpGetRequest(" + url + ")");
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);

    this._networkRequest = xhr;
    let deferred = Promise.defer();

    let log = this._log;
    let errorhandler = (evt) => {
      log.error("httpGetRequest::onError() - Error making request to " + url + ": " + evt.type);
      deferred.reject(new Error("Experiments - XHR error for " + url + " - " + evt.type));
      this._networkRequest = null;
    };
    xhr.onerror = errorhandler;
    xhr.ontimeout = errorhandler;
    xhr.onabort = errorhandler;

    xhr.onload = (event) => {
      if (xhr.status !== 200 && xhr.state !== 0) {
        log.error("httpGetRequest::onLoad() - Request to " + url + " returned status " + xhr.status);
        deferred.reject(new Error("Experiments - XHR status for " + url + " is " + xhr.status));
        this._networkRequest = null;
        return;
      }

      deferred.resolve(xhr.responseText);
      this._networkRequest = null;
    };

    try {
      xhr.open("GET", url);

      if (xhr.channel instanceof Ci.nsISupportsPriority) {
        xhr.channel.priority = Ci.nsISupportsPriority.PRIORITY_LOWEST;
      }

      xhr.timeout = MANIFEST_FETCH_TIMEOUT_MSEC;
      xhr.send(null);
    } catch (e) {
      this._log.error("httpGetRequest() - Error opening request to " + url + ": " + e);
      return Promise.reject(new Error("Experiments - Error opening XHR for " + url));
    }
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
    this._dirty = false;
    try {
      let textData = JSON.stringify({
        version: CACHE_VERSION,
        data: [...this._experiments.values()].map(e => e.toJSON()),
      });

      let encoder = new TextEncoder();
      let data = encoder.encode(textData);
      let options = { tmpPath: path + ".tmp", compression: "lz4" };
      yield this._policy.delayCacheWrite(OS.File.writeAtomic(path, data, options));
    } catch (e) {
      // We failed to write the cache, it's still dirty.
      this._dirty = true;
      this._log.error("_saveToCache failed and caught error: " + e);
      throw new CacheWriteError();
    }

    this._log.debug("_saveToCache saved to " + path);
  },

  /*
   * Task function, load the cached experiments manifest file from disk.
   */
  _loadFromCache: Task.async(function* () {
    this._log.trace("_loadFromCache");
    let path = this._cacheFilePath;
    try {
      let result = yield loadJSONAsync(path, { compression: "lz4" });
      this._populateFromCache(result);
    } catch (e) {
      if (e instanceof OS.File.Error && e.becauseNoSuchFile) {
        // No cached manifest yet.
        this._experiments = new Map();
      } else {
        throw e;
      }
    }
  }),

  _populateFromCache: function(data) {
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

      // Discard old experiments if they ended more than 180 days ago.
      if (entry.shouldDiscard()) {
        // We discarded an experiment, the cache needs to be updated.
        this._dirty = true;
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
  _updateExperiments: function(manifestObject) {
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
      if (experiments.has(id)) {
        continue;
      }

      if (!entry.startDate || entry.shouldDiscard()) {
        this._log.trace("updateExperiments() - discarding entry for " + id);
        continue;
      }

      experiments.set(id, entry);
    }

    this._experiments = experiments;
    this._dirty = true;
  },

  getActiveExperimentID: function() {
    if (!this._experiments) {
      return null;
    }
    let e = this._getActiveExperiment();
    if (!e) {
      return null;
    }
    return e.id;
  },

  getActiveExperimentBranch: function() {
    if (!this._experiments) {
      return null;
    }
    let e = this._getActiveExperiment();
    if (!e) {
      return null;
    }
    return e.branch;
  },

  _getActiveExperiment: function() {
    let enabled = [...this._experiments.values()].filter(experiment => experiment._enabled);

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
   * Disables all active experiments.
   *
   * @return Promise<> Promise that will get resolved once the task is done or failed.
   */
  disableExperiment: function(reason) {
    if (!reason) {
      throw new Error("Must specify a termination reason.");
    }

    this._log.trace("disableExperiment()");
    this._terminateReason = reason;
    return this._run();
  },

  /**
   * The Set of add-on IDs that we know about from manifests.
   */
  get _trackedAddonIds() {
    if (!this._experiments) {
      return new Set();
    }

    return new Set([...this._experiments.values()].map(e => e._addonId));
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
    let unknownAddons = installedExperiments.filter(a => !expectedAddonIds.has(a.id));
    if (unknownAddons.length) {
      this._log.warn("_evaluateExperiments() - unknown add-ons in AddonManager: " +
                     unknownAddons.map(a => a.id).join(", "));

      yield uninstallAddons(unknownAddons);
    }

    let activeExperiment = this._getActiveExperiment();
    let activeChanged = false;

    if (!activeExperiment) {
      // Avoid this pref staying out of sync if there were e.g. crashes.
      gPrefs.set(PREF_ACTIVE_EXPERIMENT, false);
    }

    // Ensure the active experiment is in the proper state. This may install,
    // uninstall, upgrade, or enable the experiment add-on. What exactly is
    // abstracted away from us by design.
    if (activeExperiment) {
      let changes;
      let shouldStopResult = yield activeExperiment.shouldStop();
      if (shouldStopResult.shouldStop) {
        let expireReasons = ["endTime", "maxActiveSeconds"];
        let kind, reason;

        if (expireReasons.indexOf(shouldStopResult.reason[0]) != -1) {
          kind = TELEMETRY_LOG.TERMINATION.EXPIRED;
          reason = null;
        } else {
          kind = TELEMETRY_LOG.TERMINATION.RECHECK;
          reason = shouldStopResult.reason;
        }
        changes = yield activeExperiment.stop(kind, reason);
      }
      else if (this._terminateReason) {
        changes = yield activeExperiment.stop(this._terminateReason);
      }
      else {
        changes = yield activeExperiment.reconcileAddonState();
      }

      if (changes) {
        this._dirty = true;
        activeChanged = true;
      }

      if (!activeExperiment._enabled) {
        activeExperiment = null;
        activeChanged = true;
      }
    }

    this._terminateReason = null;

    if (!activeExperiment && gExperimentsEnabled) {
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
          let data = [TELEMETRY_LOG.ACTIVATION.REJECTED, id];
          data = data.concat(reason);
          const key = TELEMETRY_LOG.ACTIVATION_KEY;
          TelemetryLog.log(key, data);
          this._log.trace("evaluateExperiments() - added " + key + " to TelemetryLog: " + JSON.stringify(data));
        }

        if (!applicable) {
          continue;
        }

        this._log.debug("evaluateExperiments() - activating experiment " + id);
        try {
          yield experiment.start();
          activeChanged = true;
          activeExperiment = experiment;
          this._dirty = true;
          break;
        } catch (e) {
          // On failure, clean up the best we can and try the next experiment.
          this._log.error("evaluateExperiments() - Unable to start experiment: " + e.message);
          experiment._enabled = false;
          yield experiment.reconcileAddonState();
        }
      }
    }

    gPrefs.set(PREF_ACTIVE_EXPERIMENT, activeExperiment != null);

    if (activeChanged || this._firstEvaluate) {
      Services.obs.notifyObservers(null, EXPERIMENTS_CHANGED_TOPIC, null);
      this._firstEvaluate = false;
    }

    if ("@mozilla.org/toolkit/crash-reporter;1" in Cc && activeExperiment) {
      try {
        gCrashReporter.annotateCrashReport("ActiveExperiment", activeExperiment.id);
        gCrashReporter.annotateCrashReport("ActiveExperimentBranch", activeExperiment.branch);
      } catch (e) {
        // It's ok if crash reporting is disabled.
      }
    }
  },

  /*
   * Schedule the soonest re-check of experiment applicability that is needed.
   */
  _scheduleNextRun: function() {
    this._checkForShutdown();

    if (this._timer) {
      this._timer.clear();
    }

    if (!gExperimentsEnabled || this._experiments.length == 0) {
      return;
    }

    let time = null;
    let now = this._policy.now().getTime();
    if (this._dirty) {
      // If we failed to write the cache, we should try again periodically
      time = now + 1000 * CACHE_WRITE_RETRY_DELAY_SEC;
    }

    for (let [, experiment] of this._experiments) {
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

    this._log.trace("scheduleExperimentEvaluation() - scheduling for " + time + ", now: " + now);
    this._policy.oneshotTimer(this.notify, time - now, this, "_timer");
  },
};


/*
 * Represents a single experiment.
 */

Experiments.ExperimentEntry = function(policy) {
  this._policy = policy || new Experiments.Policy();
  let log = Log.repository.getLoggerWithMessagePrefix(
    "Browser.Experiments.Experiments",
    "ExperimentEntry #" + gExperimentEntryCounter++ + "::");
  this._log = Object.create(log);
  this._log.log = (level, string, params) => {
    if (gExperiments) {
      gExperiments._addToForensicsLog("ExperimentEntry", string);
    }
    log.log(level, string, params);
  };

  // Is the experiment supposed to be running.
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
  // The experiment branch
  this._branch = null;

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
    "_branch",
  ]),

  DATE_KEYS: new Set([
    "_startDate",
    "_endDate",
  ]),

  UPGRADE_KEYS: new Map([
    ["_branch", null],
  ]),

  ADDON_CHANGE_NONE: 0,
  ADDON_CHANGE_INSTALL: 1,
  ADDON_CHANGE_UNINSTALL: 2,
  ADDON_CHANGE_ENABLE: 4,

  /*
   * Initialize entry from the manifest.
   * @param data The experiment data from the manifest.
   * @return boolean Whether initialization succeeded.
   */
  initFromManifestData: function(data) {
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

  get branch() {
    return this._branch;
  },

  set branch(v) {
    this._branch = v;
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
  initFromCacheData: function(data) {
    for (let [key, dval] of this.UPGRADE_KEYS) {
      if (!(key in data)) {
        data[key] = dval;
      }
    }

    for (let key of this.SERIALIZE_KEYS) {
      if (!(key in data) && !this.DATE_KEYS.has(key)) {
        this._log.error("initFromCacheData() - missing required key " + key);
        return false;
      }
    }

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

    // In order for the experiment's data expiration mechanism to work, use the experiment's
    // |_endData| as the |_lastChangedDate| (if available).
    this._lastChangedDate = this._endDate ? this._endDate : this._policy.now();

    return true;
  },

  /*
   * Returns a JSON representation of this object.
   */
  toJSON: function() {
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
  updateFromManifestData: function(data) {
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
  isApplicable: function() {
    let versionCmp = Cc["@mozilla.org/xpcom/version-comparator;1"]
                              .getService(Ci.nsIVersionComparator);
    let app = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
    let runtime = Cc["@mozilla.org/xre/app-info;1"]
                    .getService(Ci.nsIXULRuntime);

    let locale = this._policy.locale();
    let channel = this._policy.updatechannel();
    let data = this._manifestData;

    let now = this._policy.now() / 1000; // The manifest times are in seconds.
    let maxActive = data.maxActiveSeconds || 0;
    let startSec = (this.startDate || 0) / 1000;

    this._log.trace("isApplicable() - now=" + now
                    + ", randomValue=" + this._randomValue);

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
        condition: () => this._startDate || !data.maxStartTime || now <= data.maxStartTime },
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
  _runFilterFunction: Task.async(function* (jsfilter) {
    this._log.trace("runFilterFunction() - filter: " + jsfilter);

    let ssm = Services.scriptSecurityManager;
    const nullPrincipal = ssm.createNullPrincipal({});
    let options = {
      sandboxName: "telemetry experiments jsfilter sandbox",
      wantComponents: false,
    };

    let sandbox = Cu.Sandbox(nullPrincipal, options);
    try {
      Cu.evalInSandbox(jsfilter, sandbox);
    } catch (e) {
      this._log.error("runFilterFunction() - failed to eval jsfilter: " + e.message);
      throw ["jsfilter-evalfailed"];
    }

    let currentEnvironment = yield TelemetryEnvironment.onInitialized();

    Object.defineProperty(sandbox, "_e",
      { get: () => Cu.cloneInto(currentEnvironment, sandbox) });

    let result = false;
    try {
      result = !!Cu.evalInSandbox("filter({get telemetryEnvironment() { return _e; } })", sandbox);
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

    return true;
  }),

  /*
   * Start running the experiment.
   *
   * @return Promise<> Resolved when the operation is complete.
   */
  start: Task.async(function* () {
    this._log.trace("start() for " + this.id);

    this._enabled = true;
    return yield this.reconcileAddonState();
  }),

  // Async install of the addon for this experiment, part of the start task above.
  _installAddon: Task.async(function* () {
    let deferred = Promise.defer();

    let hash = this._policy.ignoreHashes ? null : this._manifestData.xpiHash;

    let install = yield addonInstallForURL(this._manifestData.xpiURL, hash);
    gActiveInstallURLs.add(install.sourceURI.spec);

    let failureHandler = (failureInstall, handler) => {
      let message = "AddonInstall " + handler + " for " + this.id + ", state=" +
                   (failureInstall.state || "?") + ", error=" + failureInstall.error;
      this._log.error("_installAddon() - " + message);
      this._failedStart = true;
      gActiveInstallURLs.delete(failureInstall.sourceURI.spec);

      TelemetryLog.log(TELEMETRY_LOG.ACTIVATION_KEY,
                      [TELEMETRY_LOG.ACTIVATION.INSTALL_FAILURE, this.id]);

      deferred.reject(new Error(message));
    };

    let listener = {
      _expectedID: null,

      onDownloadEnded: downloadEndedInstall => {
        this._log.trace("_installAddon() - onDownloadEnded for " + this.id);

        if (downloadEndedInstall.existingAddon) {
          this._log.warn("_installAddon() - onDownloadEnded, addon already installed");
        }

        if (downloadEndedInstall.addon.type !== "experiment") {
          this._log.error("_installAddon() - onDownloadEnded, wrong addon type");
          downloadEndedInstall.cancel();
        }
      },

      onInstallStarted: installStartedInstall => {
        this._log.trace("_installAddon() - onInstallStarted for " + this.id);

        if (installStartedInstall.existingAddon) {
          this._log.warn("_installAddon() - onInstallStarted, addon already installed");
        }

        if (installStartedInstall.addon.type !== "experiment") {
          this._log.error("_installAddon() - onInstallStarted, wrong addon type");
          return false;
        }
        return undefined;
      },

      onInstallEnded: installEndedInstall => {
        this._log.trace("_installAddon() - install ended for " + this.id);
        gActiveInstallURLs.delete(installEndedInstall.sourceURI.spec);

        this._lastChangedDate = this._policy.now();
        this._startDate = this._policy.now();
        this._enabled = true;

        TelemetryLog.log(TELEMETRY_LOG.ACTIVATION_KEY,
                       [TELEMETRY_LOG.ACTIVATION.ACTIVATED, this.id]);

        let addon = installEndedInstall.addon;
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
        listener[what] = eventInstall => failureHandler(eventInstall, what)
      });

    install.addListener(listener);
    install.install();

    return yield deferred.promise;
  }),

  /**
   * Stop running the experiment if it is active.
   *
   * @param terminationKind (optional)
   *        The termination kind, e.g. ADDON_UNINSTALLED or EXPIRED.
   * @param terminationReason (optional)
   *        The termination reason details for termination kind RECHECK.
   * @return Promise<> Resolved when the operation is complete.
   */
  stop: Task.async(function* (terminationKind, terminationReason) {
    this._log.trace("stop() - id=" + this.id + ", terminationKind=" + terminationKind);
    if (!this._enabled) {
      throw new Error("Must not call stop() on an inactive experiment.");
    }

    this._enabled = false;
    let now = this._policy.now();
    this._lastChangedDate = now;
    this._endDate = now;

    let changes = yield this.reconcileAddonState();
    this._logTermination(terminationKind, terminationReason);

    if (terminationKind == TELEMETRY_LOG.TERMINATION.ADDON_UNINSTALLED) {
      changes |= this.ADDON_CHANGE_UNINSTALL;
    }

    return changes;
  }),

  /**
   * Reconcile the state of the add-on against what it's supposed to be.
   *
   * If we are active, ensure the add-on is enabled and up to date.
   *
   * If we are inactive, ensure the add-on is not installed.
   */
  reconcileAddonState: Task.async(function* () {
    this._log.trace("reconcileAddonState()");

    if (!this._enabled) {
      if (!this._addonId) {
        this._log.trace("reconcileAddonState() - Experiment is not enabled and " +
                        "has no add-on. Doing nothing.");
        return this.ADDON_CHANGE_NONE;
      }

      let addon = yield this._getAddon();
      if (!addon) {
        this._log.trace("reconcileAddonState() - Inactive experiment has no " +
                        "add-on. Doing nothing.");
        return this.ADDON_CHANGE_NONE;
      }

      this._log.info("reconcileAddonState() - Uninstalling add-on for inactive " +
                     "experiment: " + addon.id);
      gActiveUninstallAddonIDs.add(addon.id);
      yield uninstallAddons([addon]);
      gActiveUninstallAddonIDs.delete(addon.id);
      return this.ADDON_CHANGE_UNINSTALL;
    }

    // If we get here, we're supposed to be active.

    let changes = 0;

    // That requires an add-on.
    let currentAddon = yield this._getAddon();

    // If we have an add-on but it isn't up to date, uninstall it
    // (to prepare for reinstall).
    if (currentAddon && this._needsUpdate) {
      this._log.info("reconcileAddonState() - Uninstalling add-on because update " +
                     "needed: " + currentAddon.id);
      gActiveUninstallAddonIDs.add(currentAddon.id);
      yield uninstallAddons([currentAddon]);
      gActiveUninstallAddonIDs.delete(currentAddon.id);
      changes |= this.ADDON_CHANGE_UNINSTALL;
    }

    if (!currentAddon || this._needsUpdate) {
      this._log.info("reconcileAddonState() - Installing add-on.");
      yield this._installAddon();
      changes |= this.ADDON_CHANGE_INSTALL;
    }

    let addon = yield this._getAddon();
    if (!addon) {
      throw new Error("Could not obtain add-on for experiment that should be " +
                      "enabled.");
    }

    // If we have the add-on and it is enabled, we are done.
    if (!addon.userDisabled) {
      return changes;
    }

    // Check permissions to see if we can enable the addon.
    if (!(addon.permissions & AddonManager.PERM_CAN_ENABLE)) {
      throw new Error("Don't have permission to enable addon " + addon.id + ", perm=" + addon.permission);
    }

    // Experiment addons should not require a restart.
    if (addon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_ENABLE) {
      throw new Error("Experiment addon requires a restart: " + addon.id);
    }

    let deferred = Promise.defer();

    // Else we need to enable it.
    let listener = {
      onEnabled: enabledAddon => {
        if (enabledAddon.id != addon.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        deferred.resolve();
      },
    };

    for (let handler of ["onDisabled", "onOperationCancelled", "onUninstalled"]) {
      listener[handler] = (evtAddon) => {
        if (evtAddon.id != addon.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        deferred.reject("Failed to enable addon " + addon.id + " due to: " + handler);
      };
    }

    this._log.info("reconcileAddonState() - Activating add-on: " + addon.id);
    AddonManager.addAddonListener(listener);
    addon.userDisabled = false;
    yield deferred.promise;
    changes |= this.ADDON_CHANGE_ENABLE;

    this._log.info("reconcileAddonState() - Add-on has been enabled: " + addon.id);
    return changes;
   }),

  /**
   * Obtain the underlying Addon from the Addon Manager.
   *
   * @return Promise<Addon|null>
   */
  _getAddon: function() {
    if (!this._addonId) {
      return Promise.resolve(null);
    }

    let deferred = Promise.defer();

    AddonManager.getAddonByID(this._addonId, (addon) => {
      if (addon && addon.appDisabled) {
        // Don't return PreviousExperiments.
        addon = null;
      }

      deferred.resolve(addon);
    });

    return deferred.promise;
  },

  _logTermination: function(terminationKind, terminationReason) {
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

  /**
   * Determine whether an active experiment should be stopped.
   */
  shouldStop: function() {
    if (!this._enabled) {
      throw new Error("shouldStop must not be called on disabled experiments.");
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
  shouldDiscard: function() {
    let limit = this._policy.now();
    limit.setDate(limit.getDate() - KEEP_HISTORY_N_DAYS);
    return (this._lastChangedDate < limit);
  },

  /*
   * Get next date (in epoch-ms) to schedule a re-evaluation for this.
   * Returns 0 if it doesn't need one.
   */
  getScheduleTime: function() {
    if (this._enabled) {
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
  _isManifestDataValid: function(data) {
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
var stripDateToMidnight = function(d) {
  let m = new Date(d);
  m.setUTCHours(0, 0, 0, 0);

  return m;
};

/**
 * An Add-ons Manager provider that knows about old experiments.
 *
 * This provider exposes read-only add-ons corresponding to previously-active
 * experiments. The existence of this provider (and the add-ons it knows about)
 * facilitates the display of old experiments in the Add-ons Manager UI with
 * very little custom code in that component.
 */
this.Experiments.PreviousExperimentProvider = function(experiments) {
  this._experiments = experiments;
  this._experimentList = [];
  this._log = Log.repository.getLoggerWithMessagePrefix(
    "Browser.Experiments.Experiments",
    "PreviousExperimentProvider #" + gPreviousProviderCounter++ + "::");
}

this.Experiments.PreviousExperimentProvider.prototype = Object.freeze({
  name: "PreviousExperimentProvider",

  startup: function() {
    this._log.trace("startup()");
    Services.obs.addObserver(this, EXPERIMENTS_CHANGED_TOPIC, false);
  },

  shutdown: function() {
    this._log.trace("shutdown()");
    try {
      Services.obs.removeObserver(this, EXPERIMENTS_CHANGED_TOPIC);
    } catch (e) {
      // Prevent crash in mochitest-browser3 on Mulet
    }
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case EXPERIMENTS_CHANGED_TOPIC:
        this._updateExperimentList();
        break;
    }
  },

  getAddonByID: function(id, cb) {
    for (let experiment of this._experimentList) {
      if (experiment.id == id) {
        cb(new PreviousExperimentAddon(experiment));
        return;
      }
    }

    cb(null);
  },

  getAddonsByTypes: function(types, cb) {
    if (types && types.length > 0 && types.indexOf("experiment") == -1) {
      cb([]);
      return;
    }

    cb(this._experimentList.map(e => new PreviousExperimentAddon(e)));
  },

  _updateExperimentList: function() {
    return this._experiments.getExperiments().then((experiments) => {
      let list = experiments.filter(e => !e.active);

      let newMap = new Map(list.map(e => [e.id, e]));
      let oldMap = new Map(this._experimentList.map(e => [e.id, e]));

      let added = [...newMap.keys()].filter(id => !oldMap.has(id));
      let removed = [...oldMap.keys()].filter(id => !newMap.has(id));

      for (let id of added) {
        this._log.trace("updateExperimentList() - adding " + id);
        let wrapper = new PreviousExperimentAddon(newMap.get(id));
        AddonManagerPrivate.callInstallListeners("onExternalInstall", null, wrapper, null, false);
        AddonManagerPrivate.callAddonListeners("onInstalling", wrapper, false);
      }

      for (let id of removed) {
        this._log.trace("updateExperimentList() - removing " + id);
        let wrapper = new PreviousExperimentAddon(oldMap.get(id));
        AddonManagerPrivate.callAddonListeners("onUninstalling", wrapper, false);
      }

      this._experimentList = list;

      for (let id of added) {
        let wrapper = new PreviousExperimentAddon(newMap.get(id));
        AddonManagerPrivate.callAddonListeners("onInstalled", wrapper);
      }

      for (let id of removed) {
        let wrapper = new PreviousExperimentAddon(oldMap.get(id));
        AddonManagerPrivate.callAddonListeners("onUninstalled", wrapper);
      }

      return this._experimentList;
    });
  },
});

/**
 * An add-on that represents a previously-installed experiment.
 */
function PreviousExperimentAddon(experiment) {
  this._id = experiment.id;
  this._name = experiment.name;
  this._endDate = experiment.endDate;
  this._description = experiment.description;
}

PreviousExperimentAddon.prototype = Object.freeze({
  // BEGIN REQUIRED ADDON PROPERTIES

  get appDisabled() {
    return true;
  },

  get blocklistState() {
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED
  },

  get creator() {
    return new AddonManagerPrivate.AddonAuthor("");
  },

  get foreignInstall() {
    return false;
  },

  get id() {
    return this._id;
  },

  get isActive() {
    return false;
  },

  get isCompatible() {
    return true;
  },

  get isPlatformCompatible() {
    return true;
  },

  get name() {
    return this._name;
  },

  get pendingOperations() {
    return AddonManager.PENDING_NONE;
  },

  get permissions() {
    return 0;
  },

  get providesUpdatesSecurely() {
    return true;
  },

  get scope() {
    return AddonManager.SCOPE_PROFILE;
  },

  get type() {
    return "experiment";
  },

  get userDisabled() {
    return true;
  },

  get version() {
    return null;
  },

  // END REQUIRED PROPERTIES

  // BEGIN OPTIONAL PROPERTIES

  get description() {
    return this._description;
  },

  get updateDate() {
    return new Date(this._endDate);
  },

  // END OPTIONAL PROPERTIES

  // BEGIN REQUIRED METHODS

  isCompatibleWith: function(appVersion, platformVersion) {
    return true;
  },

  findUpdates: function(listener, reason, appVersion, platformVersion) {
    AddonManagerPrivate.callNoUpdateListeners(this, listener, reason,
                                              appVersion, platformVersion);
  },

  // END REQUIRED METHODS

  /**
   * The end-date of the experiment, required for the Addon Manager UI.
   */

   get endDate() {
     return this._endDate;
   },

});
