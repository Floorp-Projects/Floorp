/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm");
loader.lazyRequireGetter(this, "ConnectionManager", "devtools/client/connection-manager", true);
loader.lazyRequireGetter(this, "AddonSimulatorProcess", "devtools/webide/simulator-process", true);
loader.lazyRequireGetter(this, "OldAddonSimulatorProcess", "devtools/webide/simulator-process", true);
loader.lazyRequireGetter(this, "CustomSimulatorProcess", "devtools/webide/simulator-process", true);
const EventEmitter = require("devtools/toolkit/event-emitter");
const promise = require("promise");

const SimulatorRegExp = new RegExp(Services.prefs.getCharPref("devtools.webide.simulatorAddonRegExp"));
const LocaleCompare = (a, b) => {
  return a.name.toLowerCase().localeCompare(b.name.toLowerCase());
};

let Simulators = {

  // The list of simulator configurations.
  _simulators: [],

  // List all available simulators.
  findSimulators() {
    if (this._loaded) {
      return promise.resolve(this._simulators);
    }

    // TODO (Bug 1146519) Load a persistent list of configured simulators first.

    // Add default simulators to the list for each new (unused) addon.
    return this.findSimulatorAddons().then(addons => {
      this._loaded = true;
      addons.forEach(this.addIfUnusedAddon.bind(this));
      return this._simulators;
    });
  },

  // List all installed simulator addons.
  findSimulatorAddons() {
    let deferred = promise.defer();
    AddonManager.getAllAddons(all => {
      let addons = [];
      for (let addon of all) {
        if (this.isSimulatorAddon(addon)) {
          addons.push(addon);
        }
      }
      // Sort simulator addons by name.
      addons.sort(LocaleCompare);
      deferred.resolve(addons);
    });
    return deferred.promise;
  },

  // Detect simulator addons, including "unofficial" ones
  isSimulatorAddon(addon) {
    return SimulatorRegExp.exec(addon.id);
  },

  // Get a unique name for a simulator (may add a suffix, e.g. "MyName (1)").
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

  // Add a new simulator to the list. Caution: `simulator.name` may be modified.
  // @return Promise to added simulator.
  add(simulator) {
    let simulators = this._simulators;
    let uniqueName = this.uniqueName(simulator.options.name);
    simulator.options.name = uniqueName;
    simulators.push(simulator);
    this.emitUpdated();
    return promise.resolve(simulator);
  },

  remove(simulator) {
    let simulators = this._simulators;
    let remaining = simulators.filter(s => s !== simulator);
    this._simulators = remaining;
    if (remaining.length !== simulators.length) {
      this.emitUpdated();
    }
  },

  // Add a new default simulator for `addon` if no other simulator uses it.
  addIfUnusedAddon(addon) {
    let simulators = this._simulators;
    let matching = simulators.filter(s => s.addon && s.addon.id == addon.id);
    if (matching.length > 0) {
      return promise.resolve();
    }
    let name = addon.name.replace(" Simulator", "");
    return this.add(new Simulator({name}, addon));
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

  emitUpdated() {
    this._simulators.sort(LocaleCompare);
    this.emit("updated");
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
