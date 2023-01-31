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
 * @param {Boolean} doConfirmPrompt - true if we want to test the case when the user accepts the prompt,
 *         false if we want to test the case when the user cancels the prompt.
 * @param {Boolean} crossDomain - if true we will open a url that redirects us to a cross domain url,
 *        if false, we will open a url that redirects us to a same domain url
 */
async function trigger401AndHandle(doConfirmPrompt, crossDomain) {
  let url = crossDomain ? CROSS_DOMAIN_URL : SAME_DOMAIN_URL;
  let dialogShown = waitForDialog(doConfirmPrompt, crossDomain);
  await BrowserTestUtils.withNewTab(url, async function() {
    await dialogShown;
  });
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
      resolve
    );
  });
}

async function waitForDialog(doConfirmPrompt, crossDomain) {
  await TestUtils.topicObserved("common-dialog-loaded");
  let dialog = gBrowser.getTabDialogBox(gBrowser.selectedBrowser)
    ._tabDialogManager._topDialog;
  let dialogDocument = dialog._frame.contentDocument;
  if (crossDomain) {
    Assert.equal(
      dialog._overlay.getAttribute("hideContent"),
      "true",
      "Dialog overlay hides the current sites content"
    );
    Assert.equal(
      window.gURLBar.value,
      AUTH_URL,
      "Correct location is provided by the prompt"
    );
    // switch to another tab and make sure we dont mess up this new tabs url bar
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "https://example.org:443"
    );
    Assert.equal(
      window.gURLBar.value,
      "https://example.org",
      "No location is provided by the prompt, correct location is displayed"
    );
    // switch back to our tab with the prompt and make sure the url bar state is still there
    BrowserTestUtils.removeTab(tab);
    Assert.equal(
      window.gURLBar.value,
      AUTH_URL,
      "Correct location is provided by the prompt"
    );
    // make sure a value that the user types in has a higher priority than our prompts location
    gBrowser.selectedBrowser.userTypedValue = "user value";
    gURLBar.setURI();
    Assert.equal(
      window.gURLBar.value,
      "user value",
      "User typed value is shown"
    );
    // if the user clears the url bar we again fall back to the location of the prompt if we trigger setURI by a tab switch
    gBrowser.selectedBrowser.userTypedValue = "";
    gURLBar.setURI(null, true);
    Assert.equal(
      window.gURLBar.value,
      AUTH_URL,
      "Correct location is provided by the prompt"
    );
  } else {
    Assert.equal(
      dialog._overlay.getAttribute("hideContent"),
      "",
      "Dialog overlay does not hide the current sites content"
    );
    Assert.equal(
      window.gURLBar.value,
      SAME_DOMAIN_URL,
      "No location is provided by the prompt, correct location is displayed"
    );
  }

  let onDialogClosed = BrowserTestUtils.waitForEvent(
    window,
    "DOMModalDialogClosed"
  );
  if (doConfirmPrompt) {
    dialogDocument.getElementById("loginTextbox").value = "guest";
    dialogDocument.getElementById("password1Textbox").value = "guest";
    dialogDocument.getElementById("commonDialog").acceptDialog();
  } else {
    dialogDocument.getElementById("commonDialog").cancelDialog();
  }

  // wait for the dialog to be closed to check that the URLBar state is reset
  await onDialogClosed;
  // Due to bug 1812014, the url bar will be clear if we have set its value to "" while the prompt was open
  // so we trigger a tab switch again to have the uri displayed to be able to check its value
  gURLBar.setURI(null, true);
  Assert.equal(
    window.gURLBar.value,
    crossDomain ? CROSS_DOMAIN_URL : SAME_DOMAIN_URL,
    "No location is provided by the prompt"
  );
}

/**
 * Tests that the 401 auth spoofing mechanisms apply if the 401 is from a different base domain than the current sites,
 * canceling the prompt
 */
add_task(async function testCrossDomainCancel() {
  await trigger401AndHandle(false, true);
});

/**
 * Tests that the 401 auth spoofing mechanisms apply if the 401 is from a different base domain than the current sites,
 * accepting the prompt
 */
add_task(async function testCrossDomainAccept() {
  await trigger401AndHandle(true, true);
});

/**
 * Tests that the 401 auth spoofing mechanisms are not triggered by a 401 within the same base domain as the current sites,
 * canceling the prompt
 */
add_task(async function testSameDomainCancel() {
  await trigger401AndHandle(false, false);
});

/**
 * Tests that the 401 auth spoofing mechanisms are not triggered by a 401 within the same base domain as the current sites,
 * accepting the prompt
 */
add_task(async function testSameDomainAccept() {
  await trigger401AndHandle(true, false);
});
