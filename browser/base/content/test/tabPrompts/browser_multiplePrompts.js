"use strict";

const CONTENT_PROMPT_SUBDIALOG = Services.prefs.getBoolPref(
  "prompts.contentPromptSubDialog",
  false
);

/**
 * Goes through a stacked series of dialogs opened with
 * CONTENT_PROMPT_SUBDIALOG set to true, and ensures that
 * the oldest one is front-most and has the right type. It
 * then closes the oldest to newest dialog.
 *
 * @param {Element} tab The <tab> that has had content dialogs opened
 * for it.
 * @param {Number} promptCount How many dialogs we expected to have been
 * opened.
 *
 * @return {Promise}
 * @resolves {undefined} Once the dialogs have all been closed.
 */
async function closeDialogs(tab, dialogCount) {
  let dialogElementsCount = dialogCount;
  let dialogs = tab.linkedBrowser.tabDialogBox.getContentDialogManager()
    .dialogs;

  is(
    dialogs.length,
    dialogElementsCount,
    "There should be " + dialogElementsCount + " dialog(s)."
  );

  let i = dialogElementsCount - 1;
  for (let dialog of dialogs) {
    dialog.focus(true);
    await dialog._dialogReady;

    let dialogWindow = dialog.frameContentWindow;
    let expectedType = ["alert", "prompt", "confirm"][i % 3];

    is(
      dialogWindow.Dialog.args.text,
      expectedType + " countdown #" + i,
      "The #" + i + " alert should be labelled as such."
    );
    i--;

    dialogWindow.Dialog.ui.button0.click();

    // The click is handled async; wait for an event loop turn for that to
    // happen.
    await new Promise(function(resolve) {
      Services.tm.dispatchToMainThread(resolve);
    });
  }

  dialogs = tab.linkedBrowser.tabDialogBox.getContentDialogManager().dialogs;
  is(dialogs.length, 0, "Dialogs should all be dismissed.");
}

/**
 * Goes through a stacked series of tabprompt modals opened with
 * CONTENT_PROMPT_SUBDIALOG set to false, and ensures that
 * the oldest one is front-most and has the right type. It also
 * ensures that the other tabprompt modals are hidden. It
 * then closes the oldest to newest dialog.
 *
 * @param {Element} tab The <tab> that has had tabprompt modals opened
 * for it.
 * @param {Number} promptCount How many modals we expected to have been
 * opened.
 *
 * @return {Promise}
 * @resolves {undefined} Once the modals have all been closed.
 */
async function closeTabModals(tab, promptCount) {
  let promptElementsCount = promptCount;
  while (promptElementsCount--) {
    let promptElements = tab.linkedBrowser.parentNode.querySelectorAll(
      "tabmodalprompt"
    );
    is(
      promptElements.length,
      promptElementsCount + 1,
      "There should be " + (promptElementsCount + 1) + " prompt(s)."
    );
    // The oldest should be the first.
    let i = 0;

    for (let promptElement of promptElements) {
      let prompt = tab.linkedBrowser.tabModalPromptBox.getPrompt(promptElement);
      let expectedType = ["alert", "prompt", "confirm"][i % 3];
      is(
        prompt.Dialog.args.text,
        expectedType + " countdown #" + i,
        "The #" + i + " alert should be labelled as such."
      );
      if (i !== promptElementsCount) {
        is(prompt.element.hidden, true, "This prompt should be hidden.");
        i++;
        continue;
      }

      is(prompt.element.hidden, false, "The last prompt should not be hidden.");
      prompt.onButtonClick(0);

      // The click is handled async; wait for an event loop turn for that to
      // happen.
      await new Promise(function(resolve) {
        Services.tm.dispatchToMainThread(resolve);
      });
    }
  }

  let promptElements = tab.linkedBrowser.parentNode.querySelectorAll(
    "tabmodalprompt"
  );
  is(promptElements.length, 0, "Prompts should all be dismissed.");
}

/*
 * This test triggers multiple alerts on one single tab, because it"s possible
 * for web content to do so. The behavior is described in bug 1266353.
 *
 * We assert the presentation of the multiple alerts, ensuring we show only
 * the oldest one.
 */
add_task(async function() {
  const PROMPTCOUNT = 9;

  let unopenedPromptCount = PROMPTCOUNT;

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com",
    true
  );
  info("Tab loaded");

  let promptsOpenedPromise = BrowserTestUtils.waitForEvent(
    tab.linkedBrowser,
    "DOMWillOpenModalDialog",
    false,
    () => {
      unopenedPromptCount--;
      return unopenedPromptCount == 0;
    }
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [PROMPTCOUNT], maxPrompts => {
    var i = maxPrompts;
    let fns = ["alert", "prompt", "confirm"];
    function openDialog() {
      i--;
      if (i) {
        SpecialPowers.Services.tm.dispatchToMainThread(openDialog);
      }
      content[fns[i % 3]](fns[i % 3] + " countdown #" + i);
    }
    SpecialPowers.Services.tm.dispatchToMainThread(openDialog);
  });

  await promptsOpenedPromise;

  if (CONTENT_PROMPT_SUBDIALOG) {
    await closeDialogs(tab, PROMPTCOUNT);
  } else {
    await closeTabModals(tab, PROMPTCOUNT);
  }

  BrowserTestUtils.removeTab(tab);
});
