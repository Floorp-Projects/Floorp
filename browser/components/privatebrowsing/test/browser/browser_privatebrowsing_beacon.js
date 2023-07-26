/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_DOMAIN = "example.com";
const TEST_TOP = `https://${TEST_DOMAIN}`;
const TEST_URL = `${TEST_TOP}/browser/browser/components/privatebrowsing/test/browser/title.sjs`;

add_task(async function () {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    TEST_TOP
  );

  // Create a promise to wait the http response of the beacon request.
  let promise = BrowserUtils.promiseObserved(
    "http-on-examine-response",
    subject => {
      let channel = subject.QueryInterface(Ci.nsIHttpChannel);
      let url = channel.URI.spec;

      return url == TEST_URL;
    }
  );

  // Open a tab and send a beacon.
  await SpecialPowers.spawn(tab.linkedBrowser, [TEST_URL], async url => {
    content.navigator.sendBeacon(url);
  });

  // Close the entire private window directly.
  await BrowserTestUtils.closeWindow(privateWin);

  // Wait the response.
  await promise;

  const cookies = Services.cookies.getCookiesFromHost(TEST_DOMAIN, {
    privateBrowsingId: 1,
  });

  is(cookies.length, 0, "No cookies after close the private window.");
});
