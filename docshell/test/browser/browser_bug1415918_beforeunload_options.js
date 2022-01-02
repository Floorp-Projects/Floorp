/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

SimpleTest.requestFlakyTimeout("Needs to test a timeout");

function delay(msec) {
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise(resolve => setTimeout(resolve, msec));
}

function allowNextNavigation(browser) {
  return PromptTestUtils.handleNextPrompt(
    browser,
    { modalType: Services.prompt.MODAL_TYPE_CONTENT, promptType: "confirmEx" },
    { buttonNumClick: 0 }
  );
}

function cancelNextNavigation(browser) {
  return PromptTestUtils.handleNextPrompt(
    browser,
    { modalType: Services.prompt.MODAL_TYPE_CONTENT, promptType: "confirmEx" },
    { buttonNumClick: 1 }
  );
}

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  const permitUnloadTimeout = Services.prefs.getIntPref(
    "dom.beforeunload_timeout_ms"
  );

  let url = TEST_PATH + "dummy_page.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser.browsingContext, [], () => {
    content.addEventListener("beforeunload", event => {
      event.preventDefault();
    });
  });

  /*
   * Check condition where beforeunload handlers request a prompt.
   */

  // Prompt is shown, user clicks OK.

  let promptShownPromise = allowNextNavigation(browser);
  ok(browser.permitUnload().permitUnload, "permit unload should be true");
  await promptShownPromise;

  // Prompt is shown, user clicks CANCEL.
  promptShownPromise = cancelNextNavigation(browser);
  ok(!browser.permitUnload().permitUnload, "permit unload should be false");
  await promptShownPromise;

  // Prompt is not shown, don't permit unload.
  let promptShown = false;
  let shownCallback = () => {
    promptShown = true;
  };

  browser.addEventListener("DOMWillOpenModalDialog", shownCallback);
  ok(
    !browser.permitUnload("dontUnload").permitUnload,
    "permit unload should be false"
  );
  ok(!promptShown, "prompt should not have been displayed");

  // Prompt is not shown, permit unload.
  promptShown = false;
  ok(
    browser.permitUnload("unload").permitUnload,
    "permit unload should be true"
  );
  ok(!promptShown, "prompt should not have been displayed");
  browser.removeEventListener("DOMWillOpenModalDialog", shownCallback);

  promptShownPromise = PromptTestUtils.waitForPrompt(browser, {
    modalType: Services.prompt.MODAL_TYPE_CONTENT,
    promptType: "confirmEx",
  });

  let promptDismissed = false;
  let closedCallback = () => {
    promptDismissed = true;
  };

  browser.addEventListener("DOMModalDialogClosed", closedCallback);

  let promise = browser.asyncPermitUnload();

  let promiseResolved = false;
  promise.then(() => {
    promiseResolved = true;
  });

  let dialog = await promptShownPromise;
  ok(!promiseResolved, "Should not have resolved promise yet");

  await delay(permitUnloadTimeout * 1.5);

  ok(!promptDismissed, "Should not have dismissed prompt yet");
  ok(!promiseResolved, "Should not have resolved promise yet");

  await PromptTestUtils.handlePrompt(dialog, { buttonNumClick: 1 });

  let { permitUnload } = await promise;
  ok(promptDismissed, "Should have dismissed prompt");
  ok(!permitUnload, "Should not have permitted unload");

  browser.removeEventListener("DOMModalDialogClosed", closedCallback);

  promptShownPromise = allowNextNavigation(browser);

  /*
   * Check condition where no one requests a prompt.  In all cases,
   * permitUnload should be true, and all handlers fired.
   */
  url += "?1";
  BrowserTestUtils.loadURI(browser, url);
  await BrowserTestUtils.browserLoaded(browser, false, url);
  await promptShownPromise;

  promptShown = false;
  browser.addEventListener("DOMWillOpenModalDialog", shownCallback);

  ok(browser.permitUnload().permitUnload, "permit unload should be true");
  ok(!promptShown, "prompt should not have been displayed");

  promptShown = false;
  ok(
    browser.permitUnload("dontUnload").permitUnload,
    "permit unload should be true"
  );
  ok(!promptShown, "prompt should not have been displayed");

  promptShown = false;
  ok(
    browser.permitUnload("unload").permitUnload,
    "permit unload should be true"
  );
  ok(!promptShown, "prompt should not have been displayed");

  browser.removeEventListener("DOMWillOpenModalDialog", shownCallback);

  await BrowserTestUtils.removeTab(tab, { skipPermitUnload: true });
});
