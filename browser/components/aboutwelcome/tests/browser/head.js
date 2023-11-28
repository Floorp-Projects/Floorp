/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  FxAccounts: "resource://gre/modules/FxAccounts.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  QueryCache: "resource://activity-stream/lib/ASRouterTargeting.jsm",
});

// Set the content pref to make it available across tests
const ABOUT_WELCOME_OVERRIDE_CONTENT_PREF = "browser.aboutwelcome.screens";

function popPrefs() {
  return SpecialPowers.popPrefEnv();
}
function pushPrefs(...prefs) {
  return SpecialPowers.pushPrefEnv({ set: prefs });
}

async function getAboutWelcomeParent(browser) {
  let windowGlobalParent = browser.browsingContext.currentWindowGlobal;
  return windowGlobalParent.getActor("AboutWelcome");
}

async function setAboutWelcomeMultiStage(value = "") {
  return pushPrefs([ABOUT_WELCOME_OVERRIDE_CONTENT_PREF, value]);
}

async function setAboutWelcomePref(value) {
  return pushPrefs(["browser.aboutwelcome.enabled", value]);
}

async function openMRAboutWelcome() {
  await setAboutWelcomePref(true); // NB: Calls pushPrefs
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  return {
    browser: tab.linkedBrowser,
    cleanup: async () => {
      BrowserTestUtils.removeTab(tab);
      await popPrefs(); // for setAboutWelcomePref()
    },
  };
}

async function onButtonClick(browser, elementId) {
  await ContentTask.spawn(
    browser,
    { elementId },
    async ({ elementId: buttonId }) => {
      let button = await ContentTaskUtils.waitForCondition(
        () => content.document.querySelector(buttonId),
        buttonId
      );
      button.click();
    }
  );
}

async function clearHistoryAndBookmarks() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
  QueryCache.expireAll();
}

/**
 * Setup functions to test welcome UI
 */
async function test_screen_content(
  browser,
  experiment,
  expectedSelectors = [],
  unexpectedSelectors = []
) {
  await ContentTask.spawn(
    browser,
    { expectedSelectors, experiment, unexpectedSelectors },
    async ({
      expectedSelectors: expected,
      experiment: experimentName,
      unexpectedSelectors: unexpected,
    }) => {
      for (let selector of expected) {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(selector),
          `Should render ${selector} in ${experimentName}`
        );
      }
      for (let selector of unexpected) {
        ok(
          !content.document.querySelector(selector),
          `Should not render ${selector} in ${experimentName}`
        );
      }

      if (experimentName === "home") {
        Assert.equal(
          content.document.location.href,
          "about:home",
          "Navigated to about:home"
        );
      } else {
        Assert.equal(
          content.document.location.href,
          "about:welcome",
          "Navigated to a welcome screen"
        );
      }
    }
  );
}

async function test_element_styles(
  browser,
  elementSelector,
  expectedStyles = {},
  unexpectedStyles = {}
) {
  await ContentTask.spawn(
    browser,
    [elementSelector, expectedStyles, unexpectedStyles],
    async ([selector, expected, unexpected]) => {
      const element = await ContentTaskUtils.waitForCondition(() =>
        content.document.querySelector(selector)
      );
      const computedStyles = content.window.getComputedStyle(element);
      Object.entries(expected).forEach(([attr, val]) =>
        is(
          computedStyles[attr],
          val,
          `${selector} should have computed ${attr} of ${val}`
        )
      );
      Object.entries(unexpected).forEach(([attr, val]) =>
        isnot(
          computedStyles[attr],
          val,
          `${selector} should not have computed ${attr} of ${val}`
        )
      );
    }
  );
}
