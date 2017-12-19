/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
  * Test for Bug 1109146.
  * The tests opens a new tab and alt + clicks to download files
  * and confirms those files are on the download list.
  *
  * The difference between this and the test "browser_contentAreaClick.js" is that
  * the code path in e10s uses ContentClick.jsm instead of browser.js::contentAreaClick() util.
  */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
                                  "resource://testing-common/PlacesTestUtils.jsm");

function setup() {
  Services.prefs.setBoolPref("browser.altClickSave", true);

  let testPage =
    "data:text/html," +
    '<p><a id="commonlink" href="http://mochi.test/moz/">Common link</a></p>' +
    '<p><math id="mathxlink" xmlns="http://www.w3.org/1998/Math/MathML" xlink:type="simple" xlink:href="http://mochi.test/moz/"><mtext>MathML XLink</mtext></math></p>' +
    '<p><svg id="svgxlink" xmlns="http://www.w3.org/2000/svg" width="100px" height="50px" version="1.1"><a xlink:type="simple" xlink:href="http://mochi.test/moz/"><text transform="translate(10, 25)">SVG XLink</text></a></svg></p>';

  return BrowserTestUtils.openNewForegroundTab(gBrowser, testPage);
}

async function clean_up() {
  // Remove downloads.
  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloads = await downloadList.getAll();
  for (let download of downloads) {
    await downloadList.remove(download);
    await download.finalize(true);
  }
  // Remove download history.
  await PlacesTestUtils.clearHistory();

  Services.prefs.clearUserPref("browser.altClickSave");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(async function test_alt_click() {
  await setup();

  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloads = [];
  let downloadView;
  // When 1 download has been attempted then resolve the promise.
  let finishedAllDownloads = new Promise( (resolve) => {
    downloadView = {
      onDownloadAdded(aDownload) {
        downloads.push(aDownload);
        resolve();
      },
    };
  });
  await downloadList.addView(downloadView);
  await BrowserTestUtils.synthesizeMouseAtCenter("#commonlink", {altKey: true}, gBrowser.selectedBrowser);

  // Wait for all downloads to be added to the download list.
  await finishedAllDownloads;
  await downloadList.removeView(downloadView);

  is(downloads.length, 1, "1 downloads");
  is(downloads[0].source.url, "http://mochi.test/moz/", "Downloaded #commonlink element");

  await clean_up();
});

add_task(async function test_alt_click_on_xlinks() {
  await setup();

  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloads = [];
  let downloadView;
  // When all 2 downloads have been attempted then resolve the promise.
  let finishedAllDownloads = new Promise( (resolve) => {
    downloadView = {
      onDownloadAdded(aDownload) {
        downloads.push(aDownload);
        if (downloads.length == 2) {
          resolve();
        }
      },
    };
  });
  await downloadList.addView(downloadView);
  await BrowserTestUtils.synthesizeMouseAtCenter("#mathxlink", {altKey: true}, gBrowser.selectedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#svgxlink", {altKey: true}, gBrowser.selectedBrowser);

  // Wait for all downloads to be added to the download list.
  await finishedAllDownloads;
  await downloadList.removeView(downloadView);

  is(downloads.length, 2, "2 downloads");
  is(downloads[0].source.url, "http://mochi.test/moz/", "Downloaded #mathxlink element");
  is(downloads[1].source.url, "http://mochi.test/moz/", "Downloaded #svgxlink element");

  await clean_up();
});
