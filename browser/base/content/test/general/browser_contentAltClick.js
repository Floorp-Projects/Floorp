/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for Bug 1109146.
 * The tests opens a new tab and alt + clicks to download files
 * and confirms those files are on the download list.
 *
 * The difference between this and the test "browser_contentAreaClick.js" is that
 * the code path in e10s uses the ClickHandler actor instead of browser.js::contentAreaClick() util.
 */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Downloads",
  "resource://gre/modules/Downloads.jsm"
);

function setup() {
  Services.prefs.setBoolPref("browser.altClickSave", true);

  let testPage =
    "data:text/html," +
    '<p><a id="commonlink" href="http://mochi.test/moz/">Common link</a></p>' +
    '<p><math id="mathlink" xmlns="http://www.w3.org/1998/Math/MathML" href="http://mochi.test/moz/"><mtext>MathML XLink</mtext></math></p>' +
    '<p><svg id="svgxlink" xmlns="http://www.w3.org/2000/svg" width="100px" height="50px" version="1.1"><a xlink:type="simple" xlink:href="http://mochi.test/moz/"><text transform="translate(10, 25)">SVG XLink</text></a></svg></p><br>' +
    '<span id="host"></span><script>document.getElementById("host").attachShadow({mode: "closed"}).appendChild(document.getElementById("commonlink").cloneNode(true));</script>' +
    '<iframe id="frame" src="https://test2.example.com:443/browser/browser/base/content/test/general/file_with_link_to_http.html"></iframe>';

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
  await PlacesUtils.history.clear();

  Services.prefs.clearUserPref("browser.altClickSave");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(async function test_alt_click() {
  await setup();

  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloads = [];
  let downloadView;
  // When 1 download has been attempted then resolve the promise.
  let finishedAllDownloads = new Promise(resolve => {
    downloadView = {
      onDownloadAdded(aDownload) {
        downloads.push(aDownload);
        resolve();
      },
    };
  });
  await downloadList.addView(downloadView);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#commonlink",
    { altKey: true },
    gBrowser.selectedBrowser
  );

  // Wait for all downloads to be added to the download list.
  await finishedAllDownloads;
  await downloadList.removeView(downloadView);

  is(downloads.length, 1, "1 downloads");
  is(
    downloads[0].source.url,
    "http://mochi.test/moz/",
    "Downloaded #commonlink element"
  );

  await clean_up();
});

add_task(async function test_alt_click_shadow_dom() {
  await setup();

  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloads = [];
  let downloadView;
  // When 1 download has been attempted then resolve the promise.
  let finishedAllDownloads = new Promise(resolve => {
    downloadView = {
      onDownloadAdded(aDownload) {
        downloads.push(aDownload);
        resolve();
      },
    };
  });
  await downloadList.addView(downloadView);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#host",
    { altKey: true },
    gBrowser.selectedBrowser
  );

  // Wait for all downloads to be added to the download list.
  await finishedAllDownloads;
  await downloadList.removeView(downloadView);

  is(downloads.length, 1, "1 downloads");
  is(
    downloads[0].source.url,
    "http://mochi.test/moz/",
    "Downloaded #commonlink element in shadow DOM."
  );

  await clean_up();
});

add_task(async function test_alt_click_on_xlinks() {
  await setup();

  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloads = [];
  let downloadView;
  // When all 2 downloads have been attempted then resolve the promise.
  let finishedAllDownloads = new Promise(resolve => {
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
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#mathlink",
    { altKey: true },
    gBrowser.selectedBrowser
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#svgxlink",
    { altKey: true },
    gBrowser.selectedBrowser
  );

  // Wait for all downloads to be added to the download list.
  await finishedAllDownloads;
  await downloadList.removeView(downloadView);

  is(downloads.length, 2, "2 downloads");
  is(
    downloads[0].source.url,
    "http://mochi.test/moz/",
    "Downloaded #mathlink element"
  );
  is(
    downloads[1].source.url,
    "http://mochi.test/moz/",
    "Downloaded #svgxlink element"
  );

  await clean_up();
});

// Alt+Click a link in a frame from another domain as the outer document.
add_task(async function test_alt_click_in_frame() {
  await setup();

  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloads = [];
  let downloadView;
  // When the download has been attempted, resolve the promise.
  let finishedAllDownloads = new Promise(resolve => {
    downloadView = {
      onDownloadAdded(aDownload) {
        downloads.push(aDownload);
        resolve();
      },
    };
  });

  await downloadList.addView(downloadView);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#linkToExample",
    { altKey: true },
    gBrowser.selectedBrowser.browsingContext.children[0]
  );

  // Wait for all downloads to be added to the download list.
  await finishedAllDownloads;
  await downloadList.removeView(downloadView);

  is(downloads.length, 1, "1 downloads");
  is(
    downloads[0].source.url,
    "http://example.org/",
    "Downloaded link in iframe."
  );

  await clean_up();
});
