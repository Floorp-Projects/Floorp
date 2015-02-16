/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

'use strict';

const { Cc, Ci, Cu } = require("chrome");

const Environment = require("sdk/system/environment").env;
const Subprocess = require("sdk/system/child_process/subprocess");
const { EventEmitter } = Cu.import("resource://gre/modules/devtools/event-emitter.js", {});
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

let platform = Services.appShell.hiddenDOMWindow.navigator.platform;
let OS = "";
if (platform.indexOf("Win") != -1) {
  OS = "win32";
} else if (platform.indexOf("Mac") != -1) {
  OS = "mac64";
} else if (platform.indexOf("Linux") != -1) {
  if (platform.indexOf("x86_64") != -1) {
    OS = "linux64";
  } else {
    OS = "linux32";
  }
}

function SimulatorProcess() {}
SimulatorProcess.prototype = {

  // Check if B2G is running.
  get isRunning() !!this.process,

  // Start the process and connect the debugger client.
  run() {

    // Resolve B2G binary.
    let b2g = this.b2gBinary;
    if (!b2g || !b2g.exists()) {
      throw Error("B2G executable not found.");
    }

    this.once("stdout", function () {
      if (OS == "mac64") {
        console.debug("WORKAROUND run osascript to show b2g-desktop window on OS=='mac64'");
        // Escape double quotes and escape characters for use in AppleScript.
        let path = b2g.path.replace(/\\/g, "\\\\").replace(/\"/g, '\\"');

        Subprocess.call({
          command: "/usr/bin/osascript",
          arguments: ["-e", 'tell application "' + path + '" to activate'],
        });
      }
    });

    this.on("stdout", (e, data) => this.log(e, data.trim()));
    this.on("stderr", (e, data) => this.log(e, data.trim()));

    let environment;
    if (OS.indexOf("linux") > -1) {
      environment = ["TMPDIR=" + Services.dirsvc.get("TmpD", Ci.nsIFile).path];
      if ("DISPLAY" in Environment) {
        environment.push("DISPLAY=" + Environment.DISPLAY);
      }
    }

    // Spawn a B2G instance.
    this.process = Subprocess.call({
      command: b2g,
      arguments: this.args,
      environment: environment,
      stdout: data => this.emit("stdout", data),
      stderr: data => this.emit("stderr", data),
      // On B2G instance exit, reset tracked process, remote debugger port and
      // shuttingDown flag, then finally emit an exit event.
      done: result => {
        console.log("B2G terminated with " + result.exitCode);
        this.process = null;
        this.emit("exit", result.exitCode);
      }
    });
  },

  // Request a B2G instance kill.
  kill() {
    let deferred = promise.defer();
    if (this.process) {
      this.once("exit", (e, exitCode) => {
        this.shuttingDown = false;
        deferred.resolve(exitCode);
      });
      if (!this.shuttingDown) {
        this.shuttingDown = true;
        this.emit("kill", null);
        this.process.kill();
      }
      return deferred.promise;
    } else {
      return promise.resolve(undefined);
    }
  },

  // Maybe log output messages.
  log(level, message) {
    if (!Services.prefs.getBoolPref("devtools.webide.logSimulatorOutput")) {
      return;
    }
    if (level === "stderr" || level === "error") {
      console.error(message);
      return;
    }
    console.log(message);
  },

  // Compute B2G CLI arguments.
  get args() {
    let args = [];

    let gaia = this.gaiaProfile;
    if (!gaia || !gaia.exists()) {
      throw Error("Gaia profile directory not found.");
    }
    args.push("-profile", gaia.path);

    args.push("-start-debugger-server", "" + this.options.port);

    // Ignore eventual zombie instances of b2g that are left over.
    args.push("-no-remote");

    return args;
  },
};

EventEmitter.decorate(SimulatorProcess.prototype);


function CustomSimulatorProcess(options) {
  this.options = options;
}

let CSPp = CustomSimulatorProcess.prototype = Object.create(SimulatorProcess.prototype);

// Compute B2G binary file handle.
Object.defineProperty(CSPp, "b2gBinary", {
  get: function() {
    let file = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsILocalFile);
    file.initWithPath(this.options.b2gBinary);
    return file;
  }
});

// Compute Gaia profile file handle.
Object.defineProperty(CSPp, "gaiaProfile", {
  get: function() {
    let file = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsILocalFile);
    file.initWithPath(this.options.gaiaProfile);
    return file;
  }
});

exports.CustomSimulatorProcess = CustomSimulatorProcess;


function AddonSimulatorProcess(addon, options) {
  this.addon = addon;
  this.options = options;
}

let ASPp = AddonSimulatorProcess.prototype = Object.create(SimulatorProcess.prototype);

// Compute B2G binary file handle.
Object.defineProperty(ASPp, "b2gBinary", {
  get: function() {
    let file;
    try {
      let pref = "extensions." + this.addon.id + ".customRuntime";
      file = Services.prefs.getComplexValue(pref, Ci.nsIFile);
    } catch(e) {}

    if (!file) {
      file = this.addon.getResourceURI().QueryInterface(Ci.nsIFileURL).file;
      file.append("b2g");
      let binaries = {
        win32: "b2g-bin.exe",
        mac64: "B2G.app/Contents/MacOS/b2g-bin",
        linux32: "b2g-bin",
        linux64: "b2g-bin",
      };
      binaries[OS].split("/").forEach(node => file.append(node));
    }
    return file;
  }
});

// Compute Gaia profile file handle.
Object.defineProperty(ASPp, "gaiaProfile", {
  get: function() {
    let file;
    try {
      let pref = "extensions." + this.addon.id + ".gaiaProfile";
      file = Services.prefs.getComplexValue(pref, Ci.nsIFile);
    } catch(e) {}

    if (!file) {
      file = this.addon.getResourceURI().QueryInterface(Ci.nsIFileURL).file;
      file.append("profile");
    }
    return file;
  }
});

exports.AddonSimulatorProcess = AddonSimulatorProcess;


function OldAddonSimulatorProcess(addon, options) {
  this.addon = addon;
  this.options = options;
}

let OASPp = OldAddonSimulatorProcess.prototype = Object.create(AddonSimulatorProcess.prototype);

// Compute B2G binary file handle.
Object.defineProperty(OASPp, "b2gBinary", {
  get: function() {
    let file;
    try {
      let pref = "extensions." + this.addon.id + ".customRuntime";
      file = Services.prefs.getComplexValue(pref, Ci.nsIFile);
    } catch(e) {}

    if (!file) {
      file = this.addon.getResourceURI().QueryInterface(Ci.nsIFileURL).file;
      let version = this.addon.name.match(/\d+\.\d+/)[0].replace(/\./, "_");
      file.append("resources");
      file.append("fxos_" + version + "_simulator");
      file.append("data");
      file.append(OS == "linux32" ? "linux" : OS);
      let binaries = {
        win32: "b2g/b2g-bin.exe",
        mac64: "B2G.app/Contents/MacOS/b2g-bin",
        linux32: "b2g/b2g-bin",
        linux64: "b2g/b2g-bin",
      };
      binaries[OS].split("/").forEach(node => file.append(node));
    }
    return file;
  }
});

// Compute B2G CLI arguments.
Object.defineProperty(OASPp, "args", {
  get: function() {
    let args = [];

    let gaia = this.gaiaProfile;
    if (!gaia || !gaia.exists()) {
      throw Error("Gaia profile directory not found.");
    }
    args.push("-profile", gaia.path);

    args.push("-dbgport", "" + this.options.port);

    // Ignore eventual zombie instances of b2g that are left over.
    args.push("-no-remote");

    return args;
  }
});

exports.OldAddonSimulatorProcess = OldAddonSimulatorProcess;
