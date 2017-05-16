/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = ROOT + "browser_form_restore_events_sample.html";

/**
 * Originally a test for Bug 476161, but then expanded to include all input
 * types in bug 640136.
 */
add_task(async function() {
  // Load a page with some form elements.
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // text fields
  await setInputValue(browser, {id: "modify01", value: Math.random()});
  await setInputValue(browser, {id: "modify02", value: Date.now()});

  // textareas
  await setInputValue(browser, {id: "modify03", value: Math.random()});
  await setInputValue(browser, {id: "modify04", value: Date.now()});

  // file
  let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
  await setInputValue(browser, {id: "modify05", value: file.path});

  // select
  await setSelectedIndex(browser, {id: "modify06", index: 1});
  await setMultipleSelected(browser, {id: "modify07", indices: [0, 1, 2]});

  // checkbox
  await setInputChecked(browser, {id: "modify08", checked: true});
  await setInputChecked(browser, {id: "modify09", checked: false});

  // radio
  await setInputChecked(browser, {id: "modify10", checked: true});
  await setInputChecked(browser, {id: "modify11", checked: true});

  // Duplicate the tab and check that restoring form data yields the expected
  // input and change events for modified form fields.
  let tab2 = gBrowser.duplicateTab(tab);
  let browser2 = tab2.linkedBrowser;
  await promiseTabRestored(tab2);

  let inputFired = await getTextContent(browser2, {id: "inputFired"});
  inputFired = inputFired.trim().split().sort().join(" ");

  let changeFired = await getTextContent(browser2, {id: "changeFired"});
  changeFired = changeFired.trim().split().sort().join(" ");

  is(inputFired, "modify01 modify02 modify03 modify04 modify05",
     "input events were only dispatched for modified input, textarea fields");

  is(changeFired, "modify06 modify07 modify08 modify09 modify11",
     "change events were only dispatched for modified select, checkbox, radio fields");

  // Cleanup.
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab);
});
