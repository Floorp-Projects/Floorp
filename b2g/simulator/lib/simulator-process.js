/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

'use strict';

const { Cc, Ci, Cu, ChromeWorker } = require("chrome");

Cu.import("resource://gre/modules/Services.jsm");

const { EventTarget } = require("sdk/event/target");
const { emit, off } = require("sdk/event/core");
const { Class } = require("sdk/core/heritage");
const Environment = require("sdk/system/environment").env;
const Runtime = require("sdk/system/runtime");
const URL = require("sdk/url");
const Subprocess = require("sdk/system/child_process/subprocess");
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

const ROOT_URI = require("addon").uri;
const PROFILE_URL = ROOT_URI + "profile/";
const BIN_URL = ROOT_URI + "b2g/";

// Log subprocess error and debug messages to the console.  This logs messages
// for all consumers of the API.  We trim the messages because they sometimes
// have trailing newlines.  And note that registerLogHandler actually registers
// an error handler, despite its name.
Subprocess.registerLogHandler(
  function(s) console.error("subprocess: " + s.trim())
);
Subprocess.registerDebugHandler(
  function(s) console.debug("subprocess: " + s.trim())
);

exports.SimulatorProcess = Class({
  extends: EventTarget,
  initialize: function initialize(options) {
    EventTarget.prototype.initialize.call(this, options);

    this.on("stdout", function onStdout(data) console.log(data.trim()));
    this.on("stderr", function onStderr(data) console.error(data.trim()));
  },

  // check if b2g is running
  get isRunning() !!this.process,

  /**
   * Start the process and connect the debugger client.
   */
  run: function() {
    // kill before start if already running
    if (this.process != null) {
      this.process
          .kill()
          .then(this.run.bind(this));
      return;
    }

    // resolve b2g binaries path (raise exception if not found)
    let b2gExecutable = this.b2gExecutable;

    this.once("stdout", function () {
      if (Runtime.OS == "Darwin") {
          console.debug("WORKAROUND run osascript to show b2g-desktop window"+
                        " on Runtime.OS=='Darwin'");
        // Escape double quotes and escape characters for use in AppleScript.
        let path = b2gExecutable.path
          .replace(/\\/g, "\\\\").replace(/\"/g, '\\"');

        Subprocess.call({
          command: "/usr/bin/osascript",
          arguments: ["-e", 'tell application "' + path + '" to activate'],
        });
      }
    });

    let environment;
    if (Runtime.OS == "Linux") {
      environment = ["TMPDIR=" + Services.dirsvc.get("TmpD",Ci.nsIFile).path];
      if ("DISPLAY" in Environment) {
        environment.push("DISPLAY=" + Environment.DISPLAY);
      }
    }

    // spawn a b2g instance
    this.process = Subprocess.call({
      command: b2gExecutable,
      arguments: this.b2gArguments,
      environment: environment,

      // emit stdout event
      stdout: (function(data) {
        emit(this, "stdout", data);
      }).bind(this),

      // emit stderr event
      stderr: (function(data) {
        emit(this, "stderr", data);
      }).bind(this),

      // on b2g instance exit, reset tracked process, remoteDebuggerPort and
      // shuttingDown flag, then finally emit an exit event
      done: (function(result) {
        console.log(this.b2gFilename + " terminated with " + result.exitCode);
        this.process = null;
        emit(this, "exit", result.exitCode);
      }).bind(this)
    });
  },

  // request a b2g instance kill
  kill: function() {
    let deferred = promise.defer();
    if (this.process) {
      this.once("exit", (exitCode) => {
        this.shuttingDown = false;
        deferred.resolve(exitCode);
      });
      if (!this.shuttingDown) {
        this.shuttingDown = true;
        emit(this, "kill", null);
        this.process.kill();
      }
      return deferred.promise;
    } else {
      return promise.resolve(undefined);
    }
  },

  // compute current b2g filename
  get b2gFilename() {
    return this._executable ? this._executableFilename : "B2G";
  },

  // compute current b2g file handle
  get b2gExecutable() {
    if (this._executable) {
      return this._executable;
    }
    let customRuntime;
    try {
      let pref = "extensions." + require("addon").id + ".customRuntime";
      customRuntime = Services.prefs.getComplexValue(pref, Ci.nsIFile);
    } catch(e) {}

    if (customRuntime) {
      this._executable = customRuntime;
      this._executableFilename = "Custom runtime";
      return this._executable;
    }

    let bin = URL.toFilename(BIN_URL);
    let executables = {
      WINNT: "b2g-bin.exe",
      Darwin: "B2G.app/Contents/MacOS/b2g-bin",
      Linux: "b2g-bin",
    };

    let path = bin;
    path += Runtime.OS == "WINNT" ? "\\" : "/";
    path += executables[Runtime.OS];
    console.log("simulator path: " + path);

    let executable = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    executable.initWithPath(path);

    if (!executable.exists()) {
      // B2G binaries not found
      throw Error("b2g-desktop Executable not found.");
    }

    this._executable = executable;
    this._executableFilename = "b2g-bin";

    return executable;
  },

  // compute b2g CLI arguments
  get b2gArguments() {
    let args = [];

    let gaiaProfile;
    try {
      let pref = "extensions." + require("addon").id + ".gaiaProfile";
      gaiaProfile = Services.prefs.getComplexValue(pref, Ci.nsIFile).path;
    } catch(e) {}

    let profile = gaiaProfile || URL.toFilename(PROFILE_URL);
    args.push("-profile", profile);
    console.log("profile", profile);

    // NOTE: push dbgport option on the b2g-desktop commandline
    args.push("-start-debugger-server", "" + this.remoteDebuggerPort);

    // Ignore eventual zombie instances of b2g that are left over
    args.push("-no-remote");

    return args;
  },
});

