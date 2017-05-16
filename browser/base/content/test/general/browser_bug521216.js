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
  if (actual.indexOf(aName) == -1)
    actual.push(aName);
  if (actual.length == expected.length) {
    is(actual.toString(), expected.toString(),
       "got events and progress notifications in expected order");

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
    record(arguments.callee.name);
}

var progressListener = {
  onLocationChange: function onLocationChange(aBrowser) {
    if (aBrowser == tab.linkedBrowser)
      record(arguments.callee.name);
  },
  onStateChange: function onStateChange(aBrowser) {
    if (aBrowser == tab.linkedBrowser)
      record(arguments.callee.name);
  },
  onLinkIconAvailable: function onLinkIconAvailable(aBrowser, aIconURL) {
    if (aBrowser == tab.linkedBrowser &&
        aIconURL == "about:logo")
      record(arguments.callee.name);
  }
};
