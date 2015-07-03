/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm");
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
loader.lazyRequireGetter(this, "ConnectionManager", "devtools/client/connection-manager", true);
loader.lazyRequireGetter(this, "AddonSimulatorProcess", "devtools/webide/simulator-process", true);
loader.lazyRequireGetter(this, "OldAddonSimulatorProcess", "devtools/webide/simulator-process", true);
loader.lazyRequireGetter(this, "CustomSimulatorProcess", "devtools/webide/simulator-process", true);
const asyncStorage = require("devtools/toolkit/shared/async-storage");
const EventEmitter = require("devtools/toolkit/event-emitter");
const promise = require("promise");

const SimulatorRegExp = new RegExp(Services.prefs.getCharPref("devtools.webide.simulatorAddonRegExp"));
const LocaleCompare = (a, b) => {
  return a.name.toLowerCase().localeCompare(b.name.toLowerCase());
};

let Simulators = {

  // The list of simulator configurations.
  _simulators: [],

  /**
   * Load a previously saved list of configurations (only once).
   *
   * @return Promise.
   */
  _load() {
    if (this._loadingPromise) {
      return this._loadingPromise;
    }

    this._loadingPromise = Task.spawn(function*() {
      let jobs = [];

      let value = yield asyncStorage.getItem("simulators");
      if (Array.isArray(value)) {
        value.forEach(options => {
          let simulator = new Simulator(options);
          Simulators.add(simulator, true);

          // If the simulator had a reference to an addon, fix it.
          if (options.addonID) {
            let job = promise.defer();
            AddonManager.getAddonByID(options.addonID, addon => {
              simulator.addon = addon;
              delete simulator.options.addonID;
              job.resolve();
            });
            jobs.push(job);
          }
        });
      }

      yield promise.all(jobs);
      yield Simulators._addUnusedAddons();
      Simulators.emitUpdated();
      return Simulators._simulators;
    });

    return this._loadingPromise;
  },

  /**
   * Add default simulators to the list for each new (unused) addon.
   *
   * @return Promise.
   */
  _addUnusedAddons: Task.async(function*() {
    let jobs = [];

    let addons = yield Simulators.findSimulatorAddons();
    addons.forEach(addon => {
      jobs.push(Simulators.addIfUnusedAddon(addon, true));
    });

    yield promise.all(jobs);
  }),

  /**
   * Save the current list of configurations.
   *
   * @return Promise.
   */
  _save: Task.async(function*() {
    yield this._load();

    let value = Simulators._simulators.map(simulator => {
      let options = JSON.parse(JSON.stringify(simulator.options));
      if (simulator.addon != null) {
        options.addonID = simulator.addon.id;
      }
      return options;
    });

    yield asyncStorage.setItem("simulators", value);
  }),

  /**
   * List all available simulators.
   *
   * @return Promised simulator list.
   */
  findSimulators: Task.async(function*() {
    yield this._load();
    return Simulators._simulators;
  }),

  /**
   * List all installed simulator addons.
   *
   * @return Promised addon list.
   */
  findSimulatorAddons() {
    let deferred = promise.defer();
    AddonManager.getAllAddons(all => {
      let addons = [];
      for (let addon of all) {
        if (Simulators.isSimulatorAddon(addon)) {
          addons.push(addon);
        }
      }
      // Sort simulator addons by name.
      addons.sort(LocaleCompare);
      deferred.resolve(addons);
    });
    return deferred.promise;
  },

  /**
   * Add a new simulator for `addon` if no other simulator uses it.
   */
  addIfUnusedAddon(addon, silently = false) {
    let simulators = this._simulators;
    let matching = simulators.filter(s => s.addon && s.addon.id == addon.id);
    if (matching.length > 0) {
      return promise.resolve();
    }
    let name = addon.name.replace(" Simulator", "");
    return this.add(new Simulator({name}, addon), silently);
  },

  // TODO (Bug 1146521) Maybe find a better way to deal with removed addons?
  removeIfUsingAddon(addon) {
    let simulators = this._simulators;
    let remaining = simulators.filter(s => !s.addon || s.addon.id != addon.id);
    this._simulators = remaining;
    if (remaining.length !== simulators.length) {
      this.emitUpdated();
    }
  },

  /**
   * Add a new simulator to the list. Caution: `simulator.name` may be modified.
   *
   * @return Promise to added simulator.
   */
  add(simulator, silently = false) {
    let simulators = this._simulators;
    let uniqueName = this.uniqueName(simulator.options.name);
    simulator.options.name = uniqueName;
    simulators.push(simulator);
    if (!silently) {
      this.emitUpdated();
    }
    return promise.resolve(simulator);
  },

  /**
   * Remove a simulator from the list.
   */
  remove(simulator) {
    let simulators = this._simulators;
    let remaining = simulators.filter(s => s !== simulator);
    this._simulators = remaining;
    if (remaining.length !== simulators.length) {
      this.emitUpdated();
    }
  },

  /**
   * Get a unique name for a simulator (may add a suffix, e.g. "MyName (1)").
   */
  uniqueName(name) {
    let simulators = this._simulators;

    let names = {};
    simulators.forEach(simulator => names[simulator.name] = true);

    // Strip any previous suffix, add a new suffix if necessary.
    let stripped = name.replace(/ \(\d+\)$/, "");
    let unique = stripped;
    for (let i = 1; names[unique]; i++) {
      unique = stripped + " (" + i + ")";
    }
    return unique;
  },

  /**
   * Detect simulator addons, including "unofficial" ones.
   */
  isSimulatorAddon(addon) {
    return SimulatorRegExp.exec(addon.id);
  },

  emitUpdated() {
    this.emit("updated");
    this._simulators.sort(LocaleCompare);
    this._save();
  },

  onConfigure(e, simulator) {
    this._lastConfiguredSimulator = simulator;
  },

  onInstalled(addon) {
    if (this.isSimulatorAddon(addon)) {
      this.addIfUnusedAddon(addon);
    }
  },

  onEnabled(addon) {
    if (this.isSimulatorAddon(addon)) {
      this.addIfUnusedAddon(addon);
    }
  },

  onDisabled(addon) {
    if (this.isSimulatorAddon(addon)) {
      this.removeIfUsingAddon(addon);
    }
  },

  onUninstalled(addon) {
    if (this.isSimulatorAddon(addon)) {
      this.removeIfUsingAddon(addon);
    }
  },
};
exports.Simulators = Simulators;
AddonManager.addAddonListener(Simulators);
EventEmitter.decorate(Simulators);
Simulators.on("configure", Simulators.onConfigure.bind(Simulators));

function Simulator(options = {}, addon = null) {
  this.addon = addon;
  this.options = options;

  // Fill `this.options` with default values where needed.
  let defaults = this._defaults;
  for (let option in defaults) {
    if (this.options[option] == null) {
      this.options[option] = defaults[option];
    }
  }
}
Simulator.prototype = {

  // Default simulation options, based on the Firefox OS Flame.
  _defaults: {
    width: 320,
    height: 570,
    pixelRatio: 1.5
  },

  restoreDefaults() {
    let options = this.options;
    let defaults = this._defaults;
    for (let option in defaults) {
      options[option] = defaults[option];
    }
  },

  launch() {
    // Close already opened simulation.
    if (this.process) {
      return this.kill().then(this.launch.bind(this));
    }

    this.options.port = ConnectionManager.getFreeTCPPort();

    // Choose simulator process type.
    if (this.options.b2gBinary) {
      // Custom binary.
      this.process = new CustomSimulatorProcess(this.options);
    } else if (this.version > "1.3") {
      // Recent simulator addon.
      this.process = new AddonSimulatorProcess(this.addon, this.options);
    } else {
      // Old simulator addon.
      this.process = new OldAddonSimulatorProcess(this.addon, this.options);
    }
    this.process.run();

    return promise.resolve(this.options.port);
  },

  kill() {
    let process = this.process;
    if (!process) {
      return promise.resolve();
    }
    this.process = null;
    return process.kill();
  },

  get id() {
    return this.name;
  },

  get name() {
    return this.options.name;
  },

  get version() {
    return this.options.b2gBinary ? "Custom" : this.addon.name.match(/\d+\.\d+/)[0];
  },
};
exports.Simulator = Simulator;
