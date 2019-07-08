/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["AsanReporter"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Log: "resource://gre/modules/Log.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["TextDecoder", "XMLHttpRequest"]);

// Define our prefs
const PREF_CLIENT_ID = "asanreporter.clientid";
const PREF_API_URL = "asanreporter.apiurl";
const PREF_AUTH_TOKEN = "asanreporter.authtoken";
const PREF_LOG_LEVEL = "asanreporter.loglevel";

const LOGGER_NAME = "asanreporter";

let logger;

XPCOMUtils.defineLazyGetter(this, "asanDumpDir", () => {
  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  return OS.Path.join(profileDir.path, "asan");
});

this.AsanReporter = {
  init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    // Setup logging
    logger = Log.repository.getLogger(LOGGER_NAME);
    logger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
    logger.addAppender(new Log.DumpAppender(new Log.BasicFormatter()));
    logger.level = Services.prefs.getIntPref(PREF_LOG_LEVEL, Log.Level.Info);

    logger.info("Starting up...");

    // Install a handler to observe tab crashes, so we can report those right
    // after they happen instead of relying on the user to restart the browser.
    Services.obs.addObserver(this, "ipc:content-shutdown");

    processDirectory();
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "ipc:content-shutdown") {
      aSubject.QueryInterface(Ci.nsIPropertyBag2);
      if (!aSubject.get("abnormal")) {
        return;
      }
      processDirectory();
    }
  },
};

function processDirectory() {
  let iterator = new OS.File.DirectoryIterator(asanDumpDir);
  let results = [];

  // Scan the directory for any ASan logs that we haven't
  // submitted yet. Store the filenames in an array so we
  // can close the iterator early.
  iterator
    .forEach(entry => {
      if (
        entry.name.indexOf("ff_asan_log.") == 0 &&
        !entry.name.includes("submitted")
      ) {
        results.push(entry);
      }
    })
    .then(
      () => {
        iterator.close();
        logger.info("Processing " + results.length + " reports...");

        // Sequentially submit all reports that we found. Note that doing this
        // with Promise.all would not result in a sequential ordering and would
        // cause multiple requests to be sent to the server at once.
        let requests = Promise.resolve();
        results.forEach(result => {
          requests = requests.then(
            // We return a promise here that already handles any submit failures
            // so our chain is not interrupted if one of the reports couldn't
            // be submitted for some reason.
            () =>
              submitReport(result.path).then(
                () => {
                  logger.info("Successfully submitted " + result.path);
                },
                e => {
                  logger.error(
                    "Failed to submit " + result.path + ". Reason: " + e
                  );
                }
              )
          );
        });

        requests.then(() => logger.info("Done processing reports."));
      },
      e => {
        iterator.close();
        logger.error("Error while iterating over report files: " + e);
      }
    );
}

function submitReport(reportFile) {
  logger.info("Processing " + reportFile);
  return OS.File.read(reportFile)
    .then(submitToServer)
    .then(() => {
      // Mark as submitted only if we successfully submitted it to the server.
      return OS.File.move(reportFile, reportFile + ".submitted");
    });
}

function submitToServer(data) {
  return new Promise(function(resolve, reject) {
    logger.debug("Setting up XHR request");
    let client = Services.prefs.getStringPref(PREF_CLIENT_ID);
    let api_url = Services.prefs.getStringPref(PREF_API_URL);
    let auth_token = Services.prefs.getStringPref(PREF_AUTH_TOKEN, null);

    let decoder = new TextDecoder();

    if (!client) {
      client = "unknown";
    }

    let versionArr = [
      Services.appinfo.version,
      Services.appinfo.appBuildID,
      AppConstants.SOURCE_REVISION_URL || "unknown",
    ];

    // Concatenate all relevant information as our server only
    // has one field available for version information.
    let product_version = versionArr.join("-");
    let os = AppConstants.platform;

    let reportObj = {
      rawStdout: "",
      rawStderr: "",
      rawCrashData: decoder.decode(data),
      // Hardcode platform as there is no other reasonable platform for ASan
      platform: "x86-64",
      product: "mozilla-central-asan-nightly",
      product_version,
      os,
      client,
      tool: "asan-nightly-program",
    };

    var xhr = new XMLHttpRequest();
    xhr.open("POST", api_url, true);
    xhr.setRequestHeader("Content-Type", "application/json");

    // For internal testing purposes, an auth_token can be specified
    if (auth_token) {
      xhr.setRequestHeader("Authorization", "Token " + auth_token);
    } else {
      // Prevent privacy leaks
      xhr.channel.loadFlags |= Ci.nsIRequest.LOAD_ANONYMOUS;
    }

    xhr.onreadystatechange = function() {
      if (xhr.readyState == 4) {
        if (xhr.status == "201") {
          logger.debug("XHR: OK");
          resolve(xhr);
        } else {
          logger.debug(
            "XHR: Status: " + xhr.status + " Response: " + xhr.responseText
          );
          reject(xhr);
        }
      }
    };

    xhr.send(JSON.stringify(reportObj));
  });
}
