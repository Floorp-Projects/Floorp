/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function is_hidden(element) {
  var style = element.ownerDocument.defaultView.getComputedStyle(element, "");
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;
  if (style.display == "-moz-popup")
    return ["hiding","closed"].indexOf(element.state) != -1;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_hidden(element.parentNode);

  return false;
}

function is_element_visible(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(!is_hidden(element), msg);
}

function waitForElementToBeVisible(element, nextTest, msg) {
  waitForCondition(() => !is_hidden(element),
                   () => {
                     ok(true, msg);
                     nextTest();
                   },
                   "Timeout waiting for visibility: " + msg);
}

function waitForElementToBeHidden(element, nextTest, msg) {
  waitForCondition(() => is_hidden(element),
                   () => {
                     ok(true, msg);
                     nextTest();
                   },
                   "Timeout waiting for invisibility: " + msg);
}

function waitForPopupAtAnchor(popup, anchorNode, nextTest, msg) {
  waitForCondition(() => popup.popupBoxObject.anchorNode == anchorNode,
                   () => {
                     ok(true, msg);
                     is_element_visible(popup, "Popup should be visible");
                     nextTest();
                   },
                   "Timeout waiting for popup at anchor: " + msg);
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(is_hidden(element), msg);
}

function loadUITourTestPage(callback, host = "https://example.com/") {
   if (gTestTab)
    gBrowser.removeTab(gTestTab);

  let url = getRootDirectory(gTestPath) + "uitour.html";
  url = url.replace("chrome://mochitests/content/", host);

  gTestTab = gBrowser.addTab(url);
  gBrowser.selectedTab = gTestTab;

  gTestTab.linkedBrowser.addEventListener("load", function onLoad() {
    gTestTab.linkedBrowser.removeEventListener("load", onLoad);

    gContentWindow = Components.utils.waiveXrays(gTestTab.linkedBrowser.contentDocument.defaultView);
    gContentAPI = gContentWindow.Mozilla.UITour;

    waitForFocus(callback, gContentWindow);
  }, true);
}

function UITourTest() {
  Services.prefs.setBoolPref("browser.uitour.enabled", true);
  let testUri = Services.io.newURI("http://example.com", null, null);
  Services.perms.add(testUri, "uitour", Services.perms.ALLOW_ACTION);

  waitForExplicitFinish();

  registerCleanupFunction(function() {
    delete window.UITour;
    delete window.gContentWindow;
    delete window.gContentAPI;
    if (gTestTab)
      gBrowser.removeTab(gTestTab);
    delete window.gTestTab;
    Services.prefs.clearUserPref("browser.uitour.enabled", true);
    Services.perms.remove("example.com", "uitour");
  });

  function done() {
    executeSoon(() => {
      if (gTestTab)
        gBrowser.removeTab(gTestTab);
      gTestTab = null;

      let highlight = document.getElementById("UITourHighlightContainer");
      is_element_hidden(highlight, "Highlight should be closed/hidden after UITour tab is closed");

      let tooltip = document.getElementById("UITourTooltip");
      is_element_hidden(tooltip, "Tooltip should be closed/hidden after UITour tab is closed");

      ok(!PanelUI.panel.hasAttribute("noautohide"), "@noautohide on the menu panel should have been cleaned up");

      is(UITour.pinnedTabs.get(window), null, "Any pinned tab should be closed after UITour tab is closed");

      executeSoon(nextTest);
    });
  }

  function nextTest() {
    if (tests.length == 0) {
      finish();
      return;
    }
    let test = tests.shift();
    info("Starting " + test.name);
    loadUITourTestPage(function() {
      test(done);
    });
  }
  nextTest();
}
