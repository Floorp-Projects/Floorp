var expected = [
  "TabOpen",
  "onStateChange",
  "onLocationChange",
  "onLinkIconAvailable",
];
var actual = [];
var tabIndex = -1;
this.__defineGetter__("tab", () => gBrowser.tabs[tabIndex]);

function test() {
  waitForExplicitFinish();
  tabIndex = gBrowser.tabs.length;
  gBrowser.addTabsProgressListener(progressListener);
  gBrowser.tabContainer.addEventListener("TabOpen", TabOpen);
  BrowserTestUtils.addTab(
    gBrowser,
    "data:text/html,<html><head><link href='about:logo' rel='shortcut icon'>"
  );
}

function recordEvent(aName) {
  info("got " + aName);
  if (!actual.includes(aName)) {
    actual.push(aName);
  }
  if (actual.length == expected.length) {
    is(
      actual.toString(),
      expected.toString(),
      "got events and progress notifications in expected order"
    );

    executeSoon(
      // eslint-disable-next-line no-shadow
      function(tab) {
        gBrowser.removeTab(tab);
        gBrowser.removeTabsProgressListener(progressListener);
        gBrowser.tabContainer.removeEventListener("TabOpen", TabOpen);
        finish();
      }.bind(null, tab)
    );
  }
}

function TabOpen(aEvent) {
  if (aEvent.target == tab) {
    recordEvent("TabOpen");
  }
}

var progressListener = {
  onLocationChange: function onLocationChange(aBrowser) {
    if (aBrowser == tab.linkedBrowser) {
      recordEvent("onLocationChange");
    }
  },
  onStateChange: function onStateChange(aBrowser) {
    if (aBrowser == tab.linkedBrowser) {
      recordEvent("onStateChange");
    }
  },
  onLinkIconAvailable: function onLinkIconAvailable(aBrowser) {
    if (aBrowser == tab.linkedBrowser) {
      recordEvent("onLinkIconAvailable");
    }
  },
};
