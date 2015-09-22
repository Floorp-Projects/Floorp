/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

'use strict';

const { Cc, Ci, Cu } = require("chrome");

const Environment = require("sdk/system/environment").env;
const EventEmitter = require("devtools/shared/event-emitter");
const promise = require("promise");
const Subprocess = require("sdk/system/child_process/subprocess");
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

loader.lazyGetter(this, "OS", () => {
  const Runtime = require("sdk/system/runtime");
  switch (Runtime.OS) {
    case "Darwin":
      return "mac64";
    case "Linux":
      if (Runtime.XPCOMABI.indexOf("x86_64") === 0) {
        return "linux64";
      } else {
        return "linux32";
      }
    case "WINNT":
      return "win32";
    default:
      return "";
  }
});

function SimulatorProcess() {}
SimulatorProcess.prototype = {

  // Check if B2G is running.
  get isRunning() {
    return !!this.process;
  },

  // Start the process and connect the debugger client.
  run() {

    // Resolve B2G binary.
    let b2g = this.b2gBinary;
    if (!b2g || !b2g.exists()) {
      throw Error("B2G executable not found.");
    }

    // Ensure Gaia profile exists.
    let gaia = this.gaiaProfile;
    if (!gaia || !gaia.exists()) {
      throw Error("Gaia profile directory not found.");
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

    let logHandler = (e, data) => this.log(e, data.trim());
    this.on("stdout", logHandler);
    this.on("stderr", logHandler);
    this.once("exit", () => {
      this.off("stdout", logHandler);
      this.off("stderr", logHandler);
    });

    let environment;
    if (OS.indexOf("linux") > -1) {
      environment = ["TMPDIR=" + Services.dirsvc.get("TmpD", Ci.nsIFile).path];
      ["DISPLAY", "XAUTHORITY"].forEach(key => {
        if (key in Environment) {
          environment.push(key + "=" + Environment[key]);
        }
      });
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

    // Gaia profile.
    args.push("-profile", this.gaiaProfile.path);

    // Debugger server port.
    let port = parseInt(this.options.port);
    args.push("-start-debugger-server", "" + port);

    // Screen size.
    let width = parseInt(this.options.width);
    let height = parseInt(this.options.height);
    if (width && height) {
      args.push("-screen", width + "x" + height);
    }

    // Ignore eventual zombie instances of b2g that are left over.
    args.push("-no-remote");

    return args;
  },
};

EventEmitter.decorate(SimulatorProcess.prototype);


function CustomSimulatorProcess(options) {
  this.options = options;
}

var CSPp = CustomSimulatorProcess.prototype = Object.create(SimulatorProcess.prototype);

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

var ASPp = AddonSimulatorProcess.prototype = Object.create(SimulatorProcess.prototype);

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

    // Custom profile from simulator configuration.
    if (this.options.gaiaProfile) {
      file = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsILocalFile);
      file.initWithPath(this.options.gaiaProfile);
      return file;
    }

    // Custom profile from addon prefs.
    try {
      let pref = "extensions." + this.addon.id + ".gaiaProfile";
      file = Services.prefs.getComplexValue(pref, Ci.nsIFile);
      return file;
    } catch(e) {}

    // Default profile from addon.
    file = this.addon.getResourceURI().QueryInterface(Ci.nsIFileURL).file;
    file.append("profile");
    return file;
  }
});

exports.AddonSimulatorProcess = AddonSimulatorProcess;


function OldAddonSimulatorProcess(addon, options) {
  this.addon = addon;
  this.options = options;
}

var OASPp = OldAddonSimulatorProcess.prototype = Object.create(AddonSimulatorProcess.prototype);

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

    // Gaia profile.
    args.push("-profile", this.gaiaProfile.path);

    // Debugger server port.
    let port = parseInt(this.options.port);
    args.push("-dbgport", "" + port);

    // Ignore eventual zombie instances of b2g that are left over.
    args.push("-no-remote");

    return args;
  }
});

exports.OldAddonSimulatorProcess = OldAddonSimulatorProcess;
