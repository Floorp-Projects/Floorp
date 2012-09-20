function test() {
  gBrowser.tabContainer.addEventListener("TabOpen", tabAdded, false);

  var tab = gBrowser.addTab("about:blank", { skipAnimation: true });
  gBrowser.removeTab(tab);
  is(tab.parentNode, null, "tab removed immediately");

  tab = gBrowser.addTab("about:blank", { skipAnimation: true });
  gBrowser.removeTab(tab, { animate: true });
  gBrowser.removeTab(tab);
  is(tab.parentNode, null, "tab removed immediately when calling removeTab again after the animation was kicked off");

  waitForExplicitFinish();

  Services.prefs.setBoolPref("browser.tabs.animate", true);

//  preperForNextText();
  todo(false, "async tests disabled because of intermittent failures (bug 585361)");
  cleanup();
}

function tabAdded() {
  info("tab added");
}

function cleanup() {
  if (Services.prefs.prefHasUserValue("browser.tabs.animate"))
    Services.prefs.clearUserPref("browser.tabs.animate");
  gBrowser.tabContainer.removeEventListener("TabOpen", tabAdded, false);
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
    gBrowser.selectedBrowser.focus();
    EventUtils.synthesizeKey("w", { accelKey: true });
  },
  function (tab) {
    info("closing tab by clicking the tab close button");
    gBrowser.selectedTab = tab;
    var button = document.getAnonymousElementByAttribute(tab, "anonid", "close-button");
    EventUtils.synthesizeMouse(button, 2, 2, {});
  }
];

function preperForNextText() {
  info("tests left: " + asyncTests.length + "; starting next");
  var tab = gBrowser.addTab("about:blank", { skipAnimation: true });
  executeSoon(function () {
    nextAsyncText(tab);
  });
}

function nextAsyncText(tab) {
  var gotCloseEvent = false;

  tab.addEventListener("TabClose", function () {
    tab.removeEventListener("TabClose", arguments.callee, false);
    info("got TabClose event");
    gotCloseEvent = true;

    const DEFAULT_ANIMATION_LENGTH = 250;
    const MAX_WAIT_TIME = DEFAULT_ANIMATION_LENGTH * 7;
    var polls = Math.ceil(MAX_WAIT_TIME / DEFAULT_ANIMATION_LENGTH);
    var pollTabRemoved = setInterval(function () {
      --polls;
      if (tab.parentNode && polls > 0)
        return;
      clearInterval(pollTabRemoved);

      is(tab.parentNode, null, "tab removed after at most " + MAX_WAIT_TIME + " ms");

      if (asyncTests.length)
        preperForNextText();
      else
        cleanup();
    }, DEFAULT_ANIMATION_LENGTH);
  }, false);

  asyncTests.shift()(tab);

  ok(gotCloseEvent, "got the close event syncronously");

  is(tab.parentNode, gBrowser.tabContainer, "tab still exists when it's about to be removed asynchronously");
}
