/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = ROOT + "browser_form_restore_events_sample.html";

/**
 * Originally a test for Bug 476161, but then expanded to include all input
 * types in bug 640136.
 */
add_task(function () {
  // Load a page with some form elements.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // text fields
  yield setInputValue(browser, {id: "modify01", value: Math.random()});
  yield setInputValue(browser, {id: "modify02", value: Date.now()});

  // textareas
  yield setInputValue(browser, {id: "modify03", value: Math.random()});
  yield setInputValue(browser, {id: "modify04", value: Date.now()});

  // file
  let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
  yield setInputValue(browser, {id: "modify05", value: file.path});

  // select
  yield setSelectedIndex(browser, {id: "modify06", index: 1});
  yield setMultipleSelected(browser, {id: "modify07", indices: [0,1,2]});

  // checkbox
  yield setInputChecked(browser, {id: "modify08", checked: true});
  yield setInputChecked(browser, {id: "modify09", checked: false});

  // radio
  yield setInputChecked(browser, {id: "modify10", checked: true});
  yield setInputChecked(browser, {id: "modify11", checked: true});

  // Duplicate the tab and check that restoring form data yields the expected
  // input and change events for modified form fields.
  let tab2 = gBrowser.duplicateTab(tab);
  let browser2 = tab2.linkedBrowser;
  yield promiseTabRestored(tab2);

  let inputFired = yield getTextContent(browser2, {id: "inputFired"});
  inputFired = inputFired.trim().split().sort().join(" ");

  let changeFired = yield getTextContent(browser2, {id: "changeFired"});
  changeFired = changeFired.trim().split().sort().join(" ");

  is(inputFired, "modify01 modify02 modify03 modify04 modify05",
     "input events were only dispatched for modified input, textarea fields");

  is(changeFired, "modify06 modify07 modify08 modify09 modify11",
     "change events were only dispatched for modified select, checkbox, radio fields");

  // Cleanup.
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab);
});
