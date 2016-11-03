/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Screenshot"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
// See LOG_LEVELS in Console.jsm. Common examples: "All", "Info", "Warn", & "Error".
const PREF_LOG_LEVEL = "extensions.mozscreenshots@mozilla.org.loglevel";
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = Cu.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  let consoleOptions = {
    maxLogLevel: "info",
    maxLogLevelPref: PREF_LOG_LEVEL,
    prefix: "mozscreenshots",
  };
  return new ConsoleAPI(consoleOptions);
});

this.Screenshot = {
  _extensionPath: null,
  _path: null,
  _imagePrefix: "",
  _imageExtension: ".png",
  _screenshotFunction: null,

  init(path, extensionPath, imagePrefix = "") {
    this._path = path;

    let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    dir.initWithPath(this._path);
    if (!dir.exists()) {
      dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    }

    this._extensionPath = extensionPath;
    this._imagePrefix = imagePrefix;
    switch (Services.appinfo.OS) {
      case "WINNT":
        this._screenshotFunction = this._screenshotWindows;
        break;
      case "Darwin":
        this._screenshotFunction = this._screenshotOSX;
        break;
      case "Linux":
        this._screenshotFunction = this._screenshotLinux;
        break;
      default:
        throw new Error("Unsupported operating system");
        break;
    }
  },

  _buildImagePath(baseName) {
    return OS.Path.join(this._path, this._imagePrefix + baseName + this._imageExtension);
  },

  // Capture the whole screen using an external application.
  captureExternal(filename) {
    let imagePath = this._buildImagePath(filename);
    return this._screenshotFunction(imagePath).then(() => {
      log.debug("saved screenshot: " + filename);
    });
  },

  // helpers

  _screenshotWindows(filename) {
    return new Promise((resolve, reject) => {
      let exe = Services.dirsvc.get("GreBinD", Ci.nsIFile);
      exe.append("screenshot.exe");
      if (!exe.exists()) {
        exe = Services.dirsvc.get("CurWorkD", Ci.nsIFile).parent;
        exe.append("bin");
        exe.append("screenshot.exe");
      }
      let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
      process.init(exe);

      let args = [filename];
      process.runAsync(args, args.length, this._processObserver(resolve, reject));
    });
  },

  _screenshotOSX: Task.async(function*(filename) {
    let screencapture = (windowID = null) => {
      return new Promise((resolve, reject) => {
        // Get the screencapture executable
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.initWithPath("/usr/sbin/screencapture");

        let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
        process.init(file);

        // Run the process.
        let args = ["-x", "-t", "png"];
        // Darwin version number for OS X 10.6 is 10.x
        if (windowID && Services.sysinfo.getProperty("version").indexOf("10.") !== 0) {
          // Capture only that window on 10.7+
          args.push("-l");
          args.push(windowID);
        }
        args.push(filename);
        process.runAsync(args, args.length, this._processObserver(resolve, reject));
      });
    };

    function readWindowID() {
      let decoder = new TextDecoder();
      let promise = OS.File.read("/tmp/mozscreenshots-windowid");
      return promise.then(function onSuccess(array) {
          return decoder.decode(array);
      });
    }

    let promiseWindowID = () => {
      return new Promise((resolve, reject) => {
        // Get the window ID of the application (assuming its front-most)
        let osascript = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        osascript.initWithPath("/bin/bash");

        let osascriptP = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
        osascriptP.init(osascript);
        let osaArgs = ["-c", "/usr/bin/osascript -e 'tell application (path to frontmost application as text) to set winID to id of window 1' > /tmp/mozscreenshots-windowid"];
        osascriptP.runAsync(osaArgs, osaArgs.length, this._processObserver(resolve, reject));
      });
    };

    yield promiseWindowID();
    let windowID = yield readWindowID();
    yield screencapture(windowID);
  }),

  _screenshotLinux(filename) {
    return new Promise((resolve, reject) => {
      let exe = Services.dirsvc.get("GreBinD", Ci.nsIFile);
      exe.append("screentopng");
      if (!exe.exists()) {
        exe = Services.dirsvc.get("CurWorkD", Ci.nsIFile).parent;
        exe.append("bin");
        exe.append("screentopng");
      }
      let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
      process.init(exe);

      let args = [filename];
      process.runAsync(args, args.length, this._processObserver(resolve, reject));
    });
  },

  _processObserver(resolve, reject) {
    return {
      observe(subject, topic, data) {
        switch (topic) {
          case "process-finished":
            try {
              // Wait 1s after process to resolve
              setTimeout(resolve, 1000);
            } catch (ex) {
              reject(ex);
            }
            break;
          default:
            reject(topic);
            break;
        }
      },
    };
  },
};
