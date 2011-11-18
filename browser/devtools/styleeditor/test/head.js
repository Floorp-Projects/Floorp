/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_BASE = "chrome://mochitests/content/browser/browser/devtools/styleeditor/test/";
const TEST_BASE_HTTP = "http://example.com/browser/browser/devtools/styleeditor/test/";
const TEST_BASE_HTTPS = "https://example.com/browser/browser/devtools/styleeditor/test/";

let gChromeWindow;               //StyleEditorChrome window

function cleanup()
{
  if (gChromeWindow) {
    gChromeWindow.close();
    gChromeWindow = null;
  }
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}

function launchStyleEditorChrome(aCallback)
{
  gChromeWindow = StyleEditor.openChrome();
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

function addTabAndLaunchStyleEditorChromeWhenLoaded(aCallback)
{
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    launchStyleEditorChrome(aCallback);
  }, true);
}

registerCleanupFunction(cleanup);
