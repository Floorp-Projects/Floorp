var tab = null;

function test() {
  waitForExplicitFinish();

  let pageLoaded = {
    onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
        gBrowser.removeProgressListener(this);
        executeSoon(checkURLBarRevert);
      }
    }
  }

  gBrowser.addProgressListener(pageLoaded);
  tab = BrowserTestUtils.addTab(gBrowser, "http://example.com");
  gBrowser.selectedTab = tab;
}

function checkURLBarRevert() {
  let originalValue = gURLBar.value;

  gBrowser.userTypedValue = "foobar";
  gBrowser.selectedTab = gBrowser.tabs[0];
  gBrowser.selectedTab = tab;
  is(gURLBar.value, "foobar", "location bar displays typed value");

  gURLBar.focus();

  EventUtils.synthesizeKey("VK_ESCAPE", {});

  is(gURLBar.value, originalValue, "ESC reverted the location bar value");

  gBrowser.removeTab(tab);
  finish();
}
