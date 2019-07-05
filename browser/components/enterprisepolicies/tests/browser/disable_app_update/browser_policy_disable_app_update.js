/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
var updateService = Cc["@mozilla.org/updates/update-service;1"].getService(
  Ci.nsIApplicationUpdateService
);

add_task(async function test_updates_post_policy() {
  is(
    Services.policies.isAllowed("appUpdate"),
    false,
    "appUpdate should be disabled by policy."
  );

  is(
    updateService.canCheckForUpdates,
    false,
    "Should not be able to check for updates with DisableAppUpdate enabled."
  );
});

add_task(async function test_update_preferences_ui() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let updateRadioGroup = content.document.getElementById("updateRadioGroup");
    is(
      updateRadioGroup.hidden,
      true,
      "Update choices should be diabled when app update is locked by policy"
    );
  });

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_update_about_ui() {
  let aboutDialog = await waitForAboutDialog();
  let panelId = "policyDisabled";

  await BrowserTestUtils.waitForCondition(
    () =>
      aboutDialog.document.getElementById("updateDeck").selectedPanel &&
      aboutDialog.document.getElementById("updateDeck").selectedPanel.id ==
        panelId,
    'Waiting for expected panel ID - expected "' + panelId + '"'
  );
  is(
    aboutDialog.document.getElementById("updateDeck").selectedPanel.id,
    panelId,
    "The About Dialog panel Id should equal " + panelId
  );

  aboutDialog.close();
});

/**
 * Waits for the About Dialog to load.
 *
 * @return A promise that returns the domWindow for the About Dialog and
 *         resolves when the About Dialog loads.
 */
function waitForAboutDialog() {
  return new Promise(resolve => {
    var listener = {
      onOpenWindow: aXULWindow => {
        Services.wm.removeListener(listener);

        async function aboutDialogOnLoad() {
          domwindow.removeEventListener("load", aboutDialogOnLoad, true);
          let chromeURI = "chrome://browser/content/aboutDialog.xul";
          is(
            domwindow.document.location.href,
            chromeURI,
            "About dialog appeared"
          );
          resolve(domwindow);
        }

        var domwindow = aXULWindow.docShell.domWindow;
        domwindow.addEventListener("load", aboutDialogOnLoad, true);
      },
      onCloseWindow: aXULWindow => {},
    };

    Services.wm.addListener(listener);
    openAboutDialog();
  });
}
