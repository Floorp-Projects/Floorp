/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for the docshell active state of local and remote browsers.

const kTestPage = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";

function promiseNewTabSwitched() {
  return new Promise(resolve => {
    gBrowser.addEventListener("TabSwitchDone", function onSwitch() {
      gBrowser.removeEventListener("TabSwitchDone", onSwitch);
      executeSoon(resolve);
    });
  });
}

function getParentTabState(aTab) {
  return aTab.linkedBrowser.docShellIsActive;
}

function getChildTabState(aTab) {
  return ContentTask.spawn(aTab.linkedBrowser, {}, function* () {
    return docShell.isActive;
  });
}

function checkState(parentSide, childSide, value, message) {
  is(parentSide, value, message + " (parent side)");
  is(childSide, value, message + " (child side)");
}

function waitForMs(aMs) {
  return new Promise((resolve) => {
    setTimeout(done, aMs);
    function done() {
      resolve(true);
    }
  });
}

add_task(function *() {
  let url = kTestPage;
  let originalTab = gBrowser.selectedTab; // test tab
  let newTab = gBrowser.addTab(url, {skipAnimation: true});
  let parentSide, childSide;

  // new tab added but not selected checks
  parentSide = getParentTabState(newTab);
  childSide = yield getChildTabState(newTab);
  checkState(parentSide, childSide, false, "newly added " + url + " tab is not active");
  parentSide = getParentTabState(originalTab);
  childSide = yield getChildTabState(originalTab);
  checkState(parentSide, childSide, true, "original tab is active initially");

  // select the newly added tab and wait  for TabSwitchDone event
  let tabSwitchedPromise = promiseNewTabSwitched();
  gBrowser.selectedTab = newTab;
  yield tabSwitchedPromise;

  if (Services.appinfo.browserTabsRemoteAutostart) {
    ok(newTab.linkedBrowser.isRemoteBrowser, "for testing we need a remote tab");
  }

  // check active state of both tabs
  parentSide = getParentTabState(newTab);
  childSide = yield getChildTabState(newTab);
  checkState(parentSide, childSide, true, "newly added " + url + " tab is active after selection");
  parentSide = getParentTabState(originalTab);
  childSide = yield getChildTabState(originalTab);
  checkState(parentSide, childSide, false, "original tab is not active while unselected");

  // switch back to the original test tab and wait for TabSwitchDone event
  tabSwitchedPromise = promiseNewTabSwitched();
  gBrowser.selectedTab = originalTab;
  yield tabSwitchedPromise;

  // check active state of both tabs
  parentSide = getParentTabState(newTab);
  childSide = yield getChildTabState(newTab);
  checkState(parentSide, childSide, false, "newly added " + url + " tab is not active after switch back");
  parentSide = getParentTabState(originalTab);
  childSide = yield getChildTabState(originalTab);
  checkState(parentSide, childSide, true, "original tab is active again after switch back");

  // switch to the new tab and wait for TabSwitchDone event
  tabSwitchedPromise = promiseNewTabSwitched();
  gBrowser.selectedTab = newTab;
  yield tabSwitchedPromise;

  // check active state of both tabs
  parentSide = getParentTabState(newTab);
  childSide = yield getChildTabState(newTab);
  checkState(parentSide, childSide, true, "newly added " + url + " tab is not active after switch back");
  parentSide = getParentTabState(originalTab);
  childSide = yield getChildTabState(originalTab);
  checkState(parentSide, childSide, false, "original tab is active again after switch back");

  gBrowser.removeTab(newTab);
});

add_task(function *() {
  let url = "about:about";
  let originalTab = gBrowser.selectedTab; // test tab
  let newTab = gBrowser.addTab(url, {skipAnimation: true});
  let parentSide, childSide;

  parentSide = getParentTabState(newTab);
  childSide = yield getChildTabState(newTab);
  checkState(parentSide, childSide, false, "newly added " + url + " tab is not active");
  parentSide = getParentTabState(originalTab);
  childSide = yield getChildTabState(originalTab);
  checkState(parentSide, childSide, true, "original tab is active initially");

  let tabSwitchedPromise = promiseNewTabSwitched();
  gBrowser.selectedTab = newTab;
  yield tabSwitchedPromise;

  if (Services.appinfo.browserTabsRemoteAutostart) {
    ok(!newTab.linkedBrowser.isRemoteBrowser, "for testing we need a local tab");
  }

  parentSide = getParentTabState(newTab);
  childSide = yield getChildTabState(newTab);
  checkState(parentSide, childSide, true, "newly added " + url + " tab is active after selection");
  parentSide = getParentTabState(originalTab);
  childSide = yield getChildTabState(originalTab);
  checkState(parentSide, childSide, false, "original tab is not active while unselected");

  tabSwitchedPromise = promiseNewTabSwitched();
  gBrowser.selectedTab = originalTab;
  yield tabSwitchedPromise;

  parentSide = getParentTabState(newTab);
  childSide = yield getChildTabState(newTab);
  checkState(parentSide, childSide, false, "newly added " + url + " tab is not active after switch back");
  parentSide = getParentTabState(originalTab);
  childSide = yield getChildTabState(originalTab);
  checkState(parentSide, childSide, true, "original tab is active again after switch back");

  tabSwitchedPromise = promiseNewTabSwitched();
  gBrowser.selectedTab = newTab;
  yield tabSwitchedPromise;

  parentSide = getParentTabState(newTab);
  childSide = yield getChildTabState(newTab);
  checkState(parentSide, childSide, true, "newly added " + url + " tab is not active after switch back");
  parentSide = getParentTabState(originalTab);
  childSide = yield getChildTabState(originalTab);
  checkState(parentSide, childSide, false, "original tab is active again after switch back");

  gBrowser.removeTab(newTab);
});
