/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = ROOT + "browser_456342_sample.xhtml";

/**
 * Bug 456342 - Restore values from non-standard input field types.
 */
add_task(async function test_restore_nonstandard_input_values() {
  // Add tab with various non-standard input field types.
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Fill in form values.
  let expectedValue = Math.random();
  await setFormElementValues(browser, {value: expectedValue});

  // Remove tab and check collected form data.
  await promiseRemoveTab(tab);
  let undoItems = JSON.parse(ss.getClosedTabData(window));
  let savedFormData = undoItems[0].state.formdata;

  let countGood = 0, countBad = 0;
  for (let id of Object.keys(savedFormData.id)) {
    if (savedFormData.id[id] == expectedValue) {
      countGood++;
    } else {
      countBad++;
    }
  }

  for (let exp of Object.keys(savedFormData.xpath)) {
    if (savedFormData.xpath[exp] == expectedValue) {
      countGood++;
    } else {
      countBad++;
    }
  }

  is(countGood, 4, "Saved text for non-standard input fields");
  is(countBad, 0, "Didn't save text for ignored field types");
});

function setFormElementValues(browser, data) {
  return sendMessage(browser, "ss-test:setFormElementValues", data);
}
