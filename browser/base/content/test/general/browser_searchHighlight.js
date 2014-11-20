/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const PREF = "browser.search.highlightCount";

const searchBar = BrowserSearch.searchBar;
const popup = document.getElementById("PopupSearchAutoComplete");
const panel1 = document.getElementById("SearchHighlight1");
const panel2 = document.getElementById("SearchHighlight2");

let current = Services.prefs.getIntPref(PREF);
registerCleanupFunction(() => {
  Services.prefs.setIntPref(PREF, current);
});

// The highlight panel should be displayed a set number of times
add_task(function* firstrun() {
  yield promiseNewEngine(TEST_ENGINE_BASENAME);

  ok(searchBar, "got search bar");
  if (!searchBar.getAttribute("oneoffui"))
    return; // This tests the one-off UI
  searchBar.value = "foo";
  searchBar.focus();

  Services.prefs.setIntPref(PREF, 2);

  let promise = promiseWaitForEvent(panel1, "popupshown");
  EventUtils.synthesizeKey("VK_DOWN", {});
  yield promise
  ok(true, "Saw panel 1 show");

  is(Services.prefs.getIntPref(PREF), 1, "Should have counted this show");

  promise = promiseWaitForEvent(panel1, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
  ok(true, "Saw panel 1 hide");

  clearTimer();

  promise = promiseWaitForEvent(panel1, "popupshown");
  EventUtils.synthesizeKey("VK_DOWN", {});
  yield promise
  ok(true, "Saw panel 1 show");

  is(Services.prefs.getIntPref(PREF), 0, "Should have counted this show");

  promise = promiseWaitForEvent(panel1, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
  ok(true, "Saw panel 1 hide");

  clearTimer();

  function listener() {
    ok(false, "Should not have seen the pane show");
  }
  panel1.addEventListener("popupshowing", listener, false);

  promise = promiseWaitForEvent(popup, "popupshown");
  EventUtils.synthesizeKey("VK_DOWN", {});
  yield promise;
  ok(true, "Saw popup show");

  promise = promiseWaitForEvent(popup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
  ok(true, "Saw popup hide");

  panel1.removeEventListener("popupshowing", listener, false);

  clearTimer();
});

// Completing the tour should stop the popup from showing again
add_task(function* dismiss() {
  ok(searchBar, "got search bar");
  if (!searchBar.getAttribute("oneoffui"))
    return; // This tests the one-off UI
  searchBar.value = "foo";
  searchBar.focus();

  Services.prefs.setIntPref(PREF, 200);

  let promise = promiseWaitForEvent(panel1, "popupshown");
  EventUtils.synthesizeKey("VK_DOWN", {});
  yield promise
  ok(true, "Saw panel 1 show");

  promise = promiseWaitForEvent(panel2, "popupshown");
  EventUtils.synthesizeMouseAtCenter(panel1.querySelector("button"), {});
  yield promise;
  ok(true, "Saw panel 2 show");

  promise = promiseWaitForEvent(panel2, "popuphidden");
  EventUtils.synthesizeMouseAtCenter(panel2.querySelector("button"), {});
  yield promise;
  ok(true, "Saw panel 2 hide");

  is(Services.prefs.getIntPref(PREF), 0, "Should have cleared the counter");

  promise = promiseWaitForEvent(popup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
  ok(true, "Saw popup hide");

  clearTimer();

  function listener() {
    ok(false, "Should not have seen the pane show");
  }
  panel1.addEventListener("popupshowing", listener, false);

  promise = promiseWaitForEvent(popup, "popupshown");
  EventUtils.synthesizeKey("VK_DOWN", {});
  yield promise;
  ok(true, "Saw popup show");

  promise = promiseWaitForEvent(popup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
  ok(true, "Saw popup hide");

  panel1.removeEventListener("popupshowing", listener, false);

  clearTimer();
});

// The highlight panel should be re-displayed if the search popup closes and
// opens quickly
add_task(function* testRedisplay() {
  ok(searchBar, "got search bar");
  if (!searchBar.getAttribute("oneoffui"))
    return; // This tests the one-off UI
  searchBar.value = "foo";
  searchBar.focus();

  Services.prefs.setIntPref(PREF, 2);

  let promise = promiseWaitForEvent(panel1, "popupshown");
  EventUtils.synthesizeKey("VK_DOWN", {});
  yield promise
  ok(true, "Saw panel 1 show");

  is(Services.prefs.getIntPref(PREF), 1, "Should have counted this show");

  promise = promiseWaitForEvent(panel2, "popupshown");
  EventUtils.synthesizeMouseAtCenter(panel1.querySelector("button"), {});
  yield promise;
  ok(true, "Saw panel 2 show");

  promise = promiseWaitForEvent(panel2, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
  ok(true, "Saw panel 2 hide");

  promise = promiseWaitForEvent(panel2, "popupshown");
  EventUtils.synthesizeKey("VK_DOWN", {});
  yield promise;
  ok(true, "Saw panel 2 show");

  is(Services.prefs.getIntPref(PREF), 1, "Should not have counted this show");

  promise = promiseWaitForEvent(panel2, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
  ok(true, "Saw panel 2 hide");

  clearTimer();
});

// The highlight panel shouldn't be displayed if there is no text in the search
// box
add_task(function* testNoText() {
  ok(searchBar, "got search bar");
  if (!searchBar.getAttribute("oneoffui"))
    return; // This tests the one-off UI
  searchBar.value = "";
  searchBar.focus();

  Services.prefs.setIntPref(PREF, 2);

  function listener() {
    ok(false, "Should not have seen the pane show");
  }
  panel1.addEventListener("popupshowing", listener, false);

  let button = document.getAnonymousElementByAttribute(searchBar,
                                                       "anonid",
                                                       "searchbar-search-button");
  let promise = promiseWaitForEvent(popup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(button, {});
  yield promise;
  ok(true, "Saw popup show");

  promise = promiseWaitForEvent(popup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
  ok(true, "Saw popup hide");

  clearTimer();

  promise = promiseWaitForEvent(popup, "popupshown");
  EventUtils.synthesizeKey("VK_DOWN", {});
  yield promise;
  ok(true, "Saw popup show");

  promise = promiseWaitForEvent(popup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
  ok(true, "Saw popup hide");

  panel1.removeEventListener("popupshowing", listener, false);

  clearTimer();
});

function clearTimer() {
  // Clear the timeout
  clearTimeout(SearchHighlight.hideTimer);
  SearchHighlight.hideTimer = null;
}

function promiseWaitForEvent(node, type, capturing) {
  return new Promise((resolve) => {
    node.addEventListener(type, function listener(event) {
      node.removeEventListener(type, listener, capturing);
      resolve(event);
    }, capturing);
  });
}

function promiseNewEngine(basename) {
  return new Promise((resolve, reject) => {
    info("Waiting for engine to be added: " + basename);
    Services.search.init({
      onInitComplete: function() {
        let url = getRootDirectory(gTestPath) + basename;
        let current = Services.search.currentEngine;
        Services.search.addEngine(url, Ci.nsISearchEngine.TYPE_MOZSEARCH, "", false, {
          onSuccess: function (engine) {
            info("Search engine added: " + basename);
            Services.search.currentEngine = engine;
            registerCleanupFunction(() => {
              Services.search.currentEngine = current;
              Services.search.removeEngine(engine);
            });
            resolve(engine);
          },
          onError: function (errCode) {
            ok(false, "addEngine failed with error code " + errCode);
            reject();
          },
        });
      }
    });
  });
}
