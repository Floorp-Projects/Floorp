/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

let TEST_PATH_AUTH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.org"
);

const CROSS_DOMAIN_URL = TEST_PATH + "redirect-crossDomain.html";

const SAME_DOMAIN_URL = TEST_PATH + "redirect-sameDomain.html";

const AUTH_URL = TEST_PATH_AUTH + "auth-route.sjs";

/**
 * Opens a new tab with a url that ether redirects us cross or same domain
 *
 * @param {Boolean} crossDomain - if true we will open a url that redirects us to a cross domain url,
 *        if false, we will open a url that redirects us to a same domain url
 */
async function trigger401AndHandle(crossDomain) {
  let dialogShown = waitForDialogAndDragNDropURL(crossDomain);
  await BrowserTestUtils.withNewTab(
    crossDomain ? CROSS_DOMAIN_URL : SAME_DOMAIN_URL,
    async function () {
      await dialogShown;
    }
  );
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
      resolve
    );
  });
}

async function waitForDialogAndDragNDropURL(crossDomain) {
  await TestUtils.topicObserved("common-dialog-loaded");
  let dialog = gBrowser.getTabDialogBox(gBrowser.selectedBrowser)
    ._tabDialogManager._topDialog;
  let dialogDocument = dialog._frame.contentDocument;

  let urlbar = document.getElementById("urlbar-input");
  let dataTran = new DataTransfer();
  let urlEvent = new DragEvent("dragstart", { dataTransfer: dataTran });
  let urlBarContainer = document.getElementById("urlbar-input-container");
  // We intentionally turn off a11y_checks for the following click, because
  // it is send to prepare the URL Bar for the mouse-specific action - for a
  // drag event, while there are other ways are accessible for users of
  // assistive technology and keyboards, therefore this test can be excluded
  // from the accessibility tests.
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
  urlBarContainer.click();
  AccessibilityUtils.resetEnv();
  // trigger a drag event in the gUrlBar
  urlbar.dispatchEvent(urlEvent);
  // this should set some propperties on our event, like the url we are dragging
  if (crossDomain) {
    is(
      urlEvent.dataTransfer.getData("text/plain"),
      AUTH_URL,
      "correct cross Domain URL is dragged over"
    );
  } else {
    is(
      urlEvent.dataTransfer.getData("text/plain"),
      SAME_DOMAIN_URL,
      "correct same domain URL is dragged over"
    );
  }

  let onDialogClosed = BrowserTestUtils.waitForEvent(
    window,
    "DOMModalDialogClosed"
  );
  dialogDocument.getElementById("commonDialog").cancelDialog();

  await onDialogClosed;
}

/**
 * Tests that the 401 auth spoofing mechanisms covers the url bar drag and drop action propperly,
 */
add_task(async function testUrlDragAndDrop() {
  await trigger401AndHandle(true);
});

/**
 * Tests that the 401 auth spoofing mechanisms do not apply to the url bar drag and drop action if the 401 is not from a different base domain,
 */
add_task(async function testUrlDragAndDrop() {
  await trigger401AndHandle(false);
});
