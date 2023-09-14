/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for redirection.

const TEST_URL = `${URL_ROOT}sjs_redirection.sjs`;
const CUSTOM_USER_AGENT = "Mozilla/5.0 (Test Device) Firefox/74.0";

addRDMTask(
  null,
  async () => {
    reloadOnUAChange(true);

    registerCleanupFunction(() => {
      reloadOnUAChange(false);
    });

    const tab = await addTab(TEST_URL);
    const browser = tab.linkedBrowser;

    const { ui } = await openRDM(tab);
    await waitForDeviceAndViewportState(ui);

    info("Change the user agent");
    await changeUserAgentInput(ui, CUSTOM_USER_AGENT);
    await testUserAgent(ui, CUSTOM_USER_AGENT);

    info("Load a page which redirects");
    const onRedirectedPageLoaded = BrowserTestUtils.browserLoaded(
      browser,
      false,
      // wait specifically for the redirected page
      url => url.includes(`?redirected`)
    );
    BrowserTestUtils.startLoadingURIString(browser, `${TEST_URL}?redirect`);
    await onRedirectedPageLoaded;

    info("Check the user agent for each requests");
    await SpecialPowers.spawn(
      browser,
      [CUSTOM_USER_AGENT],
      expectedUserAgent => {
        is(
          content.wrappedJSObject.redirectRequestUserAgent,
          expectedUserAgent,
          `Sent user agent is correct for request that caused the redirect`
        );
        is(
          content.wrappedJSObject.requestUserAgent,
          expectedUserAgent,
          `Sent user agent is correct for the redirected page`
        );
      }
    );

    await closeRDM(tab);
    await removeTab(tab);
  },
  { onlyPrefAndTask: true }
);
