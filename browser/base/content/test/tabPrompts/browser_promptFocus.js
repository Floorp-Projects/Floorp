/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

// MacOS has different default focus behavior for prompts.
const isMacOS = Services.appinfo.OS === "Darwin";

/**
 * Tests that prompts are focused when switching tabs.
 */
add_task(async function test_tabdialogbox_tab_switch_focus() {
  // Open 3 tabs
  let tabPromises = [];
  for (let i = 0; i < 3; i += 1) {
    tabPromises.push(
      BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        "https://example.com",
        true
      )
    );
  }
  // Wait for tabs to be ready
  let tabs = await Promise.all(tabPromises);
  let [tabA, tabB, tabC] = tabs;

  // Spawn two prompts, which have different default focus as determined by
  // CommonDialog#setDefaultFocus.
  let openPromise = PromptTestUtils.waitForPrompt(tabA.linkedBrowser, {
    modalType: Services.prompt.MODAL_TYPE_TAB,
    promptType: "confirm",
  });
  Services.prompt.asyncConfirm(
    tabA.linkedBrowser.browsingContext,
    Services.prompt.MODAL_TYPE_TAB,
    null,
    "prompt A"
  );
  let promptA = await openPromise;

  openPromise = PromptTestUtils.waitForPrompt(tabB.linkedBrowser, {
    modalType: Services.prompt.MODAL_TYPE_TAB,
    promptType: "promptPassword",
  });
  Services.prompt.asyncPromptPassword(
    tabB.linkedBrowser.browsingContext,
    Services.prompt.MODAL_TYPE_TAB,
    null,
    "prompt B",
    "",
    null,
    false
  );
  let promptB = await openPromise;

  // Switch tabs and check if the correct element was focused.

  // Switch back to the third tab which doesn't have a prompt.
  await BrowserTestUtils.switchTab(gBrowser, tabC);
  is(
    Services.focus.focusedElement,
    tabC.linkedBrowser,
    "Tab without prompt should have focus on browser."
  );

  // Switch to first tab which has prompt
  await BrowserTestUtils.switchTab(gBrowser, tabA);

  if (isMacOS) {
    is(
      Services.focus.focusedElement,
      promptA.ui.infoBody,
      "Tab with prompt should have focus on body."
    );
  } else {
    is(
      Services.focus.focusedElement,
      promptA.ui.button0,
      "Tab with prompt should have focus on default button."
    );
  }

  await PromptTestUtils.handlePrompt(promptA);

  // Switch to second tab which has prompt
  await BrowserTestUtils.switchTab(gBrowser, tabB);
  is(
    Services.focus.focusedElement,
    promptB.ui.password1Textbox,
    "Tab with password prompt should have focus on password field."
  );
  await PromptTestUtils.handlePrompt(promptB);

  // Cleanup
  tabs.forEach(tab => {
    BrowserTestUtils.removeTab(tab);
  });
});

/**
 * Tests that an alert prompt has focus on the default element.
 * @param {CommonDialog} prompt - Prompt to test focus for.
 * @param {number} index - Index of the prompt to log.
 */
function testAlertPromptFocus(prompt, index) {
  if (isMacOS) {
    is(
      Services.focus.focusedElement,
      prompt.ui.infoBody,
      `Prompt #${index} should have focus on body.`
    );
  } else {
    is(
      Services.focus.focusedElement,
      prompt.ui.button0,
      `Prompt #${index} should have focus on default button.`
    );
  }
}

/**
 * Test that we set the correct focus when queuing multiple prompts.
 */
add_task(async function test_tabdialogbox_prompt_queue_focus() {
  await BrowserTestUtils.withNewTab(gBrowser, async browser => {
    const PROMPT_COUNT = 10;

    let firstPromptPromise = PromptTestUtils.waitForPrompt(browser, {
      modalType: Services.prompt.MODAL_TYPE_TAB,
      promptType: "alert",
    });

    for (let i = 0; i < PROMPT_COUNT; i += 1) {
      Services.prompt.asyncAlert(
        browser.browsingContext,
        Services.prompt.MODAL_TYPE_TAB,
        null,
        "prompt " + i
      );
    }

    // Close prompts one by one and check focus.
    let nextPromptPromise = firstPromptPromise;
    for (let i = 0; i < PROMPT_COUNT; i += 1) {
      let p = await nextPromptPromise;
      testAlertPromptFocus(p, i);

      if (i < PROMPT_COUNT - 1) {
        nextPromptPromise = PromptTestUtils.waitForPrompt(browser, {
          modalType: Services.prompt.MODAL_TYPE_TAB,
          promptType: "alert",
        });
      }
      await PromptTestUtils.handlePrompt(p);
    }

    // All prompts are closed, focus should be back on the browser.
    is(
      Services.focus.focusedElement,
      browser,
      "Tab without prompts should have focus on browser."
    );
  });
});
