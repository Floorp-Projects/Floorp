/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests for the correct URL being displayed in the URL bar after switching
 * tabs which are in different states (e.g. deleted, partially deleted).
 */

"use strict";

const TEST_URL = `${TEST_BASE_URL}dummy_page.html`;

add_task(async function() {
  let charsToDelete,
    deletedURLTab,
    fullURLTab,
    partialURLTab,
    testPartialURL,
    testURL;

  charsToDelete = 5;
  deletedURLTab = BrowserTestUtils.addTab(gBrowser);
  fullURLTab = BrowserTestUtils.addTab(gBrowser);
  partialURLTab = BrowserTestUtils.addTab(gBrowser);
  testURL = TEST_URL;

  let loaded1 = BrowserTestUtils.browserLoaded(
    deletedURLTab.linkedBrowser,
    false,
    testURL
  );
  let loaded2 = BrowserTestUtils.browserLoaded(
    fullURLTab.linkedBrowser,
    false,
    testURL
  );
  let loaded3 = BrowserTestUtils.browserLoaded(
    partialURLTab.linkedBrowser,
    false,
    testURL
  );
  BrowserTestUtils.loadURI(deletedURLTab.linkedBrowser, testURL);
  BrowserTestUtils.loadURI(fullURLTab.linkedBrowser, testURL);
  BrowserTestUtils.loadURI(partialURLTab.linkedBrowser, testURL);
  await Promise.all([loaded1, loaded2, loaded3]);

  testURL = gURLBar.trimValue(testURL);
  testPartialURL = testURL.substr(0, testURL.length - charsToDelete);

  function cleanUp() {
    gBrowser.removeTab(fullURLTab);
    gBrowser.removeTab(partialURLTab);
    gBrowser.removeTab(deletedURLTab);
  }

  async function cycleTabs() {
    await BrowserTestUtils.switchTab(gBrowser, fullURLTab);
    is(
      gURLBar.value,
      testURL,
      "gURLBar.value should be testURL after switching back to fullURLTab"
    );

    await BrowserTestUtils.switchTab(gBrowser, partialURLTab);
    is(
      gURLBar.value,
      testPartialURL,
      "gURLBar.value should be testPartialURL after switching back to partialURLTab"
    );
    await BrowserTestUtils.switchTab(gBrowser, deletedURLTab);
    is(
      gURLBar.value,
      "",
      'gURLBar.value should be "" after switching back to deletedURLTab'
    );

    await BrowserTestUtils.switchTab(gBrowser, fullURLTab);
    is(
      gURLBar.value,
      testURL,
      "gURLBar.value should be testURL after switching back to fullURLTab"
    );
  }

  function urlbarBackspace(removeAll) {
    return new Promise((resolve, reject) => {
      gBrowser.selectedBrowser.focus();
      gURLBar.addEventListener(
        "input",
        function() {
          resolve();
        },
        { once: true }
      );
      gURLBar.focus();
      if (removeAll) {
        gURLBar.select();
      } else {
        gURLBar.selectionStart = gURLBar.selectionEnd = gURLBar.value.length;
      }
      EventUtils.synthesizeKey("KEY_Backspace");
    });
  }

  async function prepareDeletedURLTab() {
    await BrowserTestUtils.switchTab(gBrowser, deletedURLTab);
    is(
      gURLBar.value,
      testURL,
      "gURLBar.value should be testURL after initial switch to deletedURLTab"
    );

    // simulate the user removing the whole url from the location bar
    await urlbarBackspace(true);
    is(gURLBar.value, "", 'gURLBar.value should be "" (just set)');
  }

  async function prepareFullURLTab() {
    await BrowserTestUtils.switchTab(gBrowser, fullURLTab);
    is(
      gURLBar.value,
      testURL,
      "gURLBar.value should be testURL after initial switch to fullURLTab"
    );
  }

  async function preparePartialURLTab() {
    await BrowserTestUtils.switchTab(gBrowser, partialURLTab);
    is(
      gURLBar.value,
      testURL,
      "gURLBar.value should be testURL after initial switch to partialURLTab"
    );

    // simulate the user removing part of the url from the location bar
    let deleted = 0;
    while (deleted < charsToDelete) {
      await urlbarBackspace(false);
      deleted++;
    }

    is(
      gURLBar.value,
      testPartialURL,
      "gURLBar.value should be testPartialURL (just set)"
    );
  }

  // prepare the three tabs required by this test

  // First tab
  await prepareFullURLTab();
  await preparePartialURLTab();
  await prepareDeletedURLTab();

  // now cycle the tabs and make sure everything looks good
  await cycleTabs();
  cleanUp();
});
