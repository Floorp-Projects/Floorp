/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_VALUE = "example.com";
const START_VALUE = "example.org";

add_task(function* setup() {
  Services.prefs.setBoolPref("browser.altClickSave", true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.altClickSave");
  });
});

add_task(function* alt_left_click_test() {
  info("Running test: Alt left click");

  // Monkey patch saveURL() to avoid dealing with file save code paths.
  let oldSaveURL = saveURL;
  let saveURLPromise = new Promise(resolve => {
    saveURL = () => {
      // Restore old saveURL() value.
      saveURL = oldSaveURL;
      resolve();
    };
  });

  triggerCommand(true, {altKey: true});

  yield saveURLPromise;
  ok(true, "SaveURL was called");
  is(gURLBar.value, "", "Urlbar reverted to original value");
});

add_task(function* shift_left_click_test() {
  info("Running test: Shift left click");

  let newWindowPromise = promiseWaitForNewWindow();
  triggerCommand(true, {shiftKey: true});
  let win = yield newWindowPromise;

  // Wait for the initial browser to load.
  let browser = win.gBrowser.selectedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);

  info("URL should be loaded in a new window");
  is(gURLBar.value, "", "Urlbar reverted to original value");
  is(Services.focus.focusedElement, null, "There should be no focused element");
  is(Services.focus.focusedWindow, win.gBrowser.contentWindow, "Content window should be focused");
  is(win.gURLBar.textValue, TEST_VALUE, "New URL is loaded in new window");

  // Cleanup.
  yield promiseWindowClosed(win);
});

add_task(function* right_click_test() {
  info("Running test: Right click on go button");

  // Add a new tab.
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  triggerCommand(true, {button: 2});

  // Right click should do nothing (context menu will be shown).
  is(gURLBar.value, TEST_VALUE, "Urlbar still has the value we entered");

  // Cleanup.
  gBrowser.removeCurrentTab();
});

add_task(function* shift_accel_left_click_test() {
  info("Running test: Shift+Ctrl/Cmd left click on go button");

  // Add a new tab.
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let loadStartedPromise = promiseLoadStarted();
  triggerCommand(true, {accelKey: true, shiftKey: true});
  yield loadStartedPromise;

  // Check the load occurred in a new background tab.
  info("URL should be loaded in a new background tab");
  is(gURLBar.value, "", "Urlbar reverted to original value");
  ok(!gURLBar.focused, "Urlbar is no longer focused after urlbar command");
  is(gBrowser.selectedTab, tab, "Focus did not change to the new tab");

  // Select the new background tab
  gBrowser.selectedTab = gBrowser.selectedTab.nextSibling;
  is(gURLBar.value, TEST_VALUE, "New URL is loaded in new tab");

  // Cleanup.
  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});

add_task(function* load_in_current_tab_test() {
  let tests = [
    {desc: "Simple return keypress"},
    {desc: "Left click on go button", click: true},
    {desc: "Ctrl/Cmd+Return keypress", event: {accelKey: true}},
    {desc: "Alt+Return keypress in a blank tab", event: {altKey: true}}
  ];

  for (let test of tests) {
    info(`Running test: ${test.desc}`);

    // Add a new tab.
    let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
    yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    // Trigger a load and check it occurs in the current tab.
    let loadStartedPromise = promiseLoadStarted();
    triggerCommand(test.click || false, test.event || {});
    yield loadStartedPromise;

    info("URL should be loaded in the current tab");
    is(gURLBar.value, TEST_VALUE, "Urlbar still has the value we entered");
    is(Services.focus.focusedElement, null, "There should be no focused element");
    is(Services.focus.focusedWindow, gBrowser.contentWindow, "Content window should be focused");
    is(gBrowser.selectedTab, tab, "New URL was loaded in the current tab");

    // Cleanup.
    gBrowser.removeCurrentTab();
  }
});

add_task(function* load_in_new_tab_test() {
  let tests = [
    {desc: "Ctrl/Cmd left click on go button", click: true, event: {accelKey: true}},
    {desc: "Alt+Return keypress in a dirty tab", event: {altKey: true}, url: START_VALUE}
  ];

  for (let test of tests) {
    info(`Running test: ${test.desc}`);

    // Add a new tab.
    let tab = gBrowser.selectedTab = gBrowser.addTab(test.url || "about:blank");
    yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    // Trigger a load and check it occurs in the current tab.
    let tabSelectedPromise = promiseNewTabSelected();
    triggerCommand(test.click || false, test.event || {});
    yield tabSelectedPromise;

    // Check the load occurred in a new tab.
    info("URL should be loaded in a new focused tab");
    is(gURLBar.value, TEST_VALUE, "Urlbar still has the value we entered");
    is(Services.focus.focusedElement, null, "There should be no focused element");
    is(Services.focus.focusedWindow, gBrowser.contentWindow, "Content window should be focused");
    isnot(gBrowser.selectedTab, tab, "New URL was loaded in a new tab");

    // Cleanup.
    gBrowser.removeCurrentTab();
    gBrowser.removeCurrentTab();
  }
});

function triggerCommand(shouldClick, event) {
  gURLBar.value = TEST_VALUE;
  gURLBar.focus();

  if (shouldClick) {
    is(gURLBar.getAttribute("pageproxystate"), "invalid",
       "page proxy state must be invalid for go button to be visible");

    let goButton = document.getElementById("urlbar-go-button");
    EventUtils.synthesizeMouseAtCenter(goButton, event);
  } else {
    EventUtils.synthesizeKey("VK_RETURN", event);
  }
}

function promiseLoadStarted() {
  return new Promise(resolve => {
    gBrowser.addTabsProgressListener({
      onStateChange(browser, webProgress, req, flags, status) {
        if (flags & Ci.nsIWebProgressListener.STATE_START) {
          gBrowser.removeTabsProgressListener(this);
          resolve();
        }
      }
    });
  });
}

function promiseNewTabSelected() {
  return new Promise(resolve => {
    gBrowser.tabContainer.addEventListener("TabSelect", function onSelect() {
      gBrowser.tabContainer.removeEventListener("TabSelect", onSelect);
      resolve();
    });
  });
}

function promiseWaitForNewWindow() {
  return new Promise(resolve => {
    let listener = {
      onOpenWindow(xulWindow) {
        let win = xulWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindow);

        Services.wm.removeListener(listener);
        whenDelayedStartupFinished(win, () => resolve(win));
      },

      onCloseWindow() {},
      onWindowTitleChange() {}
    };

    Services.wm.addListener(listener);
  });
}
