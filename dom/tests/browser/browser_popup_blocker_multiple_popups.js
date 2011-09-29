/**
 * In this test, we check that the content can't open more than one popup at a
 * time (depending on "dom.allow_mulitple_popups" preference value).
 */

let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
           .getService(Ci.nsIWindowMediator);
let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"]
           .getService(Ci.nsIWindowWatcher);
let prefs = Cc["@mozilla.org/preferences-service;1"]
              .getService(Components.interfaces.nsIPrefBranch);
let gMultiplePopupsPref;

let gCurrentTest = -1;
let gTests = [
  test1,
  test2,
  test3,
  test4,
  test5,
  test6,
];

function cleanUpPopups()
{
  let windowEnumerator = wm.getEnumerator(null);

  while (windowEnumerator.hasMoreElements()) {
    let win = windowEnumerator.getNext();

    // Close all windows except ourself and the browser test harness window.
    if (win != window && !win.closed &&
        win.document.documentElement.getAttribute("id") != "browserTestHarness") {
      win.close();
    }
  }
}

function cleanUp()
{
  prefs.setBoolPref("dom.block_multiple_popups", gMultiplePopupsPref);
  cleanUpPopups(window);
}

function nextOrFinish()
{
  gCurrentTest++;

  if (gCurrentTest >= gTests.length) {
    finish();
    return;
  }

  gTests[gCurrentTest]();
}

function test()
{
  waitForExplicitFinish();

  gMultiplePopupsPref = prefs.getBoolPref("dom.block_multiple_popups");

  nextOrFinish();
}

/**
 * Two window.open();
 */
function test1()
{
  prefs.setBoolPref("dom.block_multiple_popups", true);

  gBrowser.selectedTab.linkedBrowser.addEventListener("load", function () {
    gBrowser.selectedTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    gBrowser.addEventListener("DOMPopupBlocked", function() {
      gBrowser.removeEventListener("DOMPopupBlocked", arguments.callee, true);

      ok(true, "The popup has been blocked");

      cleanUp();
      nextOrFinish();
    }, true);

    waitForFocus(function() {
      var button = gBrowser.selectedTab.linkedBrowser.contentDocument.getElementsByTagName('button')[0];
      EventUtils.synthesizeMouseAtCenter(button, {}, gBrowser.selectedTab.linkedBrowser.contentWindow);
    });
  }, true);

  gBrowser.selectedTab.linkedBrowser.loadURI("data:text/html,<!DOCTYPE html><html><body><script>function openPopups() { window.open('data:text/html,foo', '', 'foo'); window.open('data:text/html,bar', '', 'foo'); }</script><button onclick='openPopups();'>click</button></body></html>");
}

/**
 * window.open followed by w.open with w being the first popup.
 */
function test2()
{
  prefs.setBoolPref("dom.block_multiple_popups", true);

  let gPopupsCount = 0;

  gBrowser.selectedTab.linkedBrowser.addEventListener("load", function () {
    gBrowser.selectedTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    ww.registerNotification(function(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened") {
        return;
      }

      gPopupsCount++;

      if (gPopupsCount > 1) {
        return;
      }

      executeSoon(function() {
      executeSoon(function() {
      executeSoon(function() {
      executeSoon(function() {
        ww.unregisterNotification(arguments.callee);

        is(gPopupsCount, 1, "Only one popup appeared");
        cleanUp();
        nextOrFinish();
      });
      });
      });
      });
    });

    waitForFocus(function() {
      var button = gBrowser.selectedTab.linkedBrowser.contentDocument.getElementsByTagName('button')[0];
      EventUtils.synthesizeMouseAtCenter(button, {}, gBrowser.selectedTab.linkedBrowser.contentWindow);
    });
  }, true);

  gBrowser.selectedTab.linkedBrowser.loadURI("data:text/html,<!DOCTYPE html><html><body><script>function openPopups() { var w = window.open('data:text/html,foo', '', 'foo'); w.open('data:text/html,bar', '', 'foo'); }</script><button onclick='openPopups();'>click</button></body></html>");
}

/**
 * window.open followed by w.open with w being the first popup and the second popup being actually a tab.
 */
function test3()
{
  prefs.setBoolPref("dom.block_multiple_popups", true);

  let gPopupsCount = 0;

  gBrowser.selectedTab.linkedBrowser.addEventListener("load", function () {
    gBrowser.selectedTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    ww.registerNotification(function(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened") {
        return;
      }

      gPopupsCount++;

      if (gPopupsCount > 1) {
        return;
      }

      executeSoon(function() {
      executeSoon(function() {
      executeSoon(function() {
      executeSoon(function() {
        ww.unregisterNotification(arguments.callee);

        is(gPopupsCount, 1, "Only one popup appeared");
        cleanUp();
        nextOrFinish();
      });
      });
      });
      });
    });

    waitForFocus(function() {
      var button = gBrowser.selectedTab.linkedBrowser.contentDocument.getElementsByTagName('button')[0];
      EventUtils.synthesizeMouseAtCenter(button, {}, gBrowser.selectedTab.linkedBrowser.contentWindow);
    });
  }, true);

  gBrowser.selectedTab.linkedBrowser.loadURI("data:text/html,<!DOCTYPE html><html><body><script>function openPopups() { var w = window.open('data:text/html,foo', '', 'foo'); w.open('data:text/html,bar', '', ''); }</script><button onclick='openPopups();'>click</button></body></html>");
}

/**
 * window.open and .click() on the element opening the window.
 */
function test4()
{
  prefs.setBoolPref("dom.block_multiple_popups", true);

  gBrowser.selectedTab.linkedBrowser.addEventListener("load", function () {
    gBrowser.selectedTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    gBrowser.addEventListener("DOMPopupBlocked", function() {
      gBrowser.removeEventListener("DOMPopupBlocked", arguments.callee, true);

      ok(true, "The popup has been blocked");

      cleanUp();
      nextOrFinish();
    }, true);

    waitForFocus(function() {
      var button = gBrowser.selectedTab.linkedBrowser.contentDocument.getElementsByTagName('button')[0];
      EventUtils.synthesizeMouseAtCenter(button, {}, gBrowser.selectedTab.linkedBrowser.contentWindow);
    });
  }, true);

  gBrowser.selectedTab.linkedBrowser.loadURI("data:text/html,<!DOCTYPE html><html><body><script>var r = false; function openPopups() { window.open('data:text/html,foo', '', 'foo'); if (!r) { document.getElementsByTagName('button')[0].click(); r=true; } }</script><button onclick='openPopups();'>click</button></body></html>");
}

/**
 * Two window.open from the chrome.
 */
function test5()
{
  prefs.setBoolPref("dom.block_multiple_popups", true);

  let gPopupsCount = 0;

  ww.registerNotification(function(aSubject, aTopic, aData) {
    if (aTopic != "domwindowopened") {
      return;
    }

    gPopupsCount++;

    if (gPopupsCount != 2) {
      return;
    }

    executeSoon(function() {
    executeSoon(function() {
    executeSoon(function() {
    executeSoon(function() {
      ww.unregisterNotification(arguments.callee);

      is(gPopupsCount, 2, "Both window appeared");
      cleanUp();
      nextOrFinish();
    });
    });
    });
    });
  });

  window.open("data:text/html,foo", '', 'foo');
  window.open("data:text/html,foo", '', 'foo');
}

/**
 * Two window.open with the pref being disabled.
 */
function test6()
{
  prefs.setBoolPref("dom.block_multiple_popups", false);

  let gPopupsCount = 0;

  gBrowser.selectedTab.linkedBrowser.addEventListener("load", function () {
    gBrowser.selectedTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    ww.registerNotification(function(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened") {
        return;
      }

      gPopupsCount++;

      if (gPopupsCount != 2) {
        return;
      }

      executeSoon(function() {
      executeSoon(function() {
      executeSoon(function() {
      executeSoon(function() {
        ww.unregisterNotification(arguments.callee);

        is(gPopupsCount, 2, "Both window appeared");
        cleanUp();
        nextOrFinish();
      });
      });
      });
      });
    });

    waitForFocus(function() {
      var button = gBrowser.selectedTab.linkedBrowser.contentDocument.getElementsByTagName('button')[0];
      EventUtils.synthesizeMouseAtCenter(button, {}, gBrowser.selectedTab.linkedBrowser.contentWindow);
    });
  }, true);

  gBrowser.selectedTab.linkedBrowser.loadURI("data:text/html,<!DOCTYPE html><html><body><script>function openPopups() { window.open('data:text/html,foo', '', 'foo'); window.open('data:text/html,bar', '', 'foo'); }</script><button onclick='openPopups();'>click</button></body></html>");
}
