/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_BASE = "chrome://mochitests/content/browser/browser/devtools/styleeditor/test/";
const TEST_BASE_HTTP = "http://example.com/browser/browser/devtools/styleeditor/test/";
const TEST_BASE_HTTPS = "https://example.com/browser/browser/devtools/styleeditor/test/";
const TEST_HOST = 'mochi.test:8888';

let tempScope = {};
Cu.import("resource:///modules/devtools/Target.jsm", tempScope);
let TargetFactory = tempScope.TargetFactory;

let gChromeWindow;               //StyleEditorChrome window
let cache = Cc["@mozilla.org/network/cache-service;1"]
              .getService(Ci.nsICacheService);


// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helpers.js", this);

function cleanup()
{
  gChromeWindow = null;
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}

function launchStyleEditorChrome(aCallback, aSheet, aLine, aCol)
{
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  let panel = gDevTools.getPanelForTarget("styleeditor", target);
  if (panel && panel.isReady) {
    gChromeWindow = panel._panelWin;
    gChromeWindow.styleEditorChrome._alwaysDisableAnimations = true;
    if (aSheet) {
      panel.selectStyleSheet(aSheet, aLine, aCol);
    }
    aCallback(gChromeWindow.styleEditorChrome);
  } else {
    let toolbox = gDevTools.openToolboxForTab(target, "styleeditor");
    toolbox.once("styleeditor-ready", function(event, panel) {
      gChromeWindow = panel._panelWin;
      gChromeWindow.styleEditorChrome._alwaysDisableAnimations = true;
      if (aSheet) {
        panel.selectStyleSheet(aSheet, aLine, aCol);
      }
      aCallback(gChromeWindow.styleEditorChrome);
    });
  }
}

function launchStyleEditorChromeFromWindow(aWindow, aCallback, aSheet, aLine, aCol)
{
  gChromeWindow = aWindow.StyleEditor.openChrome(aSheet, aLine, aCol);
  if (gChromeWindow.document.readyState != "complete") {
    gChromeWindow.addEventListener("load", function onChromeLoad() {
      gChromeWindow.removeEventListener("load", onChromeLoad, true);
      gChromeWindow.styleEditorChrome._alwaysDisableAnimations = true;
      aCallback(gChromeWindow.styleEditorChrome);
    }, true);
  } else {
    gChromeWindow.styleEditorChrome._alwaysDisableAnimations = true;
    aCallback(gChromeWindow.styleEditorChrome);
  }
}

function addTabAndLaunchStyleEditorChromeWhenLoaded(aCallback, aSheet, aLine, aCol)
{
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    launchStyleEditorChrome(aCallback, aSheet, aLine, aCol);
  }, true);
}

function checkDiskCacheFor(host)
{
  let foundPrivateData = false;

  let visitor = {
    visitDevice: function(deviceID, deviceInfo) {
      if (deviceID == "disk")
        info("disk device contains " + deviceInfo.entryCount + " entries");
      return deviceID == "disk";
    },

    visitEntry: function(deviceID, entryInfo) {
      info(entryInfo.key);
      foundPrivateData |= entryInfo.key.contains(host);
      is(foundPrivateData, false, "web content present in disk cache");
    }
  };
  cache.visitEntries(visitor);
  is(foundPrivateData, false, "private data present in disk cache");
}

registerCleanupFunction(cleanup);
