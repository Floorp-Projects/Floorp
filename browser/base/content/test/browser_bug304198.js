/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is bug 491431 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jon Herron <leftturnsolutions@yahoo.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function test() {
  waitForExplicitFinish();

  let charsToDelete, deletedURLTab, fullURLTab, partialURLTab, testPartialURL, testURL;

  charsToDelete = 5;
  deletedURLTab = gBrowser.addTab();
  fullURLTab = gBrowser.addTab();
  partialURLTab = gBrowser.addTab();
  testURL = "http://example.org/browser/browser/base/content/test/dummy_page.html";

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

