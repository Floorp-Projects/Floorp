/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let tab, browser;

function tearDown()
{
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}

registerCleanupFunction(tearDown);

function addTab(aURL)
{
  gBrowser.selectedTab = gBrowser.addTab();
  content.location = aURL;
  tab = gBrowser.selectedTab;
  browser = gBrowser.getBrowserForTab(tab);
}

function onLoad() {
  executeSoon(function (){
    browser.removeEventListener("npapi-carbon-event-model-failure", 
				arguments.callee, false);
    let notificationBox = gBrowser.getNotificationBox();
    let notificationBar = notificationBox.getNotificationWithValue("carbon-failure-plugins");
    ok(notificationBar, "Carbon Error plugin notification bar was found");
    finish();
  });
}

function test() {
  try {
    let abi = Services.appinfo.XPCOMABI;
    if (!abi.match(/64/)) {
      todo(false, "canceling test, wrong platform");
      return;
    }
    let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].getService(Ci.nsIMacUtils);
     if (!macutils.isUniversalBinary) {
       todo(false, "canceling test, not a universal build")
       return;
     }
  } 
  catch (e) { 
    return; 
  }
  waitForExplicitFinish();
  addTab('data:text/html,<h1>Plugin carbon mismatch test</h1><embed id="test" style="width: 100px; height: 100px" type="application/x-test-carbon">');
  gBrowser.addEventListener("npapi-carbon-event-model-failure", onLoad, false);
}

