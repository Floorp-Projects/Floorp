/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test is designed to ensure that the correct command/operation happens
 * when pressing enter with various key combinations in the urlbar.
 */

const TEST_VALUE = "example.com";
const START_VALUE = "example.org";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.altClickSave", true],
      ["browser.urlbar.autoFill", false],
    ],
  });
});

add_task(async function alt_left_click_test() {
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

  await triggerCommand("click", {altKey: true});

  await saveURLPromise;
  ok(true, "SaveURL was called");
  is(gURLBar.value, "", "Urlbar reverted to original value");
});

add_task(async function shift_left_click_test() {
  info("Running test: Shift left click");

  let destinationURL = "http://" + TEST_VALUE + "/";
  let newWindowPromise = BrowserTestUtils.waitForNewWindow({url: destinationURL});
  await triggerCommand("click", {shiftKey: true});
  let win = await newWindowPromise;

  info("URL should be loaded in a new window");
  is(gURLBar.value, "", "Urlbar reverted to original value");
  await promiseCheckChildNoFocusedElement(gBrowser.selectedBrowser);
  is(document.activeElement, gBrowser.selectedBrowser, "Content window should be focused");
  is(win.gURLBar.textValue, TEST_VALUE, "New URL is loaded in new window");

  // Cleanup.
  let ourWindowRefocusedPromise = Promise.all([
    BrowserTestUtils.waitForEvent(window, "activate"),
    BrowserTestUtils.waitForEvent(window, "focus", true),
  ]);
  await BrowserTestUtils.closeWindow(win);
  await ourWindowRefocusedPromise;
});

add_task(async function right_click_test() {
  info("Running test: Right click on go button");

  // Add a new tab.
  await promiseOpenNewTab();

  await triggerCommand("click", {button: 2});

  // Right click should do nothing (context menu will be shown).
  is(gURLBar.value, TEST_VALUE, "Urlbar still has the value we entered");

  // Cleanup.
  gBrowser.removeCurrentTab();
});

add_task(async function shift_accel_left_click_test() {
  info("Running test: Shift+Ctrl/Cmd left click on go button");

  // Add a new tab.
  let tab = await promiseOpenNewTab();

  let loadStartedPromise = promiseLoadStarted();
  await triggerCommand("click", {accelKey: true, shiftKey: true});
  await loadStartedPromise;

  // Check the load occurred in a new background tab.
  info("URL should be loaded in a new background tab");
  is(gURLBar.value, "", "Urlbar reverted to original value");
  ok(!gURLBar.focused, "Urlbar is no longer focused after urlbar command");
  is(gBrowser.selectedTab, tab, "Focus did not change to the new tab");

  // Select the new background tab
  gBrowser.selectedTab = gBrowser.selectedTab.nextElementSibling;
  is(gURLBar.textValue, TEST_VALUE, "New URL is loaded in new tab");

  // Cleanup.
  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});

add_task(async function load_in_current_tab_test() {
  let tests = [
    {
      desc: "Simple return keypress",
      type: "keypress",
    },
    {
      desc: "Left click on go button",
      type: "click",
    },
    {
      desc: "Ctrl/Cmd+Return keypress",
      type: "keypress",
      details: {accelKey: true},
    },
    {
      desc: "Alt+Return keypress in a blank tab",
      type: "keypress",
      details: {altKey: true},
    },
  ];

  for (let {desc, type, details} of tests) {
    info(`Running test: ${desc}`);

    // Add a new tab.
    let tab = await promiseOpenNewTab();

    // Trigger a load and check it occurs in the current tab.
    let loadStartedPromise = promiseLoadStarted();
    await triggerCommand(type, details);
    await loadStartedPromise;

    info("URL should be loaded in the current tab");
    is(gURLBar.textValue, TEST_VALUE, "Urlbar still has the value we entered");
    await promiseCheckChildNoFocusedElement(gBrowser.selectedBrowser);
    is(document.activeElement, gBrowser.selectedBrowser, "Content window should be focused");
    is(gBrowser.selectedTab, tab, "New URL was loaded in the current tab");

    // Cleanup.
    gBrowser.removeCurrentTab();
  }
});

add_task(async function load_in_new_tab_test() {
  let tests = [
    {
      desc: "Ctrl/Cmd left click on go button",
      type: "click",
      details: {accelKey: true},
      url: "about:blank",
    },
    {
      desc: "Alt+Return keypress in a dirty tab",
      type: "keypress",
      details: {altKey: true},
      url: START_VALUE,
    },
  ];

  for (let {desc, type, details, url} of tests) {
    info(`Running test: ${desc}`);

    // Add a new tab.
    let tab = await promiseOpenNewTab(url);

    // Trigger a load and check it occurs in the current tab.
    let tabSwitchedPromise = promiseNewTabSwitched();
    await triggerCommand(type, details);
    await tabSwitchedPromise;

    // Check the load occurred in a new tab.
    info("URL should be loaded in a new focused tab");
    is(gURLBar.textValue, TEST_VALUE, "Urlbar still has the value we entered");
    await promiseCheckChildNoFocusedElement(gBrowser.selectedBrowser);
    is(document.activeElement, gBrowser.selectedBrowser, "Content window should be focused");
    isnot(gBrowser.selectedTab, tab, "New URL was loaded in a new tab");

    // Cleanup.
    gBrowser.removeCurrentTab();
    gBrowser.removeCurrentTab();
  }
});

async function triggerCommand(type, details = {}) {
  gURLBar.focus();
  gURLBar.value = "";
  EventUtils.sendString(TEST_VALUE);

  Assert.equal(await UrlbarTestUtils.promiseUserContextId(window),
               gBrowser.selectedTab.getAttribute("usercontextid"),
               "userContextId must be the same as the originating tab");

  if (type == "click") {
    ok(gURLBar.hasAttribute("usertyping"),
       "usertyping attribute must be set for the go button to be visible");
    EventUtils.synthesizeMouseAtCenter(gURLBar.goButton, details);
  } else if (type == "keypress") {
    EventUtils.synthesizeKey("KEY_Enter", details);
  } else {
    throw new Error("Unsupported event type");
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
      },
    });
  });
}

let gUserContextIdSerial = 1;
async function promiseOpenNewTab(url = "about:blank") {
  let tab = BrowserTestUtils.addTab(gBrowser, url, {userContextId: gUserContextIdSerial++});
  let tabSwitchPromise = promiseNewTabSwitched(tab);
  gBrowser.selectedTab = tab;
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await tabSwitchPromise;
  return tab;
}

function promiseNewTabSwitched() {
  return new Promise(resolve => {
    gBrowser.addEventListener("TabSwitchDone", function() {
      executeSoon(resolve);
    }, {once: true});
  });
}

function promiseCheckChildNoFocusedElement(browser) {
  if (!gMultiProcessBrowser) {
    Assert.equal(Services.focus.focusedElement, null, "There should be no focused element");
    return null;
  }

  return ContentTask.spawn(browser, { }, async function() {
    Assert.equal(Services.focus.focusedElement, null, "There should be no focused element");
  });
}
