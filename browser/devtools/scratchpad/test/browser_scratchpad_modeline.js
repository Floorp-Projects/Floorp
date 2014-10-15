/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 644413 */

let tempScope = {};
Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);
Cu.import("resource://gre/modules/FileUtils.jsm", tempScope);
let NetUtil = tempScope.NetUtil;
let FileUtils = tempScope.FileUtils;


let gScratchpad; // Reference to the Scratchpad object.
let gFile; // Reference to the temporary nsIFile we will work with.
let DEVTOOLS_CHROME_ENABLED = "devtools.chrome.enabled";

// The temporary file content.
let gFileContent = "function main() { return 0; }";

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(DEVTOOLS_CHROME_ENABLED, false);
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html,<p>test file open and save in Scratchpad";
}

function runTests() {
  gScratchpad = gScratchpadWindow.Scratchpad;
  function size(obj) { return Object.keys(obj).length; }

  // Test Scratchpad._scanModeLine method.
  let obj = gScratchpad._scanModeLine();
  is(size(obj), 0, "Mode-line object has no properties");

  obj = gScratchpad._scanModeLine("/* This is not a mode-line comment */");
  is(size(obj), 0, "Mode-line object has no properties");

  obj = gScratchpad._scanModeLine("/* -sp-context:browser */");
  is(size(obj), 1, "Mode-line object has one property");
  is(obj["-sp-context"], "browser");

  obj = gScratchpad._scanModeLine("/* -sp-context: browser */");
  is(size(obj), 1, "Mode-line object has one property");
  is(obj["-sp-context"], "browser");

  obj = gScratchpad._scanModeLine("// -sp-context: browser");
  is(size(obj), 1, "Mode-line object has one property");
  is(obj["-sp-context"], "browser");

  obj = gScratchpad._scanModeLine("/* -sp-context:browser, other:true */");
  is(size(obj), 2, "Mode-line object has two properties");
  is(obj["-sp-context"], "browser");
  is(obj["other"], "true");

  // Test importing files with a mode-line in them.
  let content = "/* -sp-context:browser */\n" + gFileContent;
  createTempFile("fileForBug644413.tmp", content, function(aStatus, aFile) {
    ok(Components.isSuccessCode(aStatus), "File was saved successfully");

    gFile = aFile;
    gScratchpad.importFromFile(gFile.QueryInterface(Ci.nsILocalFile), true, fileImported);
  });
}

function fileImported(status, content) {
  ok(Components.isSuccessCode(status), "File was imported successfully");

  // Since devtools.chrome.enabled is off, Scratchpad should still be in
  // the content context.
  is(gScratchpad.executionContext, gScratchpadWindow.SCRATCHPAD_CONTEXT_CONTENT);

  // Set the pref and try again.
  Services.prefs.setBoolPref(DEVTOOLS_CHROME_ENABLED, true);

  gScratchpad.importFromFile(gFile.QueryInterface(Ci.nsILocalFile), true, function(status, content) {
    ok(Components.isSuccessCode(status), "File was imported successfully");
    is(gScratchpad.executionContext, gScratchpadWindow.SCRATCHPAD_CONTEXT_BROWSER);

    gFile.remove(false);
    gFile = null;
    gScratchpad = null;
    finish();
  });
}

registerCleanupFunction(function () {
  Services.prefs.clearUserPref(DEVTOOLS_CHROME_ENABLED);
});
