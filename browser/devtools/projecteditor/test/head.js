/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const TargetFactory = devtools.TargetFactory;
const {console} = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
const promise = devtools.require("sdk/core/promise");
const {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm", {});
const {NetUtil} = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const ProjectEditor = devtools.require("projecteditor/projecteditor");

const TEST_URL_ROOT = "http://mochi.test:8888/browser/browser/devtools/projecteditor/test/";
const SAMPLE_WEBAPP_URL = TEST_URL_ROOT + "/helper_homepage.html";
let TEMP_PATH;

// All test are asynchronous
waitForExplicitFinish();

//Services.prefs.setBoolPref("devtools.dump.emit", true);

// Set the testing flag on gDevTools and reset it when the test ends
gDevTools.testing = true;
registerCleanupFunction(() => gDevTools.testing = false);

// Clear preferences that may be set during the course of tests.
registerCleanupFunction(() => {
  // Services.prefs.clearUserPref("devtools.dump.emit");
  TEMP_PATH = null;
});

// Auto close the toolbox and close the test tabs when the test ends
registerCleanupFunction(() => {
  try {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.closeToolbox(target);
  } catch (ex) {
    dump(ex);
  }
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

/**
 * Define an async test based on a generator function
 */
function asyncTest(generator) {
  return () => Task.spawn(generator).then(null, ok.bind(null, false)).then(finish);
}

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
function addTab(url) {
  info("Adding a new tab with URL: '" + url + "'");
  let def = promise.defer();

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    info("URL '" + url + "' loading complete");
    waitForFocus(() => {
      def.resolve(tab);
    }, content);
  }, true);
  content.location = url;

  return def.promise;
}

function addProjectEditorTabForTempDirectory() {
  TEMP_PATH = buildTempDirectoryStructure();
  let CUSTOM_OPTS = {
    name: "Test",
    iconUrl: "chrome://browser/skin/devtools/tool-options.svg",
    projectOverviewURL: SAMPLE_WEBAPP_URL
  };

  return addProjectEditorTab().then((projecteditor) => {
    return projecteditor.setProjectToAppPath(TEMP_PATH, CUSTOM_OPTS).then(() => {
      return projecteditor;
    });
  });
}

function addProjectEditorTab() {
  return addTab("chrome://browser/content/devtools/projecteditor-test.html").then(() => {
    let iframe = content.document.getElementById("projecteditor-iframe");
    let projecteditor = ProjectEditor.ProjectEditor(iframe);

    ok (iframe, "Tab has placeholder iframe for projecteditor");
    ok (projecteditor, "ProjectEditor has been initialized");

    return projecteditor.loaded.then((projecteditor) => {
      return projecteditor;
    });
  });
}

/**
 * Build a temporary directory as a workspace for this loader
 * https://developer.mozilla.org/en-US/Add-ons/Code_snippets/File_I_O
 */
function buildTempDirectoryStructure() {

  // First create (and remove) the temp dir to discard any changes
  let TEMP_DIR = FileUtils.getDir("TmpD", ["ProjectEditor"], true);
  TEMP_DIR.remove(true);

  // Now rebuild our fake project.
  TEMP_DIR = FileUtils.getDir("TmpD", ["ProjectEditor"], true);

  FileUtils.getDir("TmpD", ["ProjectEditor", "css"], true);
  FileUtils.getDir("TmpD", ["ProjectEditor", "data"], true);
  FileUtils.getDir("TmpD", ["ProjectEditor", "img", "icons"], true);
  FileUtils.getDir("TmpD", ["ProjectEditor", "js"], true);

  let htmlFile = FileUtils.getFile("TmpD", ["ProjectEditor", "index.html"]);
  htmlFile.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  writeToFile(htmlFile, [
    '<!DOCTYPE html>',
    '<html lang="en">',
    ' <head>',
    '   <meta charset="utf-8" />',
    '   <title>ProjectEditor Temp File</title>',
    '   <link rel="stylesheet" href="style.css" />',
    ' </head>',
    ' <body id="home">',
    '   <p>ProjectEditor Temp File</p>',
    ' </body>',
    '</html>'].join("\n")
  );

  let readmeFile = FileUtils.getFile("TmpD", ["ProjectEditor", "README.md"]);
  readmeFile.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  writeToFile(readmeFile, [
    '## Readme'
    ].join("\n")
  );

  let licenseFile = FileUtils.getFile("TmpD", ["ProjectEditor", "LICENSE"]);
  licenseFile.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  writeToFile(licenseFile, [
   '/* This Source Code Form is subject to the terms of the Mozilla Public',
   ' * License, v. 2.0. If a copy of the MPL was not distributed with this',
   ' * file, You can obtain one at http://mozilla.org/MPL/2.0/. */'
    ].join("\n")
  );

  let cssFile = FileUtils.getFile("TmpD", ["ProjectEditor", "css", "styles.css"]);
  cssFile.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  writeToFile(cssFile, [
    'body {',
    ' background: red;',
    '}'
    ].join("\n")
  );

  FileUtils.getFile("TmpD", ["ProjectEditor", "js", "script.js"]).createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  FileUtils.getFile("TmpD", ["ProjectEditor", "img", "fake.png"]).createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  FileUtils.getFile("TmpD", ["ProjectEditor", "img", "icons", "16x16.png"]).createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  FileUtils.getFile("TmpD", ["ProjectEditor", "img", "icons", "32x32.png"]).createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  FileUtils.getFile("TmpD", ["ProjectEditor", "img", "icons", "128x128.png"]).createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  FileUtils.getFile("TmpD", ["ProjectEditor", "img", "icons", "vector.svg"]).createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  return TEMP_DIR.path;
}

// https://developer.mozilla.org/en-US/Add-ons/Code_snippets/File_I_O#Writing_to_a_file
function writeToFile(file, data) {
  console.log("Writing to file: " + file.path, file.exists());
  let defer = promise.defer();
  var ostream = FileUtils.openSafeFileOutputStream(file);

  var converter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  var istream = converter.convertToInputStream(data);

  // The last argument (the callback) is optional.
  NetUtil.asyncCopy(istream, ostream, function(status) {
    if (!Components.isSuccessCode(status)) {
      // Handle error!
      info("ERROR WRITING TEMP FILE", status);
    }
  });
}

function getTempFile(path) {
  let parts = ["ProjectEditor"];
  parts = parts.concat(path.split("/"));
  return FileUtils.getFile("TmpD", parts);
}

// https://developer.mozilla.org/en-US/Add-ons/Code_snippets/File_I_O#Writing_to_a_file
function* getFileData(path) {
  let file = new FileUtils.File(path);
  let def = promise.defer();

  NetUtil.asyncFetch(file, function(inputStream, status) {
    if (!Components.isSuccessCode(status)) {
      info("ERROR READING TEMP FILE", status);
    }

    // Detect if an empty file is loaded
    try {
      inputStream.available();
    } catch(e) {
      def.resolve("");
      return;
    }

    var data = NetUtil.readInputStreamToString(inputStream, inputStream.available());
    def.resolve(data);
  });

  return def.promise;
}

function onceEditorCreated(projecteditor) {
  let def = promise.defer();
  projecteditor.once("onEditorCreated", (editor) => {
    def.resolve(editor);
  });
  return def.promise;
}

function onceEditorLoad(projecteditor) {
  let def = promise.defer();
  projecteditor.once("onEditorLoad", (editor) => {
    def.resolve(editor);
  });
  return def.promise;
}

function onceEditorActivated(projecteditor) {
  let def = promise.defer();
  projecteditor.once("onEditorActivated", (editor) => {
    def.resolve(editor);
  });
  return def.promise;
}

function onceEditorSave(projecteditor) {
  let def = promise.defer();
  projecteditor.once("onEditorSave", (editor, resource) => {
    def.resolve(resource);
  });
  return def.promise;
}
