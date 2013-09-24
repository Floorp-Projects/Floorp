/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let newTab = gBrowser.selectedTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  registerCleanupFunction(function () {
    gBrowser.removeTab(newTab);
  });

  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
  let ChromeUtils = {};
  scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

  let tabContainer = gBrowser.tabContainer;
  var receivedDropCount = 0;
  function dropListener() {
    receivedDropCount++;
    if (receivedDropCount == triggeredDropCount) {
      is(openedTabs, validDropCount, "correct number of tabs were opened");
      executeSoon(finish);
    }
  }
  tabContainer.addEventListener("drop", dropListener, false);
  registerCleanupFunction(function () {
    tabContainer.removeEventListener("drop", dropListener, false);
  });

  var openedTabs = 0;
  function tabOpenListener(e) {
    openedTabs++;
    let tab = e.target;
    executeSoon(function () {
      gBrowser.removeTab(tab);
    });
  }

  tabContainer.addEventListener("TabOpen", tabOpenListener, false);
  registerCleanupFunction(function () {
    tabContainer.removeEventListener("TabOpen", tabOpenListener, false);
  });

  var triggeredDropCount = 0;
  var validDropCount = 0;
  function drop(text, valid) {
    triggeredDropCount++;
    if (valid)
      validDropCount++;
    executeSoon(function () {
      // A drop type of "link" onto an existing tab would normally trigger a
      // load in that same tab, but tabbrowser code in _getDragTargetTab treats
      // drops on the outer edges of a tab differently (loading a new tab
      // instead). The events created by synthesizeDrop have all of their
      // coordinates set to 0 (screenX/screenY), so they're treated as drops
      // on the outer edge of the tab, thus they open new tabs.
      ChromeUtils.synthesizeDrop(newTab, newTab, [[{type: "text/plain", data: text}]], "link", window);
    });
  }

  // Begin and end with valid drops to make sure we wait for all drops before
  // ending the test
  drop("mochi.test/first", true);
  drop("javascript:'bad'");
  drop("jAvascript:'bad'");
  drop("search this", true);
  drop("mochi.test/second", true);
  drop("data:text/html,bad");
  drop("mochi.test/third", true);
}
