/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Execute the provided script content by generating a dynamic script tag and
 * inserting it in the page for the current selected browser.
 *
 * @param {string} script
 *     The script to execute.
 * @returns {Promise}
 *     A promise that resolves when the script node was added and removed from
 *     the content page.
 */
function createScriptNode(script) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [script],
    function (_script) {
      const scriptTag = content.document.createElement("script");
      scriptTag.append(content.document.createTextNode(_script));
      content.document.body.append(scriptTag);
    }
  );
}

add_task(
  async function test_close_prompt_event_detail_accepted_with_default_value() {
    const closed = BrowserTestUtils.waitForEvent(
      window,
      "DOMModalDialogClosed"
    );

    const promptPromise = BrowserTestUtils.promiseAlertDialogOpen();

    const defaultValue = "Default value";
    await createScriptNode(
      `setTimeout(() => window.prompt('Enter your name:', '${defaultValue}'))`
    );
    const promptWin = await promptPromise;

    info("accepting prompt with default value.");
    promptWin.document.querySelector("dialog").acceptDialog();

    const closedEvent = await closed;

    is(
      closedEvent.detail.areLeaving,
      true,
      "Received correct `areLeaving` value in the event detail"
    );
    is(
      closedEvent.detail.value,
      defaultValue,
      "Received correct `value` value in the event detail"
    );
  }
);

add_task(
  async function test_close_prompt_event_detail_accepted_with_amended_default_value() {
    const closed = BrowserTestUtils.waitForEvent(
      window,
      "DOMModalDialogClosed"
    );

    const promptPromise = BrowserTestUtils.promiseAlertDialogOpen();

    const defaultValue = "Default value";
    await createScriptNode(
      `setTimeout(() => window.prompt('Enter your name:', '${defaultValue}'))`
    );
    const promptWin = await promptPromise;

    const amendedValue = "Test";
    promptWin.document.getElementById("loginTextbox").value = amendedValue;

    info("accepting prompt with amended value.");
    promptWin.document.querySelector("dialog").acceptDialog();

    const closedEvent = await closed;

    is(
      closedEvent.detail.areLeaving,
      true,
      "Received correct `areLeaving` value in the event detail"
    );
    is(
      closedEvent.detail.value,
      amendedValue,
      "Received correct `value` value in the event detail"
    );
  }
);

add_task(
  async function test_close_prompt_event_detail_dismissed_with_default_value() {
    const closed = BrowserTestUtils.waitForEvent(
      window,
      "DOMModalDialogClosed"
    );

    const promptPromise = BrowserTestUtils.promiseAlertDialogOpen();

    const defaultValue = "Default value";
    await createScriptNode(
      `setTimeout(() => window.prompt('Enter your name:', '${defaultValue}'))`
    );
    const promptWin = await promptPromise;

    info("Dismissing prompt with default value.");
    promptWin.document.querySelector("dialog").cancelDialog();

    const closedEvent = await closed;

    is(
      closedEvent.detail.areLeaving,
      false,
      "Received correct `areLeaving` value in the event detail"
    );
    is(
      closedEvent.detail.value,
      null,
      "Received correct `value` value in the event detail"
    );
  }
);

add_task(
  async function test_close_prompt_event_detail_dismissed_with_amended_default_value() {
    const closed = BrowserTestUtils.waitForEvent(
      window,
      "DOMModalDialogClosed"
    );

    const promptPromise = BrowserTestUtils.promiseAlertDialogOpen();

    const defaultValue = "Default value";
    await createScriptNode(
      `setTimeout(() => window.prompt('Enter your name:', '${defaultValue}'))`
    );
    const promptWin = await promptPromise;

    const amendedValue = "Test";
    promptWin.document.getElementById("loginTextbox").value = amendedValue;

    info("Dismissing prompt with amended value.");
    promptWin.document.querySelector("dialog").cancelDialog();

    const closedEvent = await closed;

    is(
      closedEvent.detail.areLeaving,
      false,
      "Received correct `areLeaving` value in the event detail"
    );
    is(
      closedEvent.detail.value,
      null,
      "Received correct `value` value in the event detail"
    );
  }
);
