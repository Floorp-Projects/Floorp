/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm");
const { EventEmitter } = Cu.import("resource://gre/modules/devtools/event-emitter.js");
const { ConnectionManager } = require("devtools/client/connection-manager");
const { AddonSimulatorProcess, OldAddonSimulatorProcess } = require("devtools/webide/simulator-process");
const promise = require("promise");

const SimulatorRegExp = new RegExp(Services.prefs.getCharPref("devtools.webide.simulatorAddonRegExp"));

let Simulators = {
  // TODO (Bug 1090949) Don't generate this list from installed simulator
  // addons, but instead implement a persistent list of user-configured
  // simulators.
  getAll() {
    let deferred = promise.defer();
    AddonManager.getAllAddons(addons => {
      let simulators = [];
      for (let addon of addons) {
        if (SimulatorRegExp.exec(addon.id)) {
          simulators.push(new Simulator(addon));
        }
      }
      // Sort simulators alphabetically by name.
      simulators.sort((a, b) => {
        return a.name.toLowerCase().localeCompare(b.name.toLowerCase())
      });
      deferred.resolve(simulators);
    });
    return deferred.promise;
  },
}
EventEmitter.decorate(Simulators);
exports.Simulators = Simulators;

function update() {
  Simulators.emit("updated");
}
AddonManager.addAddonListener({
  onEnabled: update,
  onDisabled: update,
  onInstalled: update,
  onUninstalled: update
});


function Simulator(addon) {
  this.addon = addon;
}

Simulator.prototype = {
  launch() {
    // Close already opened simulation.
    if (this.process) {
      return this.kill().then(this.launch.bind(this));
    }

    let options = {
      port: ConnectionManager.getFreeTCPPort()
    };

    if (this.version <= "1.3") {
      // Support older simulator addons.
      this.process = new OldAddonSimulatorProcess(this.addon, options);
    } else {
      this.process = new AddonSimulatorProcess(this.addon, options);
    }
    this.process.run();

    return promise.resolve(options.port);
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
    return this.addon.id;
  },

  get name() {
    return this.addon.name.replace(" Simulator", "");
  },

  get version() {
    return this.name.match(/\d+\.\d+/)[0];
  },
};
