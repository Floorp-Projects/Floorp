/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * LogShake is a module which listens for log requests sent by Gaia. In
 * response to a sufficiently large acceleration (a shake), it will save log
 * files to an arbitrary directory which it will then return on a
 * 'capture-logs-success' event with detail.logFilenames representing each log
 * file's name and detail.logPaths representing the patch to each log file or
 * the path to the archive.
 * If an error occurs it will instead produce a 'capture-logs-error' event.
 * We send a capture-logs-start events to notify the system app and the user,
 * since dumping can be a bit long sometimes.
 */

/* enable Mozilla javascript extensions and global strictness declaration,
 * disable valid this checking */
/* jshint moz: true, esnext: true */
/* jshint -W097 */
/* jshint -W040 */
/* global Services, Components, dump, LogCapture, LogParser,
   OS, Promise, volumeService, XPCOMUtils, SystemAppProxy */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// Constants for creating zip file taken from toolkit/webapps/tests/head.js
const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE    = 0x20;

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
 * shake.
 */
const EXCITEMENT_THRESHOLD = 500;
/**
 * The maximum fraction to update the excitement value per frame. This
 * corresponds to requiring shaking for approximately 10 motion events (1.6
 * seconds)
 */
const EXCITEMENT_FILTER_ALPHA = 0.2;
const DEVICE_MOTION_EVENT = "devicemotion";
const SCREEN_CHANGE_EVENT = "screenchange";
const CAPTURE_LOGS_CONTENT_EVENT = "requestSystemLogs";
const CAPTURE_LOGS_START_EVENT = "capture-logs-start";
const CAPTURE_LOGS_ERROR_EVENT = "capture-logs-error";
const CAPTURE_LOGS_SUCCESS_EVENT = "capture-logs-success";

var LogShake = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  /**
   * If LogShake is in QA Mode, which bundles all files into a compressed archive
   */
  qaModeEnabled: false,

  /**
   * If LogShake is listening for device motion events. Required due to lag
   * between HAL layer of device motion events and listening for device motion
   * events.
   */
  deviceMotionEnabled: false,

  /**
   * We only listen to motion events when the screen is enabled, keep track
   * of its state.
   */
  screenEnabled: true,

  /**
   * Flag monitoring if the preference to enable shake to capture is
   * enabled in gaia.
   */
  listenToDeviceMotion: true,

  /**
   * If a capture has been requested and is waiting for reads/parsing. Used for
   * debouncing.
   */
  captureRequested: false,

  /**
   * The current excitement (movement) level
   */
  excitement: 0,

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
    "/system/b2g/application.ini": LogParser.prettyPrintArray,
    "/cache/recovery/last_install": LogParser.prettyPrintArray,
    "/cache/recovery/last_kmsg": LogParser.prettyPrintArray,
    "/cache/recovery/last_log": LogParser.prettyPrintArray
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

    // Reset excitement to clear residual motion
    this.excitement = 0;

    SystemAppProxy.addEventListener(CAPTURE_LOGS_CONTENT_EVENT, this, false);
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

    case CAPTURE_LOGS_CONTENT_EVENT:
      this.startCapture();
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

  enableQAMode: function() {
    debug("Enabling QA Mode");
    this.qaModeEnabled = true;
  },

  disableQAMode: function() {
    debug("Disabling QA Mode");
    this.qaModeEnabled = false;
  },

  enableDeviceMotionListener: function() {
    this.listenToDeviceMotion = true;
    this.startDeviceMotionListener();
  },

  disableDeviceMotionListener: function() {
    this.listenToDeviceMotion = false;
    this.stopDeviceMotionListener();
  },

  startDeviceMotionListener: function() {
    if (!this.deviceMotionEnabled &&
        this.listenToDeviceMotion &&
        this.screenEnabled) {
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

    let acc = event.accelerationIncludingGravity;

    // Updates excitement by a factor of at most alpha, ignoring sudden device
    // motion. See bug #1101994 for more information.
    let newExcitement = acc.x * acc.x + acc.y * acc.y + acc.z * acc.z;
    this.excitement += (newExcitement - this.excitement) * EXCITEMENT_FILTER_ALPHA;

    if (this.excitement > EXCITEMENT_THRESHOLD) {
      this.startCapture();
    }
  },

  startCapture: function() {
    if (this.captureRequested) {
      return;
    }
    this.captureRequested = true;
    SystemAppProxy._sendCustomEvent(CAPTURE_LOGS_START_EVENT, {});
    this.captureLogs().then(logResults => {
      // On resolution send the success event to the requester
      SystemAppProxy._sendCustomEvent(CAPTURE_LOGS_SUCCESS_EVENT, {
        logPaths: logResults.logPaths,
        logFilenames: logResults.logFilenames
      });
      this.captureRequested = false;
    }, error => {
      // On an error send the error event
      SystemAppProxy._sendCustomEvent(CAPTURE_LOGS_ERROR_EVENT, {error: error});
      this.captureRequested = false;
    });
  },

  handleScreenChangeEvent: function(event) {
    this.screenEnabled = event.detail.screenEnabled;
    if (this.screenEnabled) {
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
    return this.readLogs().then(logArrays => {
      return this.saveLogs(logArrays);
    });
  },

  /**
   * Read in all log files, returning their formatted contents
   * @return {Promise<Array>}
   */
  readLogs: function() {
    let logArrays = {};
    let readPromises = [];

    try {
      logArrays["properties"] =
        LogParser.prettyPrintPropertiesArray(LogCapture.readProperties());
    } catch (ex) {
      Cu.reportError("Unable to get device properties: " + ex);
    }

    // Let Gecko perfom the dump to a file, and just collect it
    let readAboutMemoryPromise = new Promise(resolve => {
      // Wrap the readAboutMemory promise to make it infallible
      LogCapture.readAboutMemory().then(aboutMemory => {
        let file = OS.Path.basename(aboutMemory);
        let logArray;
        try {
          logArray = LogCapture.readLogFile(aboutMemory);
          if (!logArray) {
            debug("LogCapture.readLogFile() returned nothing for about:memory");
          }
          // We need to remove the dumped file, now that we have it in memory
          OS.File.remove(aboutMemory);
        } catch (ex) {
          Cu.reportError("Unable to handle about:memory dump: " + ex);
        }
        logArrays[file] = LogParser.prettyPrintArray(logArray);
        resolve();
      }, ex => {
        Cu.reportError("Unable to get about:memory dump: " + ex);
        resolve();
      });
    });
    readPromises.push(readAboutMemoryPromise);

    // Wrap the promise to make it infallible
    let readScreenshotPromise = new Promise(resolve => {
      LogCapture.getScreenshot().then(screenshot => {
        logArrays["screenshot.png"] = screenshot;
        resolve();
      }, ex => {
        Cu.reportError("Unable to get screenshot dump: " + ex);
        resolve();
      });
    });
    readPromises.push(readScreenshotPromise);

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

    // Because the promises we depend upon can't fail this means that the
    // blocking log reads will always be honored.
    return Promise.all(readPromises).then(() => {
      return logArrays;
    });
  },

  /**
   * Save the formatted arrays of log files to an sdcard if available
   */
  saveLogs: function(logArrays) {
    if (!logArrays || Object.keys(logArrays).length === 0) {
      return Promise.reject("Zero logs saved");
    }

    if (this.qaModeEnabled) {
      return makeBaseLogsDirectory().then(writeLogArchive(logArrays),
                                          rejectFunction("Error making base log directory"));
    } else {
      return makeBaseLogsDirectory().then(makeLogsDirectory,
                                          rejectFunction("Error making base log directory"))
                                    .then(writeLogFiles(logArrays),
                                          rejectFunction("Error creating log directory"));
    }
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

function getLogIdentifier() {
  let d = new Date();
  d = new Date(d.getTime() - d.getTimezoneOffset() * 60000);
  let timestamp = d.toISOString().slice(0, -5).replace(/[:T]/g, "-");
  return timestamp;
}

function rejectFunction(message) {
  return function(err) {
    debug(message + ": " + err);
    return Promise.reject(err);
  };
}

function makeBaseLogsDirectory() {
  let sdcardPrefix;
  try {
    sdcardPrefix = getSdcardPrefix();
  } catch(e) {
    // Handles missing sdcard
    return Promise.reject(e);
  }

  let dirNameRoot = getLogDirectoryRoot();

  let logsRoot = OS.Path.join(sdcardPrefix, dirNameRoot);

  debug("Creating base log directory at root " + sdcardPrefix);

  return OS.File.makeDir(logsRoot, {from: sdcardPrefix}).then(
    function() {
      return {
        sdcardPrefix: sdcardPrefix,
        basePrefix: dirNameRoot
      };
    }
  );
}

function makeLogsDirectory({sdcardPrefix, basePrefix}) {
  let dirName = getLogIdentifier();

  let logsRoot = OS.Path.join(sdcardPrefix, basePrefix);
  let logsDir = OS.Path.join(logsRoot, dirName);

  debug("Creating base log directory at root " + sdcardPrefix);
  debug("Final created directory will be " + logsDir);

  return OS.File.makeDir(logsDir, {ignoreExisting: false}).then(
    function() {
      debug("Created: " + logsDir);
      return {
        logPrefix: OS.Path.join(basePrefix, dirName),
        sdcardPrefix: sdcardPrefix
      };
    },
    rejectFunction("Error at OS.File.makeDir for " + logsDir)
  );
}

function getFile(filename) {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(filename);
  return file;
}

/**
 * Make a zip file
 * @param {String} absoluteZipFilename - Fully qualified desired location of the zip file
 * @param {Map<String, Uint8Array>} logArrays - Map from log location to log data
 * @return {Array<String>} Paths of entries in the archive
 */
function makeZipFile(absoluteZipFilename, logArrays) {
  let logFilenames = [];
  let zipWriter = Cc["@mozilla.org/zipwriter;1"].createInstance(Ci.nsIZipWriter);
  let zipFile = getFile(absoluteZipFilename);
  zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  for (let logLocation in logArrays) {
    let logArray = logArrays[logLocation];
    let logFilename = getLogFilename(logLocation);
    logFilenames.push(logFilename);

    debug("Adding " + logFilename + " to the zip");
    let logArrayStream = Cc["@mozilla.org/io/arraybuffer-input-stream;1"]
                           .createInstance(Ci.nsIArrayBufferInputStream);
    // Set data to be copied, default offset to 0 because it is not present on
    // ArrayBuffer objects
    logArrayStream.setData(logArray.buffer, logArray.byteOffset || 0,
                           logArray.byteLength);

    zipWriter.addEntryStream(logFilename, Date.now(),
                             Ci.nsIZipWriter.COMPRESSION_DEFAULT,
                             logArrayStream, false);
  }
  zipWriter.close();

  return logFilenames;
}

function writeLogArchive(logArrays) {
  return function({sdcardPrefix, basePrefix}) {
    // Now the directory is guaranteed to exist, save the logs into their
    // archive file

    let zipFilename = getLogIdentifier() + "-logs.zip";
    let zipPath = OS.Path.join(basePrefix, zipFilename);
    let zipPrefix = OS.Path.dirname(zipPath);
    let absoluteZipPath = OS.Path.join(sdcardPrefix, zipPath);

    debug("Creating zip file at " + zipPath);
    let logFilenames = [];
    try {
      logFilenames = makeZipFile(absoluteZipPath, logArrays);
    } catch(e) {
      return Promise.reject(e);
    }
    debug("Zip file created");

    return {
      logFilenames: logFilenames,
      logPaths: [zipPath],
      compressed: true
    };
  };
}

function writeLogFiles(logArrays) {
  return function({sdcardPrefix, logPrefix}) {
    // Now the directory is guaranteed to exist, save the logs
    let logFilenames = [];
    let logPaths = [];
    let saveRequests = [];

    for (let logLocation in logArrays) {
      debug("Requesting save of " + logLocation);
      let logArray = logArrays[logLocation];
      let logFilename = getLogFilename(logLocation);
      // The local pathrepresents the relative path within the SD card, not the
      // absolute path because Gaia will refer to it using the DeviceStorage
      // API
      let localPath = OS.Path.join(logPrefix, logFilename);

      logFilenames.push(logFilename);
      logPaths.push(localPath);

      let absolutePath = OS.Path.join(sdcardPrefix, localPath);
      let saveRequest = OS.File.writeAtomic(absolutePath, logArray);
      saveRequests.push(saveRequest);
    }

    return Promise.all(saveRequests).then(
      function() {
        return {
          logFilenames: logFilenames,
          logPaths: logPaths,
          compressed: false
        };
      },
      rejectFunction("Error at some save request")
    );
  };
}

LogShake.init();
this.LogShake = LogShake;
