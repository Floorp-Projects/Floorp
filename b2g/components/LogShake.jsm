/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * LogShake is a module which listens for log requests sent by Gaia. In
 * response to a sufficiently large acceleration (a shake), it will save log
 * files to an arbitrary directory which it will then return on a
 * 'capture-logs-success' event with detail.logFilenames representing each log
 * file's filename in the directory. If an error occurs it will instead produce
 * a 'capture-logs-error' event.
 * We send a capture-logs-start events to notify the system app and the user,
 * since dumping can be a bit long sometimes.
 */

/* enable Mozilla javascript extensions and global strictness declaration,
 * disable valid this checking */
/* jshint moz: true */
/* jshint -W097 */
/* jshint -W040 */
/* global Services, Components, dump, LogCapture, LogParser,
   OS, Promise, volumeService, XPCOMUtils, SystemAppProxy */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LogCapture", "resource://gre/modules/LogCapture.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LogParser", "resource://gre/modules/LogParser.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise", "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy", "resource://gre/modules/SystemAppProxy.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "powerManagerService",
                                   "@mozilla.org/power/powermanagerservice;1",
                                   "nsIPowerManagerService");

XPCOMUtils.defineLazyServiceGetter(this, "volumeService",
                                   "@mozilla.org/telephony/volume-service;1",
                                   "nsIVolumeService");

this.EXPORTED_SYMBOLS = ["LogShake"];

function debug(msg) {
  dump("LogShake.jsm: "+msg+"\n");
}

/**
 * An empirically determined amount of acceleration corresponding to a
 * shake
 */
const EXCITEMENT_THRESHOLD = 500;
const DEVICE_MOTION_EVENT = "devicemotion";
const SCREEN_CHANGE_EVENT = "screenchange";
const CAPTURE_LOGS_START_EVENT = "capture-logs-start";
const CAPTURE_LOGS_ERROR_EVENT = "capture-logs-error";
const CAPTURE_LOGS_SUCCESS_EVENT = "capture-logs-success";

let LogShake = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
  /**
   * If LogShake is listening for device motion events. Required due to lag
   * between HAL layer of device motion events and listening for device motion
   * events.
   */
  deviceMotionEnabled: false,

  /**
   * If a capture has been requested and is waiting for reads/parsing. Used for
   * debouncing.
   */
  captureRequested: false,

  /**
   * Map of files which have log-type information to their parsers
   */
  LOGS_WITH_PARSERS: {
    "/dev/log/main": LogParser.prettyPrintLogArray,
    "/dev/log/system": LogParser.prettyPrintLogArray,
    "/dev/log/radio": LogParser.prettyPrintLogArray,
    "/dev/log/events": LogParser.prettyPrintLogArray,
    "/proc/cmdline": LogParser.prettyPrintArray,
    "/proc/kmsg": LogParser.prettyPrintArray,
    "/proc/last_kmsg": LogParser.prettyPrintArray,
    "/proc/meminfo": LogParser.prettyPrintArray,
    "/proc/uptime": LogParser.prettyPrintArray,
    "/proc/version": LogParser.prettyPrintArray,
    "/proc/vmallocinfo": LogParser.prettyPrintArray,
    "/proc/vmstat": LogParser.prettyPrintArray,
    "/system/b2g/application.ini": LogParser.prettyPrintArray
  },

  /**
   * Start existing, observing motion events if the screen is turned on.
   */
  init: function() {
    // TODO: no way of querying screen state from power manager
    // this.handleScreenChangeEvent({ detail: {
    //   screenEnabled: powerManagerService.screenEnabled
    // }});

    // However, the screen is always on when we are being enabled because it is
    // either due to the phone starting up or a user enabling us directly.
    this.handleScreenChangeEvent({ detail: {
      screenEnabled: true
    }});

    SystemAppProxy.addEventListener(SCREEN_CHANGE_EVENT, this, false);

    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  /**
   * Handle an arbitrary event, passing it along to the proper function
   */
  handleEvent: function(event) {
    switch (event.type) {
    case DEVICE_MOTION_EVENT:
      if (!this.deviceMotionEnabled) {
        return;
      }
      this.handleDeviceMotionEvent(event);
      break;

    case SCREEN_CHANGE_EVENT:
      this.handleScreenChangeEvent(event);
      break;
    }
  },

  /**
   * Handle an observation from Services.obs
   */
  observe: function(subject, topic) {
    if (topic === "xpcom-shutdown") {
      this.uninit();
    }
  },

  startDeviceMotionListener: function() {
    if (!this.deviceMotionEnabled) {
      SystemAppProxy.addEventListener(DEVICE_MOTION_EVENT, this, false);
      this.deviceMotionEnabled = true;
    }
  },

  stopDeviceMotionListener: function() {
    SystemAppProxy.removeEventListener(DEVICE_MOTION_EVENT, this, false);
    this.deviceMotionEnabled = false;
  },

  /**
   * Handle a motion event, keeping track of "excitement", the magnitude
   * of the device"s acceleration.
   */
  handleDeviceMotionEvent: function(event) {
    // There is a lag between disabling the event listener and event arrival
    // ceasing.
    if (!this.deviceMotionEnabled) {
      return;
    }

    var acc = event.accelerationIncludingGravity;

    var excitement = acc.x * acc.x + acc.y * acc.y + acc.z * acc.z;

    if (excitement > EXCITEMENT_THRESHOLD) {
      if (!this.captureRequested) {
        this.captureRequested = true;
        SystemAppProxy._sendCustomEvent(CAPTURE_LOGS_START_EVENT, {});
        this.captureLogs().then(logResults => {
          // On resolution send the success event to the requester
          SystemAppProxy._sendCustomEvent(CAPTURE_LOGS_SUCCESS_EVENT, {
            logFilenames: logResults.logFilenames,
            logPrefix: logResults.logPrefix
          });
          this.captureRequested = false;
        },
        error => {
          // On an error send the error event
          SystemAppProxy._sendCustomEvent(CAPTURE_LOGS_ERROR_EVENT, {error: error});
          this.captureRequested = false;
        });
      }
    }
  },

  handleScreenChangeEvent: function(event) {
    if (event.detail.screenEnabled) {
      this.startDeviceMotionListener();
    } else {
      this.stopDeviceMotionListener();
    }
  },

  /**
   * Captures and saves the current device logs, returning a promise that will
   * resolve to an array of log filenames.
   */
  captureLogs: function() {
    let logArrays = this.readLogs();
    return saveLogs(logArrays);
  },

  /**
   * Read in all log files, returning their formatted contents
   */
  readLogs: function() {
    let logArrays = {};

    try {
      logArrays["properties"] =
        LogParser.prettyPrintPropertiesArray(LogCapture.readProperties());
    } catch (ex) {
      Cu.reportError("Unable to get device properties: " + ex);
    }

    // Let Gecko perfom the dump to a file, and just collect it
    try {
      LogCapture.readAboutMemory().then(aboutMemory => {
        let file = OS.Path.basename(aboutMemory);
        let logArray;
        try {
          logArray = LogCapture.readLogFile(aboutMemory);
          if (!logArray) {
            debug("LogCapture.readLogFile() returned nothing about:memory ");
          }
          // We need to remove the dumped file, now that we have it in memory
          OS.File.remove(aboutMemory);
        } catch (ex) {
          Cu.reportError("Unable to handle about:memory dump: " + ex);
        }
        logArrays[file] = LogParser.prettyPrintArray(logArray);
      });
    } catch (ex) {
      Cu.reportError("Unable to get about:memory dump: " + ex);
    }

    for (let loc in this.LOGS_WITH_PARSERS) {
      let logArray;
      try {
        logArray = LogCapture.readLogFile(loc);
        if (!logArray) {
          debug("LogCapture.readLogFile() returned nothing for: " + loc);
          continue;
        }
      } catch (ex) {
        Cu.reportError("Unable to LogCapture.readLogFile('" + loc + "'): " + ex);
        continue;
      }

      try {
        logArrays[loc] = this.LOGS_WITH_PARSERS[loc](logArray);
      } catch (ex) {
        Cu.reportError("Unable to parse content of '" + loc + "': " + ex);
        continue;
      }
    }
    return logArrays;
  },

  /**
   * Stop logshake, removing all listeners
   */
  uninit: function() {
    this.stopDeviceMotionListener();
    SystemAppProxy.removeEventListener(SCREEN_CHANGE_EVENT, this, false);
    Services.obs.removeObserver(this, "xpcom-shutdown");
  }
};

function getLogFilename(logLocation) {
  // sanitize the log location
  let logName = logLocation.replace(/\//g, "-");
  if (logName[0] === "-") {
    logName = logName.substring(1);
  }

  // If no extension is provided, default to forcing .log
  let extension = ".log";
  let logLocationExt = logLocation.split(".");
  if (logLocationExt.length > 1) {
    // otherwise, just append nothing
    extension = "";
  }

  return logName + extension;
}

function getSdcardPrefix() {
  return volumeService.getVolumeByName("sdcard").mountPoint;
}

function getLogDirectoryRoot() {
  return "logs";
}

function getLogDirectory() {
  let d = new Date();
  d = new Date(d.getTime() - d.getTimezoneOffset() * 60000);
  let timestamp = d.toISOString().slice(0, -5).replace(/[:T]/g, "-");
  return timestamp;
}

/**
 * Save the formatted arrays of log files to an sdcard if available
 */
function saveLogs(logArrays) {
  if (!logArrays || Object.keys(logArrays).length === 0) {
    return Promise.resolve({
      logFilenames: [],
      logPrefix: ""
    });
  }

  let sdcardPrefix, dirNameRoot, dirName;
  try {
    sdcardPrefix = getSdcardPrefix();
    dirNameRoot = getLogDirectoryRoot();
    dirName = getLogDirectory();
  } catch(e) {
    // Return promise failed with exception e
    // Handles missing sdcard
    return Promise.reject(e);
  }

  debug("making a directory all the way from " + sdcardPrefix + " to " + (sdcardPrefix + "/" + dirNameRoot + "/" + dirName) );
  let logsRoot = OS.Path.join(sdcardPrefix, dirNameRoot);
  return OS.File.makeDir(logsRoot, {from: sdcardPrefix}).then(
    function() {
      debug("First OS.File.makeDir done");
      let logsDir = OS.Path.join(logsRoot, dirName);
      debug("Creating " + logsDir);
      return OS.File.makeDir(logsDir, {ignoreExisting: false}).then(
        function() {
          debug("Created: " + logsDir);
          // Now the directory is guaranteed to exist, save the logs
          let logFilenames = [];
          let saveRequests = [];

          debug("Will now traverse logArrays: " + logArrays.length);

          for (let logLocation in logArrays) {
            debug("requesting save of " + logLocation);
            let logArray = logArrays[logLocation];
            // The filename represents the relative path within the SD card, not the
            // absolute path because Gaia will refer to it using the DeviceStorage
            // API
            let filename = OS.Path.join(dirNameRoot, dirName, getLogFilename(logLocation));
            logFilenames.push(filename);
            let saveRequest = OS.File.writeAtomic(OS.Path.join(sdcardPrefix, filename), logArray);
            saveRequests.push(saveRequest);
          }

          return Promise.all(saveRequests).then(
            function() {
              debug("returning logfilenames: "+logFilenames.toSource());
              return {
                logFilenames: logFilenames,
                logPrefix: OS.Path.join(dirNameRoot, dirName)
              };
            }, function(err) {
              debug("Error at some save request: " + err);
              return Promise.reject(err);
            });
        }, function(err) {
          debug("Error at OS.File.makeDir for " + logsDir + ": " + err);
          return Promise.reject(err);
        });
    }, function(err) {
      debug("Error at first OS.File.makeDir: " + err);
      return Promise.reject(err);
    });
}

LogShake.init();
this.LogShake = LogShake;
