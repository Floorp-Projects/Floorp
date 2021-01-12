/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

SimpleTest.requestFlakyTimeout("Needs to test a timeout");

function delay(msec) {
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise(resolve => setTimeout(resolve, msec));
}

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  await SpecialPowers.pushPrefEnv({
    set: [["prompts.contentPromptSubDialog", false]],
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

  let allowNavigation;
  let promptShown = false;
  let promptDismissed = false;
  let promptTimeout;

  const DIALOG_TOPIC = "tabmodal-dialog-loaded";
  async function observer(node) {
    promptShown = true;

    if (promptTimeout) {
      await delay(promptTimeout);
    }

    let button = node.querySelector(
      `.tabmodalprompt-button${allowNavigation ? 0 : 1}`
    );
    button.click();
    promptDismissed = true;
  }
  Services.obs.addObserver(observer, DIALOG_TOPIC);

  /*
   * Check condition where beforeunload handlers request a prompt.
   */

  // Prompt is shown, user clicks OK.
  allowNavigation = true;
  promptShown = false;

  ok(browser.permitUnload().permitUnload, "permit unload should be true");
  ok(promptShown, "prompt should have been displayed");

  // Prompt is shown, user clicks CANCEL.
  allowNavigation = false;
  promptShown = false;

  ok(!browser.permitUnload().permitUnload, "permit unload should be false");
  ok(promptShown, "prompt should have been displayed");

  // Prompt is not shown, don't permit unload.
  promptShown = false;
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

  promptShown = false;
  promptDismissed = false;
  promptTimeout = 3 * permitUnloadTimeout;
  let promise = browser.asyncPermitUnload();

  let promiseResolved = false;
  promise.then(() => {
    promiseResolved = true;
  });

  await TestUtils.waitForCondition(() => promptShown);
  ok(!promptDismissed, "Should not have dismissed prompt yet");
  ok(!promiseResolved, "Should not have resolved promise yet");

  await delay(permitUnloadTimeout * 1.5);

  ok(!promptDismissed, "Should not have dismissed prompt yet");
  ok(!promiseResolved, "Should not have resolved promise yet");

  let { permitUnload } = await promise;
  ok(promptDismissed, "Should have dismissed prompt");
  ok(!permitUnload, "Should not have permitted unload");

  promptTimeout = null;

  /*
   * Check condition where no one requests a prompt.  In all cases,
   * permitUnload should be true, and all handlers fired.
   */

  allowNavigation = true;

  url += "?1";
  BrowserTestUtils.loadURI(browser, url);
  await BrowserTestUtils.browserLoaded(browser, false, url);

  promptShown = false;
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

  await BrowserTestUtils.removeTab(tab);

  Services.obs.removeObserver(observer, DIALOG_TOPIC);
});
