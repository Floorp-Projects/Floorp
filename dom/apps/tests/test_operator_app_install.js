"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/OperatorApps.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

// From prio.h
const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE    = 0x20;

SimpleTest.waitForExplicitFinish();

var gApp = null;

var index = -1;
var singlevariantDir = undefined;

function debug(aMsg) {
  //dump("== Tests debug == " + aMsg + "\n");
}


var updateData = {
  name : "testOperatorApp1",
  version : 2,
  size : 767,
  package_path: "http://test/tests/dom/apps/tests/file_packaged_app.sjs",
  description: "Updated even faster than Firefox, just to annoy slashdotters",
  developer: {
    name: "Tester Operator App",
    url: "http://mochi.test:8888"
  }
};

var manifestData = {
  name : "testOperatorApp1",
  version : 2,
  description: "Updated even faster than Firefox, just to annoy slashdotters",
  launch_path: "index.html",
  developer: {
    name: "Tester Operator App",
    url: "http://mochi.test:8888"
  },
  default_locale: "en-US"
};

var metadataData = {
  id: "testOperatorApp1",
  installOrigin: "http://mochi.test:8888",
  manifestURL: "http://test/tests/dom/apps/tests/file_packaged_app.sjs",
  origin: "http://test"
};

function writeFile(aFile, aData, aCb) {
  debug("Saving " + aFile.path);
  // Initialize the file output stream.
  let ostream = FileUtils.openSafeFileOutputStream(aFile);

  // Obtain a converter to convert our data to a UTF-8 encoded input stream.
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                  .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";

  // Asynchronously copy the data to the file.
  let istream = converter.convertToInputStream(aData);
  NetUtil.asyncCopy(istream, ostream, function(rc) {
    FileUtils.closeSafeFileOutputStream(ostream);
    if (aCb)
      aCb();
  });
}

// File and resources helpers
function addZipEntry(zipWriter, entry, entryName) {
  var stream = Cc["@mozilla.org/io/string-input-stream;1"]
               .createInstance(Ci.nsIStringInputStream);
  stream.setData(entry, entry.length);
  zipWriter.addEntryStream(entryName, Date.now(),
                           Ci.nsIZipWriter.COMPRESSION_BEST, stream, false);
}

function setupDataDirs(aCb) {
  let dirNum = "tmp_" + Math.floor(Math.random() * 10000000 + 1);
  let tmpDir = FileUtils.getDir("TmpD", [dirNum, "singlevariantapps"], true,
                                true);
  let appDir = FileUtils.getDir("TmpD", [dirNum, "singlevariantapps",
                                "testOperatorApp1"], true, true);

  singlevariantDir = tmpDir.path;
  let singlevariantFile = tmpDir.clone();
  singlevariantFile.append("singlevariantconf.json");


  writeFile(singlevariantFile, JSON.stringify({"214-007":["testOperatorApp1"]}),
            function() {
    let indexhtml = "<html></html>";
    let manifest = JSON.stringify(manifestData);
    // Create the application package.
    var zipWriter = Cc["@mozilla.org/zipwriter;1"]
                    .createInstance(Ci.nsIZipWriter);
    var zipFile = FileUtils.getFile("TmpD", [
                                      dirNum,
                                      "singlevariantapps",
                                      "testOperatorApp1",
                                      "application.zip"]);
    zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
    addZipEntry(zipWriter, indexhtml, "index.html");
    addZipEntry(zipWriter, manifest, "manifest.webapp");
    zipWriter.close();

    var metadataFile = appDir.clone();
    metadataFile.append("metadata.json");
    writeFile(metadataFile, JSON.stringify(metadataData), function() {
      var updateFile = appDir.clone();
      updateFile.append("update.webapp");
      writeFile(updateFile, JSON.stringify(updateData), aCb);
    });
  });
}

function next() {
  index += 1;
  if (index >= steps.length) {
    ok(false, "Shouldn't get here!");
    return;
  }
  try {
    steps[index]();
  } catch(ex) {
    ok(false, "Caught exception", ex);
  }
}

function go() {
  next();
}

function finish() {
  SimpleTest.finish();
}

function mozAppsError() {
  ok(false, "mozApps error: " + this.error.name);
  finish();
}

function installOperatorApp(aMcc, aMnc) {
  OperatorAppsRegistry.appsDir = singlevariantDir;
  OperatorAppsRegistry._installOperatorApps(aMcc, aMnc);
}

function checkAppState(aApp,
                       aVersion,
                       aExpectedApp,
                       aCb) {
  debug(JSON.stringify(aApp, null, 2));
  if (aApp.manifest) {
    debug(JSON.stringify(aApp.manifest, null, 2));
  }

  if (aExpectedApp.name) {
    if (aApp.manifest) {
      is(aApp.manifest.name, aExpectedApp.name, "Check name");
    }
    is(aApp.updateManifest.name, aExpectedApp.name, "Check name mini-manifest");
  }
  if (aApp.manifest) {
    is(aApp.manifest.version, aVersion, "Check version");
  }
  if (typeof aExpectedApp.size !== "undefined" && aApp.manifest) {
    is(aApp.manifest.size, aExpectedApp.size, "Check size");
  }
  if (aApp.manifest) {
    is(aApp.manifest.launch_path, "index.html", "Check launch path");
  }
  if (aExpectedApp.manifestURL) {
    is(aApp.manifestURL, aExpectedApp.manifestURL, "Check manifestURL");
  }
  if (aExpectedApp.installOrigin) {
    is(aApp.installOrigin, aExpectedApp.installOrigin, "Check installOrigin");
  }
  ok(aApp.removable, "Removable app");
  if (typeof aExpectedApp.progress !== "undefined") {
    todo(aApp.progress == aExpectedApp.progress, "Check progress");
  }
  if (aExpectedApp.installState) {
    is(aApp.installState, aExpectedApp.installState, "Check installState");
  }
  if (typeof aExpectedApp.downloadAvailable !== "undefined") {
    is(aApp.downloadAvailable, aExpectedApp.downloadAvailable,
       "Check download available");
  }
  if (typeof aExpectedApp.downloading !== "undefined") {
    is(aApp.downloading, aExpectedApp.downloading, "Check downloading");
  }
  if (typeof aExpectedApp.downloadSize !== "undefined") {
    is(aApp.downloadSize, aExpectedApp.downloadSize, "Check downloadSize");
  }
  if (typeof aExpectedApp.readyToApplyDownload !== "undefined") {
    is(aApp.readyToApplyDownload, aExpectedApp.readyToApplyDownload,
       "Check readyToApplyDownload");
  }
  if (aCb && typeof aCb === 'function') {
    aCb();
  }
  return;
}

var steps = [
  function() {
    prepareEnv(next);
  },
  function() {
    setupDataDirs(next);
    ok(true, "Data directory set up to " + singlevariantDir);
  },
  function() {
    ok(true, "autoConfirmAppInstall");
    SpecialPowers.autoConfirmAppInstall(next);
  },
  function() {
    ok(true, "== TEST == Install operator app");

    navigator.mozApps.mgmt.oninstall = function(evt) {
      ok(true, "Got oninstall event");
      gApp = evt.application;
      gApp.ondownloaderror = function() {
        ok(false, "Download error " + gApp.downloadError.name);
        finish();
      };
      let downloadsuccessHandler = function() {
        gApp.ondownloadsuccess = null;
        ok(true, "App downloaded");

        var expected = {
          name: manifestData.name,
          manifestURL: metadataData.manifestURL,
          installOrigin: metadataData.installOrigin,
          progress: 0,
          installState: "installed",
          downloadAvailable: false,
          downloading: false,
          downloadSize: 767,
          readyToApplyDownload: false
        };
        checkAppState(gApp, manifestData.version, expected, next);
      };
      gApp.ondownloadsuccess = downloadsuccessHandler;
      if (!gApp.downloading && gApp.ondownloadsuccess) {
        ok(true, "Got an earlier event");
        // Seems we set the handler too late.
        gApp.ondownloadsuccess = null;
        downloadsuccessHandler();
      }
    };
    installOperatorApp("214", "007");
  },
  function() {
    ok(true, "all done!\n");
    finish();
  }
];

go();

