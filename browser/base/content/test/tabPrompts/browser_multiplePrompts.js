"use strict";

/*
 * This test triggers multiple alerts on one single tab, because it"s possible
 * for web content to do so. The behavior is described in bug 1266353.
 *
 * We assert the presentation of the multiple alerts, ensuring we show only
 * the oldest one.
 */
add_task(function*() {
  const PROMPTCOUNT = 5;

  let contentScript = function() {
    var i = 5; // contentScript has no access to PROMPTCOUNT.
    window.addEventListener("message", function() {
      i--;
      if (i) {
        window.postMessage("ping", "*");
      }
      alert("Alert countdown #" + i);
    });
    window.postMessage("ping", "*");
  };
  let url = "data:text/html,<script>(" + encodeURIComponent(contentScript.toSource()) + ")();</script>"

  let promptsOpenedPromise = new Promise(function(resolve) {
    let unopenedPromptCount = PROMPTCOUNT;
    Services.obs.addObserver(function observer() {
      unopenedPromptCount--;
      if (!unopenedPromptCount) {
        Services.obs.removeObserver(observer, "tabmodal-dialog-loaded");
        info("Prompts opened.");
        resolve();
      }
    }, "tabmodal-dialog-loaded", false);
  });

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url, true);
  info("Tab loaded");

  yield promptsOpenedPromise;

  let promptsCount = PROMPTCOUNT;
  while (promptsCount--) {
    let prompts = tab.linkedBrowser.parentNode.querySelectorAll("tabmodalprompt");
    is(prompts.length, promptsCount + 1, "There should be " + (promptsCount + 1) + " prompt(s).");
    // The oldest should be the first.
    let i = 0;
    for (let prompt of prompts) {
      is(prompt.Dialog.args.text, "Alert countdown #" + i, "The #" + i + " alert should be labelled as such.");
      if (i !== promptsCount) {
        is(prompt.hidden, true, "This prompt should be hidden.");
        i++;
        continue;
      }

      is(prompt.hidden, false, "The last prompt should not be hidden.");
      prompt.onButtonClick(0);
    }
  }

  let prompts = tab.linkedBrowser.parentNode.querySelectorAll("tabmodalprompt");
  is(prompts.length, 0, "Prompts should all be dismissed.");

  yield BrowserTestUtils.removeTab(tab);
});
