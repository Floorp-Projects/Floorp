/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function() {
  let charsToDelete, deletedURLTab, fullURLTab, partialURLTab, testPartialURL, testURL;

  charsToDelete = 5;
  deletedURLTab = BrowserTestUtils.addTab(gBrowser);
  fullURLTab = BrowserTestUtils.addTab(gBrowser);
  partialURLTab = BrowserTestUtils.addTab(gBrowser);
  testURL = "http://example.org/browser/browser/base/content/test/urlbar/dummy_page.html";

  let loaded1 = BrowserTestUtils.browserLoaded(deletedURLTab.linkedBrowser, false, testURL);
  let loaded2 = BrowserTestUtils.browserLoaded(fullURLTab.linkedBrowser, false, testURL);
  let loaded3 = BrowserTestUtils.browserLoaded(partialURLTab.linkedBrowser, false, testURL);
  deletedURLTab.linkedBrowser.loadURI(testURL);
  fullURLTab.linkedBrowser.loadURI(testURL);
  partialURLTab.linkedBrowser.loadURI(testURL);
  await Promise.all([loaded1, loaded2, loaded3]);

  testURL = gURLBar.trimValue(testURL);
  testPartialURL = testURL.substr(0, (testURL.length - charsToDelete));

  function cleanUp() {
    gBrowser.removeTab(fullURLTab);
    gBrowser.removeTab(partialURLTab);
    gBrowser.removeTab(deletedURLTab);
  }

  async function cycleTabs() {
    await BrowserTestUtils.switchTab(gBrowser, fullURLTab);
    is(gURLBar.textValue, testURL, "gURLBar.textValue should be testURL after switching back to fullURLTab");

    await BrowserTestUtils.switchTab(gBrowser, partialURLTab);
    is(gURLBar.textValue, testPartialURL, "gURLBar.textValue should be testPartialURL after switching back to partialURLTab");
    await BrowserTestUtils.switchTab(gBrowser, deletedURLTab);
    is(gURLBar.textValue, "", 'gURLBar.textValue should be "" after switching back to deletedURLTab');

    await BrowserTestUtils.switchTab(gBrowser, fullURLTab);
    is(gURLBar.textValue, testURL, "gURLBar.textValue should be testURL after switching back to fullURLTab");
  }

  function urlbarBackspace() {
    return new Promise((resolve, reject) => {
      gBrowser.selectedBrowser.focus();
      gURLBar.addEventListener("input", function() {
        resolve();
      }, {once: true});
      gURLBar.focus();
      if (gURLBar.selectionStart == gURLBar.selectionEnd) {
        gURLBar.selectionStart = gURLBar.selectionEnd = gURLBar.textValue.length;
      }
      EventUtils.synthesizeKey("VK_BACK_SPACE", {});
    });
  }

  async function prepareDeletedURLTab() {
    await BrowserTestUtils.switchTab(gBrowser, deletedURLTab);
    is(gURLBar.textValue, testURL, "gURLBar.textValue should be testURL after initial switch to deletedURLTab");

    // simulate the user removing the whole url from the location bar
    Services.prefs.setBoolPref("browser.urlbar.clickSelectsAll", true);

    await urlbarBackspace();
    is(gURLBar.textValue, "", 'gURLBar.textValue should be "" (just set)');
    if (Services.prefs.prefHasUserValue("browser.urlbar.clickSelectsAll")) {
      Services.prefs.clearUserPref("browser.urlbar.clickSelectsAll");
    }
  }

  async function prepareFullURLTab() {
    await BrowserTestUtils.switchTab(gBrowser, fullURLTab);
    is(gURLBar.textValue, testURL, "gURLBar.textValue should be testURL after initial switch to fullURLTab");
  }

  async function preparePartialURLTab() {
    await BrowserTestUtils.switchTab(gBrowser, partialURLTab);
    is(gURLBar.textValue, testURL, "gURLBar.textValue should be testURL after initial switch to partialURLTab");

    // simulate the user removing part of the url from the location bar
    Services.prefs.setBoolPref("browser.urlbar.clickSelectsAll", false);

    let deleted = 0;
    while (deleted < charsToDelete) {
      await urlbarBackspace();
      deleted++;
    }

    is(gURLBar.textValue, testPartialURL, "gURLBar.textValue should be testPartialURL (just set)");
    if (Services.prefs.prefHasUserValue("browser.urlbar.clickSelectsAll")) {
      Services.prefs.clearUserPref("browser.urlbar.clickSelectsAll");
    }
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
