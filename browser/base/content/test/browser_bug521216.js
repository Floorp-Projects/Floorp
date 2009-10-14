var expected = ["TabOpen", "onLocationChange", "onStateChange", "onLinkIconAvailable"];
var actual = [];
var tabIndex = -1;
__defineGetter__("tab", function () gBrowser.tabContainer.childNodes[tabIndex]);

function test() {
  waitForExplicitFinish();
  tabIndex = gBrowser.tabContainer.childElementCount;
  gBrowser.addTabsProgressListener(progressListener);
  gBrowser.tabContainer.addEventListener("TabOpen", TabOpen, false);
  gBrowser.addTab("http://example.org/browser/browser/base/content/test/dummy_page.html");
}

function record(aName) {
  info("got " + aName);
  if (actual.indexOf(aName) == -1)
    actual.push(aName);
  if (actual.length == expected.length) {
    is(actual.toString(), expected.toString(),
       "got events and progress notifications in expected order");
    gBrowser.removeTab(tab);
    gBrowser.removeTabsProgressListener(progressListener);
    gBrowser.tabContainer.removeEventListener("TabOpen", TabOpen, false);
    finish();
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
  onProgressChange: function () {},
  onSecurityChange: function () {},
  onStateChange: function onStateChange(aBrowser) {
    if (aBrowser == tab.linkedBrowser)
      record(arguments.callee.name);
  },
  onStatusChange: function () {},
  onLinkIconAvailable: function onLinkIconAvailable(aBrowser) {
    if (aBrowser == tab.linkedBrowser)
      record(arguments.callee.name);
  }
};
