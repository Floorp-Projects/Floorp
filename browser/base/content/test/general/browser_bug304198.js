/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let charsToDelete, deletedURLTab, fullURLTab, partialURLTab, testPartialURL, testURL;

  charsToDelete = 5;
  deletedURLTab = gBrowser.addTab();
  fullURLTab = gBrowser.addTab();
  partialURLTab = gBrowser.addTab();
  testURL = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";

  function cleanUp() {
    gBrowser.removeTab(fullURLTab);
    gBrowser.removeTab(partialURLTab);
    gBrowser.removeTab(deletedURLTab);
  }

  function cycleTabs() {
    gBrowser.selectedTab = fullURLTab;
    is(gURLBar.value, testURL, 'gURLBar.value should be testURL after switching back to fullURLTab');

    gBrowser.selectedTab = partialURLTab;
    is(gURLBar.value, testPartialURL, 'gURLBar.value should be testPartialURL after switching back to partialURLTab');

    gBrowser.selectedTab = deletedURLTab;
    is(gURLBar.value, '', 'gURLBar.value should be "" after switching back to deletedURLTab');

    gBrowser.selectedTab = fullURLTab;
    is(gURLBar.value, testURL, 'gURLBar.value should be testURL after switching back to fullURLTab');
  }

  // function borrowed from browser_bug386835.js
  function load(tab, url, cb) {
    tab.linkedBrowser.addEventListener("load", function (event) {
      event.currentTarget.removeEventListener("load", arguments.callee, true);
      cb();
    }, true);
    tab.linkedBrowser.loadURI(url);
  }

  function urlbarBackspace(cb) {
    gBrowser.selectedBrowser.focus();
    gURLBar.addEventListener("focus", function () {
      gURLBar.removeEventListener("focus", arguments.callee, false);
      gURLBar.addEventListener("input", function () {
        gURLBar.removeEventListener("input", arguments.callee, false);
        cb();
      }, false);
      executeSoon(function () {
        EventUtils.synthesizeKey("VK_BACK_SPACE", {});
      });
    }, false);
    gURLBar.focus();
  }

  function prepareDeletedURLTab(cb) {
    gBrowser.selectedTab = deletedURLTab;
    is(gURLBar.value, testURL, 'gURLBar.value should be testURL after initial switch to deletedURLTab');

    // simulate the user removing the whole url from the location bar
    gPrefService.setBoolPref("browser.urlbar.clickSelectsAll", true);

    urlbarBackspace(function () {
      is(gURLBar.value, "", 'gURLBar.value should be "" (just set)');
      if (gPrefService.prefHasUserValue("browser.urlbar.clickSelectsAll"))
        gPrefService.clearUserPref("browser.urlbar.clickSelectsAll");
      cb();
    });
  }

  function prepareFullURLTab(cb) {
    gBrowser.selectedTab = fullURLTab;
    is(gURLBar.value, testURL, 'gURLBar.value should be testURL after initial switch to fullURLTab');
    cb();
  }

  function preparePartialURLTab(cb) {
    gBrowser.selectedTab = partialURLTab;
    is(gURLBar.value, testURL, 'gURLBar.value should be testURL after initial switch to partialURLTab');

    // simulate the user removing part of the url from the location bar
    gPrefService.setBoolPref("browser.urlbar.clickSelectsAll", false);

    var deleted = 0;
    urlbarBackspace(function () {
      deleted++;
      if (deleted < charsToDelete) {
        urlbarBackspace(arguments.callee);
      } else {
        is(gURLBar.value, testPartialURL, "gURLBar.value should be testPartialURL (just set)");
        if (gPrefService.prefHasUserValue("browser.urlbar.clickSelectsAll"))
          gPrefService.clearUserPref("browser.urlbar.clickSelectsAll");
        cb();
      }
    });
  }

  function runTests() {
    testURL = gURLBar.trimValue(testURL);
    testPartialURL = testURL.substr(0, (testURL.length - charsToDelete));

    // prepare the three tabs required by this test
    prepareFullURLTab(function () {
      preparePartialURLTab(function () {
        prepareDeletedURLTab(function () {
          // now cycle the tabs and make sure everything looks good
          cycleTabs();
          cleanUp();
          finish();
        });
      });
    });
  }

  load(deletedURLTab, testURL, function() {
    load(fullURLTab, testURL, function() {
      load(partialURLTab, testURL, runTests);
    });
  });
}

