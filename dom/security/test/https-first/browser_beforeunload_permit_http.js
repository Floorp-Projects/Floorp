"use strict";

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://nocert.example.com/"
);
/*
 * Description of Tests:
 *
 * Test load page and reload:
 * 1. Enable HTTPS-First and the pref to trigger beforeunload by user interaction
 * 2. Open an HTTP site. HTTPS-First will try to upgrade it to https - but since it has no cert that try will fail
 * 3. Then simulated user interaction and reload the page with a reload flag.
 * 4. That should lead to a beforeUnload prompt that asks for users permission to perform reload. HTTPS-First should not try to upgrade the reload again
 *
 * Test Navigation:
 * 1. Enable HTTPS-First and the pref to trigger beforeunload by user interaction
 * 2. Open an http site. HTTPS-First will try to upgrade it to https - but since it has no cert for https that try will fail
 * 3. Then simulated user interaction and navigate to another http page. Again HTTPS-First will try to upgrade to HTTPS
 * 4. This attempted navigation leads to a prompt which askes for permission to leave page - accept it
 * 5. Since the site is not using a valid HTTPS cert HTTPS-First will downgrade the request back to HTTP
 * 6. User should NOT get asked again for permission to unload
 *
 * Test Session History Navigation:
 * 1. Enable HTTPS-First and the pref to trigger beforeunload by user interaction
 * 2. Open an http site. HTTPS-First will try to upgrade it to https - but since it has no cert for https that try will fail
 * 3. Then navigate to another http page and simulated a user interaction.
 * 4. Trigger a session history navigation by clicking the "back button".
 * 5. This attempted navigation leads to a prompt which askes for permission to leave page - accept it
 */
add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_first", true],
      ["dom.require_user_interaction_for_beforeunload", true],
    ],
  });
});
const TESTS = [
  {
    name: "Normal Reload (No flag)",
    reloadFlag: Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
  },
  {
    name: "Bypass Cache Reload",
    reloadFlag: Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE,
  },
  {
    name: "Bypass Proxy Reload",
    reloadFlag: Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY,
  },
  {
    name: "Bypass Cache and Proxy Reload",
    reloadFlag:
      Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE |
      Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY,
  },
];

add_task(async function testReloadFlags() {
  for (let index = 0; index < TESTS.length; index++) {
    const testCase = TESTS[index];
    // The onbeforeunload dialog should appear
    let dialogPromise = PromptTestUtils.waitForPrompt(null, {
      modalType: Services.prompt.MODAL_TYPE_CONTENT,
      promptType: "confirmEx",
    });
    let reloadPromise = loadPageAndReload(testCase);
    let dialog = await dialogPromise;
    Assert.ok(true, "Showed the beforeunload dialog.");
    await PromptTestUtils.handlePrompt(dialog, { buttonNumClick: 0 });
    await reloadPromise;
  }
});

add_task(async function testNavigation() {
  // The onbeforeunload dialog should appear
  let dialogPromise = PromptTestUtils.waitForPrompt(null, {
    modalType: Services.prompt.MODAL_TYPE_CONTENT,
    promptType: "confirmEx",
  });

  let openPagePromise = openPage();
  let dialog = await dialogPromise;
  Assert.ok(true, "Showed the beforeunload dialog.");
  await PromptTestUtils.handlePrompt(dialog, { buttonNumClick: 0 });
  await openPagePromise;
});

add_task(async function testSessionHistoryNavigation() {
  // The onbeforeunload dialog should appear
  let dialogPromise = PromptTestUtils.waitForPrompt(null, {
    modalType: Services.prompt.MODAL_TYPE_CONTENT,
    promptType: "confirmEx",
  });

  let openPagePromise = loadPagesAndUseBackButton();
  let dialog = await dialogPromise;
  Assert.ok(true, "Showed the beforeunload dialog.");
  await PromptTestUtils.handlePrompt(dialog, { buttonNumClick: 0 });
  await openPagePromise;
});

async function openPage() {
  // Open about:blank in a new tab
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      // Load http page
      BrowserTestUtils.startLoadingURIString(
        browser,
        `${TEST_PATH_HTTP}file_beforeunload_permit_http.html`
      );
      await BrowserTestUtils.browserLoaded(browser);
      // Interact with page such that unload permit will be necessary
      await BrowserTestUtils.synthesizeMouse("body", 2, 2, {}, browser);
      let hasInteractedWith = await SpecialPowers.spawn(
        browser,
        [""],
        function () {
          return content.document.userHasInteracted;
        }
      );

      is(true, hasInteractedWith, "Simulated successfully user interaction");
      // And then navigate away to another site which proves that user won't be asked twice to permit a reload (otherwise the test get timed out)
      BrowserTestUtils.startLoadingURIString(
        browser,
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        "http://self-signed.example.com/"
      );
      await BrowserTestUtils.browserLoaded(browser);
      Assert.ok(true, "Navigated successfully.");
    }
  );
}

async function loadPageAndReload(testCase) {
  // Load initial site
  // Open about:blank in a new tab
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      BrowserTestUtils.startLoadingURIString(
        browser,
        `${TEST_PATH_HTTP}file_beforeunload_permit_http.html`
      );
      await BrowserTestUtils.browserLoaded(browser);
      // Interact with page such that unload permit will be necessary
      await BrowserTestUtils.synthesizeMouse("body", 2, 2, {}, browser);

      let hasInteractedWith = await SpecialPowers.spawn(
        browser,
        [""],
        function () {
          return content.document.userHasInteracted;
        }
      );
      is(true, hasInteractedWith, "Simulated successfully user interaction");
      BrowserReloadWithFlags(testCase.reloadFlag);
      await BrowserTestUtils.browserLoaded(browser);
      is(true, true, `reload with flag ${testCase.name} was successful`);
    }
  );
}

async function loadPagesAndUseBackButton() {
  // Load initial site
  // Open about:blank in a new tab
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      BrowserTestUtils.startLoadingURIString(
        browser,
        `${TEST_PATH_HTTP}file_beforeunload_permit_http.html`
      );
      await BrowserTestUtils.browserLoaded(browser);

      BrowserTestUtils.startLoadingURIString(
        browser,
        `${TEST_PATH_HTTP}file_beforeunload_permit_http.html?getASessionHistoryEntry`
      );
      await BrowserTestUtils.browserLoaded(browser);
      // Interact with page such that unload permit will be necessary
      await BrowserTestUtils.synthesizeMouse("body", 2, 2, {}, browser);

      let hasInteractedWith = await SpecialPowers.spawn(
        browser,
        [""],
        function () {
          return content.document.userHasInteracted;
        }
      );
      is(true, hasInteractedWith, "Simulated successfully user interaction");
      // Go back one site by clicking the back button
      info("Clicking back button");
      let backButton = document.getElementById("back-button");
      backButton.click();
      await BrowserTestUtils.browserLoaded(browser);
      is(true, true, `Got back successful`);
    }
  );
}
