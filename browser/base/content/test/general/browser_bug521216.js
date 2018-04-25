var expected = ["TabOpen", "onStateChange", "onLocationChange", "onLinkIconAvailable"];
var actual = [];
var tabIndex = -1;
this.__defineGetter__("tab", () => gBrowser.tabs[tabIndex]);

function test() {
  waitForExplicitFinish();
  tabIndex = gBrowser.tabs.length;
  gBrowser.addTabsProgressListener(progressListener);
  gBrowser.tabContainer.addEventListener("TabOpen", TabOpen);
  BrowserTestUtils.addTab(gBrowser, "data:text/html,<html><head><link href='about:logo' rel='shortcut icon'>");
}

function record(aName) {
  info("got " + aName);
  if (!actual.includes(aName))
    actual.push(aName);
  if (actual.length == expected.length) {
    is(actual.toString(), expected.toString(),
       "got events and progress notifications in expected order");

    // eslint-disable-next-line no-shadow
    executeSoon(function(tab) {
      gBrowser.removeTab(tab);
      gBrowser.removeTabsProgressListener(progressListener);
      gBrowser.tabContainer.removeEventListener("TabOpen", TabOpen);
      finish();
    }.bind(null, tab));
  }
}

function TabOpen(aEvent) {
  if (aEvent.target == tab)
    record("TabOpen");
}

var progressListener = {
  onLocationChange: function onLocationChange(aBrowser) {
    if (aBrowser == tab.linkedBrowser)
      record("onLocationChange");
  },
  onStateChange: function onStateChange(aBrowser) {
    if (aBrowser == tab.linkedBrowser)
      record("onStateChange");
  },
  onLinkIconAvailable: function onLinkIconAvailable(aBrowser, aIconURL) {
    if (aBrowser == tab.linkedBrowser &&
        aIconURL == "about:logo")
      record("onLinkIconAvailable");
  }
};
