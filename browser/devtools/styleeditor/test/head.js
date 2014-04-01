/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_BASE = "chrome://mochitests/content/browser/browser/devtools/styleeditor/test/";
const TEST_BASE_HTTP = "http://example.com/browser/browser/devtools/styleeditor/test/";
const TEST_BASE_HTTPS = "https://example.com/browser/browser/devtools/styleeditor/test/";
const TEST_HOST = 'mochi.test:8888';

let tempScope = {};
Cu.import("resource://gre/modules/devtools/Loader.jsm", tempScope);
let TargetFactory = tempScope.devtools.TargetFactory;
Cu.import("resource://gre/modules/LoadContextInfo.jsm", tempScope);
let LoadContextInfo = tempScope.LoadContextInfo;
Cu.import("resource://gre/modules/devtools/Console.jsm", tempScope);
let console = tempScope.console;

let gPanelWindow;
let cache = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
              .getService(Ci.nsICacheStorageService);


// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "../../../commandline/test/helpers.js", this);

gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  gDevTools.testing = false;
});

function cleanup()
{
  gPanelWindow = null;
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}

function addTabAndOpenStyleEditors(count, callback) {
  let currentCount = 0;
  let panel;
  addTabAndCheckOnStyleEditorAdded(p => panel = p, function () {
    currentCount++;
    info(currentCount + " of " + count + " editors opened");
    if (currentCount == count) {
      callback(panel);
    }
  });
}

function addTabAndCheckOnStyleEditorAdded(callbackOnce, callbackOnAdded) {
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openStyleEditorInWindow(window, function (panel) {
      // Execute the individual callback with the panel argument.
      callbackOnce(panel);
      // Report editors that already opened while loading.
      for (let editor of panel.UI.editors) {
        callbackOnAdded(editor);
      }
      // Report new editors added afterwards.
      panel.UI.on("editor-added", (event, editor) => callbackOnAdded(editor));
    });
  }, true);
}

function openStyleEditorInWindow(win, callback) {
  let target = TargetFactory.forTab(win.gBrowser.selectedTab);
  win.gDevTools.showToolbox(target, "styleeditor").then(function(toolbox) {
    let panel = toolbox.getCurrentPanel();
    gPanelWindow = panel._panelWin;

    panel.UI._alwaysDisableAnimations = true;
    callback(panel);
  });
}

function checkDiskCacheFor(host, done)
{
  let foundPrivateData = false;

  Visitor.prototype = {
    onCacheStorageInfo: function(num, consumption)
    {
      info("disk storage contains " + num + " entries");
    },
    onCacheEntryInfo: function(entry)
    {
      info(entry.key);
      foundPrivateData |= entry.key.contains(host);
    },
    onCacheEntryVisitCompleted: function()
    {
      is(foundPrivateData, false, "web content present in disk cache");
      done();
    }
  };
  function Visitor() {}

  var storage = cache.diskCacheStorage(LoadContextInfo.default, false);
  storage.asyncVisitStorage(new Visitor(), true /* Do walk entries */);
}

registerCleanupFunction(cleanup);
