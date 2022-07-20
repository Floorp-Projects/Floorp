/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test for Bug 655273
 *
 * Call pushState and then make sure that the favicon service associates our
 * old favicon with the new URI.
 */

const { PlacesTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PlacesTestUtils.sys.mjs"
);

add_task(async function test() {
  const testDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  const origURL = testDir + "file_bug655270.html";
  const newURL = origURL + "?new_page";

  const faviconURL = testDir + "favicon_bug655270.ico";

  let icon1;
  let promiseIcon1 = PlacesTestUtils.waitForNotification(
    "favicon-changed",
    events =>
      events.some(e => {
        if (e.url == origURL) {
          icon1 = e.faviconUrl;
          return true;
        }
        return false;
      }),
    "places"
  );
  let icon2;
  let promiseIcon2 = PlacesTestUtils.waitForNotification(
    "favicon-changed",
    events =>
      events.some(e => {
        if (e.url == newURL) {
          icon2 = e.faviconUrl;
          return true;
        }
        return false;
      }),
    "places"
  );

  // The page at origURL has a <link rel='icon'>, so we should get a call into
  // our observer below when it loads.  Once we verify that we have the right
  // favicon URI, we call pushState, which should trigger another favicon change
  // event, this time for the URI after pushState.
  let tab = BrowserTestUtils.addTab(gBrowser, origURL);
  await promiseIcon1;
  is(icon1, faviconURL, "FaviconURL for original URI");
  // Ignore the promise returned here and wait for the next
  // onPageChanged notification.
  SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    content.history.pushState("", "", "?new_page");
  });
  await promiseIcon2;
  is(icon2, faviconURL, "FaviconURL for new URI");
  gBrowser.removeTab(tab);
});
