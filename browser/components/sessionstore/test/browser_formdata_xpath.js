/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = ROOT + "browser_formdata_xpath_sample.html";

/**
 * Bug 346337 - Generic form data restoration tests.
 */
add_task(function setup() {
  // make sure we don't save form data at all (except for tab duplication)
  Services.prefs.setIntPref("browser.sessionstore.privacy_level", 2);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.sessionstore.privacy_level");
  });
});

const FILE1 = createFilePath("346337_test1.file");
const FILE2 = createFilePath("346337_test2.file");

const FIELDS = {
  "//input[@name='input']":     Date.now().toString(),
  "//input[@name='spaced 1']":  Math.random().toString(),
  "//input[3]":                 "three",
  "//input[@type='checkbox']":  true,
  "//input[@name='uncheck']":   false,
  "//input[@type='radio'][1]":  false,
  "//input[@type='radio'][2]":  true,
  "//input[@type='radio'][3]":  false,
  "//select":                   2,
  "//select[@multiple]":        [1, 3],
  "//textarea[1]":              "",
  "//textarea[2]":              "Some text... " + Math.random(),
  "//textarea[3]":              "Some more text\n" + new Date(),
  "//input[@type='file'][1]":   [FILE1],
  "//input[@type='file'][2]":   [FILE1, FILE2]
};

add_task(function test_form_data_restoration() {
  // Load page with some input fields.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Fill in some values.
  for (let xpath of Object.keys(FIELDS)) {
    yield setFormValue(browser, xpath);
  }

  // Duplicate the tab.
  let tab2 = gBrowser.duplicateTab(tab);
  let browser2 = tab2.linkedBrowser;
  yield promiseTabRestored(tab2);

  // Check that all form values have been duplicated.
  for (let xpath of Object.keys(FIELDS)) {
    let expected = JSON.stringify(FIELDS[xpath]);
    let actual = JSON.stringify(yield getFormValue(browser2, xpath));
    is(actual, expected, "The value for \"" + xpath + "\" was correctly restored");
  }

  // Remove all tabs.
  yield promiseRemoveTab(tab2);
  yield promiseRemoveTab(tab);

  // Restore one of the tabs again.
  tab = ss.undoCloseTab(window, 0);
  browser = tab.linkedBrowser;
  yield promiseTabRestored(tab);

  // Check that none of the form values have been restored due to the privacy
  // level settings.
  for (let xpath of Object.keys(FIELDS)) {
    let expected = FIELDS[xpath];
    if (expected) {
      let actual = yield getFormValue(browser, xpath, expected);
      isnot(actual, expected, "The value for \"" + xpath + "\" was correctly discarded");
    }
  }

  // Cleanup.
  yield promiseRemoveTab(tab);
});

function createFilePath(leaf) {
  let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
  file.append(leaf);
  return file.path;
}

function isArrayOfNumbers(value) {
  return Array.isArray(value) && value.every(n => typeof(n) === "number");
}

function isArrayOfStrings(value) {
  return Array.isArray(value) && value.every(n => typeof(n) === "string");
}

function getFormValue(browser, xpath) {
  let value = FIELDS[xpath];

  if (typeof value == "string") {
    return getInputValue(browser, {xpath: xpath});
  }

  if (typeof value == "boolean") {
    return getInputChecked(browser, {xpath: xpath});
  }

  if (typeof value == "number") {
    return getSelectedIndex(browser, {xpath: xpath});
  }

  if (isArrayOfNumbers(value)) {
    return getMultipleSelected(browser, {xpath: xpath});
  }

  if (isArrayOfStrings(value)) {
    return getFileNameArray(browser, {xpath: xpath});
  }

  throw new Error("unknown input type");
}

function setFormValue(browser, xpath) {
  let value = FIELDS[xpath];

  if (typeof value == "string") {
    return setInputValue(browser, {xpath: xpath, value: value});
  }

  if (typeof value == "boolean") {
    return setInputChecked(browser, {xpath: xpath, checked: value});
  }

  if (typeof value == "number") {
    return setSelectedIndex(browser, {xpath: xpath, index: value});
  }

  if (isArrayOfNumbers(value)) {
    return setMultipleSelected(browser, {xpath: xpath, indices: value});
  }

  if (isArrayOfStrings(value)) {
    return setFileNameArray(browser, {xpath: xpath, names: value});
  }

  throw new Error("unknown input type");
}
