"use strict";

const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://nocert.example.com/"
);
/*
 * Description of the test:
 * Test Navigation:
 * 1. Enable https-first and the pref to trigger beforeunload by user interaction
 * 2. Open an http site. Https-First will try to upgrade it to https - but since it has no cert that try will fail
 * 3. Then simulated user interaction and navigate to another http only page. That results in a new request which, again, https-first will try to upgrade to https
 * 4. That leads to a prompt which askes for permission to leave page - accept it
 * 5. Since the site is not using a valid https cert https-first will downgrade the request back to http
 * 6. That leads to a new request, which shouldn't lead to another prompt because the user already gave permission to leave the page
 * 7. To validate that didn't see twice the same prompt, validate we navigated successfully.
 * Test Reload:
 * analog just with navigation to same page(reload).
 */
add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_first", true],
      ["dom.require_user_interaction_for_beforeunload", true],
    ],
  });
});

add_task(async function testNavigation() {
  // The onbeforeunload dialog should appear.
  let dialogPromise = PromptTestUtils.waitForPrompt(null, {
    modalType: Services.prompt.MODAL_TYPE_CONTENT,
    promptType: "confirmEx",
  });

  let openPagePromise = openPage(true);
  let dialog = await dialogPromise;
  Assert.ok(true, "Showed the beforeunload dialog.");
  await PromptTestUtils.handlePrompt(dialog, { buttonNumClick: 0 });
  await openPagePromise;
});

add_task(async function testReload() {
  // The onbeforeunload dialog should appear.
  let dialogPromise = PromptTestUtils.waitForPrompt(null, {
    modalType: Services.prompt.MODAL_TYPE_CONTENT,
    promptType: "confirmEx",
  });
  let reloadPromise = reloadPage();
  info("wait for dialog");
  let dialog = await dialogPromise;
  Assert.ok(true, "Showed the beforeunload dialog.");
  await PromptTestUtils.handlePrompt(dialog, { buttonNumClick: 0 });
  await reloadPromise;
});

async function openPage(shouldClick) {
  // Open about:blank in a new tab.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      // Load http page.
      BrowserTestUtils.loadURI(
        browser,
        `${TEST_PATH_HTTP}file_beforeunload_permit_http.html`
      );
      await BrowserTestUtils.browserLoaded(browser);

      if (shouldClick) {
        // interact with page such that unload permit will be necessary
        await BrowserTestUtils.synthesizeMouse("body", 2, 2, {}, browser);
      }
      let hasInteractedWith = await SpecialPowers.spawn(
        browser,
        [""],
        function() {
          return content.document.userHasInteracted;
        }
      );
      is(
        shouldClick,
        hasInteractedWith,
        "Click should update document interactivity state"
      );
      // And then navigate away to another site which proves that user won't be asked twice to permit a reload (otherwise the test get timed out)
      BrowserTestUtils.loadURI(browser, "http://self-signed.example.com/");
      await BrowserTestUtils.browserLoaded(browser);
      Assert.ok(true, "Navigated successfully.");
    }
  );
}
async function reloadPage() {
  // Load initial site
  // Open about:blank in a new tab.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      // Load http page.
      BrowserTestUtils.loadURI(
        browser,
        `${TEST_PATH_HTTP}file_beforeunload_permit_http.html`
      );
      await BrowserTestUtils.browserLoaded(browser);
      // reload page by interacting
      await BrowserTestUtils.synthesizeMouse("body", 2, 2, {}, browser);

      let hasInteractedWith = await SpecialPowers.spawn(
        browser,
        [""],
        function() {
          return content.document.userHasInteracted;
        }
      );
      is(true, hasInteractedWith, "Simulated successfully user interaction");
      // Reload the site to trigger prompt which aks for permission to leave page
      info("Reload page");
      // reload http page through url bar
      BrowserTestUtils.loadURI(
        browser,
        `${TEST_PATH_HTTP}file_beforeunload_permit_http.html`
      );
      await BrowserTestUtils.browserLoaded(browser);
      is(true, true, "reload was successfull");
    }
  );
}
