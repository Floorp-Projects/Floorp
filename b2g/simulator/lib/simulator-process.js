/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

'use strict';

const { Cc, Ci, Cu, ChromeWorker } = require("chrome");

Cu.import("resource://gre/modules/Services.jsm");

const Environment = require("sdk/system/environment").env;
const Runtime = require("sdk/system/runtime");
const Subprocess = require("sdk/system/child_process/subprocess");
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const { EventEmitter } = Cu.import("resource://gre/modules/devtools/shared/event-emitter.js", {});


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

function SimulatorProcess(options) {
  this.options = options;

  EventEmitter.decorate(this);
  this.on("stdout", (e, data) => { console.log(data.trim()) });
  this.on("stderr", (e, data) => { console.error(data.trim()) });
}

SimulatorProcess.prototype = {

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
      environment = ["TMPDIR=" + Services.dirsvc.get("TmpD", Ci.nsIFile).path];
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
      stdout: data => {
        this.emit("stdout", data);
      },

      // emit stderr event
      stderr: data => {
        this.emit("stderr", data);
      },

      // on b2g instance exit, reset tracked process, remote debugger port and
      // shuttingDown flag, then finally emit an exit event
      done: (function(result) {
        console.log("B2G terminated with " + result.exitCode);
        this.process = null;
        this.emit("exit", result.exitCode);
      }).bind(this)
    });
  },

  // request a b2g instance kill
  kill: function() {
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

  // compute current b2g file handle
  get b2gExecutable() {
    if (this._executable) {
      return this._executable;
    }

    let executable = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    executable.initWithPath(this.options.runtimePath);

    if (!executable.exists()) {
      // B2G binaries not found
      throw Error("b2g-desktop Executable not found.");
    }

    this._executable = executable;

    return executable;
  },

  // compute b2g CLI arguments
  get b2gArguments() {
    let args = [];

    let profile = this.options.profilePath;
    args.push("-profile", profile);
    console.log("profile", profile);

    // NOTE: push dbgport option on the b2g-desktop commandline
    args.push("-start-debugger-server", "" + this.options.port);

    // Ignore eventual zombie instances of b2g that are left over
    args.push("-no-remote");

    return args;
  },
};

exports.SimulatorProcess = SimulatorProcess;
