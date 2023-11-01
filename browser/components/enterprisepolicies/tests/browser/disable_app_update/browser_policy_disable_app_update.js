/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

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

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let setting = content.document.getElementById("updateSettingsContainer");
    is(
      setting.hidden,
      true,
      "Update choices should be disabled when app update is locked by policy"
    );
  });

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_update_about_ui() {
  let aboutDialog = await waitForAboutDialog();
  let panelId = "policyDisabled";

  await BrowserTestUtils.waitForCondition(
    () => aboutDialog.gAppUpdater?.selectedPanel?.id == panelId,
    'Waiting for expected panel ID - expected "' + panelId + '"'
  );
  is(
    aboutDialog.gAppUpdater.selectedPanel.id,
    panelId,
    "The About Dialog panel Id should equal " + panelId
  );

  // Make sure that we still remain on the "disabled by policy" panel after
  // `AppUpdater.stop()` is called.
  aboutDialog.gAppUpdater._appUpdater.stop();
  is(
    aboutDialog.gAppUpdater.selectedPanel.id,
    panelId,
    "The About Dialog panel Id should still equal " + panelId
  );

  aboutDialog.close();
});

/**
 * Waits for the About Dialog to load.
 *
 * @returns {Promise}
 *   A promise that returns the domWindow for the About Dialog and resolves when
 *   the About Dialog loads.
 */
function waitForAboutDialog() {
  return new Promise(resolve => {
    var listener = {
      onOpenWindow: aXULWindow => {
        Services.wm.removeListener(listener);

        async function aboutDialogOnLoad() {
          domwindow.removeEventListener("load", aboutDialogOnLoad, true);
          let chromeURI = "chrome://browser/content/aboutDialog.xhtml";
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

add_task(async function test_no_update_intervention() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "update firefox",
      waitForFocus,
      fireInputEvent: true,
    });
    for (let result of context.results) {
      Assert.notEqual(result.type, UrlbarUtils.RESULT_TYPE.TIP);
    }
    await UrlbarTestUtils.promisePopupClose(window, () =>
      window.gURLBar.blur()
    );
  });
});
