/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function checkForPrompt(prefVal) {
  return async function() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["privacy.history.custom", true],
        ["browser.privatebrowsing.autostart", !prefVal],
      ],
    });

    await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
      leaveOpen: true,
    });
    let doc = gBrowser.contentDocument;
    is(
      doc.getElementById("historyMode").value,
      "custom",
      "Expect custom history mode"
    );

    // Stub out the prompt method as an easy way to check it was shown. We throw away
    // the tab straight after so don't need to bother restoring it.
    let promptFired = false;
    doc.defaultView.confirmRestartPrompt = () => {
      promptFired = true;
      return doc.defaultView.CONFIRM_RESTART_PROMPT_RESTART_NOW;
    };
    // Tick the checkbox and pretend the user did it:
    let checkbox = doc.getElementById("privateBrowsingAutoStart");
    checkbox.checked = prefVal;
    checkbox.doCommand();

    // Now the prompt should have shown.
    ok(
      promptFired,
      `Expect a prompt when turning permanent private browsing ${
        prefVal ? "on" : "off"
      }!`
    );
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  };
}

/**
 * Check we show the prompt if the permanent private browsing pref is false
 * and we flip the checkbox to true.
 */
add_task(checkForPrompt(true));

/**
 * Check it works in the other direction:
 */
add_task(checkForPrompt(false));
