// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function clearFormHistory() {
  FormHistory.update({ op : "remove" });
}

function test() {
  runTests();
  clearFormHistory();
}

function setUp() {
  clearFormHistory();
  PanelUI.hide();
  yield hideContextUI();
}

function tearDown() {
  PanelUI.hide();
}

function checkAutofillMenuItemContents(aItemList)
{
  let errors = 0;
  let found = 0;
  for (let idx = 0; idx < AutofillMenuUI.commands.childNodes.length; idx++) {
    let item = AutofillMenuUI.commands.childNodes[idx];
    let label = item.firstChild.getAttribute("value");
    let value = item.getAttribute("data");
    if (aItemList.indexOf(value) == -1) {
      errors++;
      info("unexpected entry:" + value);
    } else {
      found++;
    }
  }
  is(errors, 0, "autofill menu item list error check");
  is(found, aItemList.length, "autofill item list length mismatch, some items were not found.");
}

gTests.push({
  desc: "simple auto complete test to insure auto complete code doesn't break.",
  setUp: setUp,
  tearDown: tearDown,
  run: function () {
    let loadedPromise, shownPromise;

    yield addTab(chromeRoot + "browser_form_auto_complete.html");
    yield waitForCondition(function () {
      return !Browser.selectedTab.isLoading();
    });

    let tabDocument = Browser.selectedTab.browser.contentWindow.document;
    let form = tabDocument.getElementById("form1");
    let input = tabDocument.getElementById("textedit1");

    input.value = "hellothere";

    loadedPromise = waitForObserver("satchel-storage-changed", null, "formhistory-add");
    form.submit();
    yield loadedPromise;

    // XXX Solves a problem with events not getting delivered to Content.js
    // immediately after submitting the form.
    yield waitForMs(500);

    tabDocument = Browser.selectedTab.browser.contentWindow.document;
    input = tabDocument.getElementById("textedit1");
    ok(input, "input isn't null");
    input.focus();

    // Desktop and metrofx display auto-completes in response to double mouse clicks. The
    // first click is ignored.
    shownPromise = waitForEvent(document, "popupshown");
    EventUtils.synthesizeMouseAtCenter(input, {}, Browser.selectedTab.browser.contentWindow);
    EventUtils.synthesizeMouseAtCenter(input, {}, Browser.selectedTab.browser.contentWindow);
    yield shownPromise;

    checkAutofillMenuItemContents(["hellothere", "one", "two", "three", "four", "five"]);
  }
});

gTests.push({
  desc: "Test autocomplete selection with arrow key.",
  setUp: setUp,
  tearDown: tearDown,
  run: function () {

    let newTab = yield addTab(chromeRoot + "browser_form_auto_complete.html");
    yield waitForCondition(function () {
      return !Browser.selectedTab.isLoading();
    });

    let tabDocument = newTab.browser.contentWindow.document;
    let input = tabDocument.getElementById("textedit1");
    input.focus();

    let shownPromise = waitForEvent(document, "popupshown");
    EventUtils.synthesizeKey("o", {}, window);
    yield shownPromise;

    EventUtils.synthesizeKey("VK_DOWN", {}, window);

    yield waitForCondition(() => input.value == "one");

    is(input.value, "one", "Input updated correctly");

    EventUtils.synthesizeKey("VK_DOWN", {}, window);

    yield waitForCondition(() => input.value == "two");

    is(input.value, "two", "Input updated correctly");

    Browser.closeTab(newTab, { forceClose: true });
  }
});
