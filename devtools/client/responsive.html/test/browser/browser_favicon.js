/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that favicons make it to the parent process.

const TEST_URL = `${URL_ROOT}favicon.html`;

function waitForLinkAvailable(browser) {
  let resolve, reject;

  const listener = {
    onLinkIconAvailable(b, dataURI, iconURI) {
      // Ignore icons for other browsers or empty icons.
      if (browser !== b || !iconURI) {
        return;
      }

      gBrowser.removeTabsProgressListener(listener);
      resolve(iconURI);
    },
  };

  const promise = new Promise((res, rej) => {
    resolve = res;
    reject = rej;

    gBrowser.addTabsProgressListener(listener);
  });

  promise.cancel = () => {
    gBrowser.removeTabsProgressListener(listener);

    reject();
  };

  return promise;
}

add_task(async function() {
  const tab = await addTab("about:blank");
  const browser = tab.linkedBrowser;

  await openRDM(tab);

  const promise = waitForLinkAvailable(browser);
  await load(browser, TEST_URL);
  const iconURI = await promise;
  is(iconURI, `${URL_ROOT}favicon.ico`, "Should have loaded the right icon.");
  const icon = tab.getAttribute("image");
  ok(icon.startsWith("data:"), "Should see the data icon on the tab.");

  await closeRDM(tab);

  await removeTab(tab);
});
