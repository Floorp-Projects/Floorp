"use strict";

/*
 * This test triggers multiple alerts on one single tab, because it"s possible
 * for web content to do so. The behavior is described in bug 1266353.
 *
 * We assert the presentation of the multiple alerts, ensuring we show only
 * the oldest one.
 */
add_task(async function() {
  const PROMPTCOUNT = 9;

  let contentScript = function(MAX_PROMPT) {
    var i = MAX_PROMPT;
    let fns = ["alert", "prompt", "confirm"];
    function openDialog() {
      i--;
      if (i) {
        SpecialPowers.Services.tm.dispatchToMainThread(openDialog);
      }
      window[fns[i % 3]](fns[i % 3] + " countdown #" + i);
    }
    SpecialPowers.Services.tm.dispatchToMainThread(openDialog);
  };
  let url =
    "data:text/html,<script>(" +
    encodeURIComponent(contentScript.toSource()) +
    ")(" +
    PROMPTCOUNT +
    ");</script>";

  let promptsOpenedPromise = new Promise(function(resolve) {
    let unopenedPromptCount = PROMPTCOUNT;
    Services.obs.addObserver(function observer() {
      unopenedPromptCount--;
      if (!unopenedPromptCount) {
        Services.obs.removeObserver(observer, "tabmodal-dialog-loaded");
        info("Prompts opened.");
        resolve();
      }
    }, "tabmodal-dialog-loaded");
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url, true);
  info("Tab loaded");

  await promptsOpenedPromise;

  let promptElementsCount = PROMPTCOUNT;
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
      let prompt = tab.linkedBrowser.tabModalPromptBox.prompts.get(
        promptElement
      );
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

  BrowserTestUtils.removeTab(tab);
});
