/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {};
Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);
Cu.import("resource://gre/modules/FileUtils.jsm", tempScope);
let NetUtil = tempScope.NetUtil;
let FileUtils = tempScope.FileUtils;

// Reference to the Scratchpad object.
let gScratchpad;

// Reference to the temporary nsIFile we will work with.
let gFile;

// The temporary file content.
let gFileContent = "function main() { return 0; }";

const SCRATCHPAD_CONTEXT_CONTENT = 1;
const SCRATCHPAD_CONTEXT_BROWSER = 2;

function test() {
  waitForExplicitFinish();

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
  is(gScratchpad.executionContext, SCRATCHPAD_CONTEXT_BROWSER);

  gFile.remove(false);
  gFile = null;
  gScratchpad = null;
  finish();
}

