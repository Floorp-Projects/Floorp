/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["UrlbarTestUtils"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
});

var UrlbarTestUtils = {
  promiseSearchComplete(win, dontAnimate = false) {
    return BrowserTestUtils.waitForPopupEvent(win.gURLBar.popup, "shown").then(() => {
      function searchIsComplete() {
        let isComplete = win.gURLBar.controller.searchStatus >=
                         Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH;
        if (isComplete) {
          dump(`Restore popup dontAnimate value to ${dontAnimate}\n`);
          win.gURLBar.popup.setAttribute("dontanimate", dontAnimate);
        }
        return isComplete;
      }

      // Wait until there are at least two matches.
      return BrowserTestUtils.waitForCondition(searchIsComplete, "waiting urlbar search to complete");
    });
  },

  promiseAutocompleteResultPopup(inputText, win, waitForFocus, fireInputEvent = false) {
    let dontAnimate = !!win.gURLBar.popup.getAttribute("dontanimate");
    waitForFocus(() => {
      dump(`Disable popup animation. Change dontAnimate value from ${dontAnimate} to true.\n`);
      win.gURLBar.popup.setAttribute("dontanimate", "true");
      win.gURLBar.focus();
      win.gURLBar.value = inputText;
      if (fireInputEvent) {
        // This is necessary to get the urlbar to set gBrowser.userTypedValue.
        let event = win.document.createEvent("Events");
        event.initEvent("input", true, true);
        win.gURLBar.dispatchEvent(event);
      }
      win.gURLBar.controller.startSearch(inputText);
    }, win);

    return this.promiseSearchComplete(win, dontAnimate);
  },

  async waitForAutocompleteResultAt(win, index) {
    let searchString = win.gURLBar.controller.searchString;
    await BrowserTestUtils.waitForCondition(
      () => win.gURLBar.popup.richlistbox.itemChildren.length > index &&
            win.gURLBar.popup.richlistbox.itemChildren[index].getAttribute("ac-text") == searchString.trim(),
      `Waiting for the autocomplete result for "${searchString}" at [${index}] to appear`);
    // Ensure the addition is complete, for proper mouse events on the entries.
    await new Promise(resolve => win.requestIdleCallback(resolve, {timeout: 1000}));
    return win.gURLBar.popup.richlistbox.itemChildren[index];
  },

  promiseSuggestionsPresent(win, msg = "") {
    return TestUtils.waitForCondition(this.suggestionsPresent.bind(this, win),
                                      msg || "Waiting for suggestions");
  },

  suggestionsPresent(win) {
    let controller = win.gURLBar.popup.input.controller;
    let matchCount = controller.matchCount;
    for (let i = 0; i < matchCount; i++) {
      let url = controller.getValueAt(i);
      let mozActionMatch = url.match(/^moz-action:([^,]+),(.*)$/);
      if (mozActionMatch) {
        let [, type, paramStr] = mozActionMatch;
        let params = JSON.parse(paramStr);
        if (type == "searchengine" && "searchSuggestion" in params) {
          return true;
        }
      }
    }
    return false;
  },

  promiseSpeculativeConnection(httpserver) {
    return BrowserTestUtils.waitForCondition(() => {
      if (httpserver) {
        return httpserver.connectionNumber == 1;
      }
      return false;
    }, "Waiting for connection setup");
  },
};
