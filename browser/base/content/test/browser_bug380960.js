function test() {
  var tab = gBrowser.addTab("about:blank", { skipAnimation: true });
  gBrowser.removeTab(tab);
  is(tab.parentNode, null, "tab removed immediately");

  waitForExplicitFinish();

  Services.prefs.setBoolPref("browser.tabs.animate", true);
  nextAsyncText();
}

function cleanup() {
  if (Services.prefs.prefHasUserValue("browser.tabs.animate"))
    Services.prefs.clearUserPref("browser.tabs.animate");
  finish();
}

var asyncTests = [
  function (tab) {
    info("closing tab with middle click");
    EventUtils.synthesizeMouse(tab, 2, 2, { button: 1 });
  },
  function (tab) {
    info("closing tab with accel+w");
    gBrowser.selectedTab = tab;
    content.focus();
    EventUtils.synthesizeKey("w", { accelKey: true });
  },
  function (tab) {
    info("closing tab by clicking the tab close button");
    gBrowser.selectedTab = tab;
    var button = document.getAnonymousElementByAttribute(tab, "anonid", "close-button");
    EventUtils.synthesizeMouse(button, 2, 2, {});
  }
];

function nextAsyncText() {
  var tab = gBrowser.addTab("about:blank", { skipAnimation: true });

  var gotCloseEvent = false;

  tab.addEventListener("TabClose", function () {
    gotCloseEvent = true;

    const DEFAULT_ANIMATION_LENGTH = 250;
    const MAX_WAIT_TIME = DEFAULT_ANIMATION_LENGTH * 3;
    const INTERVAL_LENGTH = 100;
    var polls = Math.ceil(MAX_WAIT_TIME / INTERVAL_LENGTH);
    var pollTabRemoved = setInterval(function () {
      --polls;
      if (tab.parentNode && polls > 0)
        return;
      clearInterval(pollTabRemoved);

      is(tab.parentNode, null, "tab removed after at most " + MAX_WAIT_TIME + " ms");

      if (asyncTests.length)
        nextAsyncText();
      else
        cleanup();
    }, INTERVAL_LENGTH);
  }, false);

  asyncTests.shift()(tab);

  ok(gotCloseEvent, "got the close event syncronously");

  is(tab.parentNode, gBrowser.tabContainer, "tab still exists when it's about to be removed asynchronously");
}
