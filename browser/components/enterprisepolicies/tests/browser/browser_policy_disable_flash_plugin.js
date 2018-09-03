/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const labelTextAlwaysActivate = "Always Activate";
const labelTextAskToActivate = "Ask to Activate";
const labelTextNeverActivate = "Never Activate";

function restore_prefs() {
  Services.prefs.clearUserPref("plugin.state.flash");
}
registerCleanupFunction(restore_prefs);

async function test_flash_status({expectedLabelText, locked}) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserOpenAddonsMgr("addons://list/plugin");
  await ContentTask.spawn(tab.linkedBrowser, {aExpectedLabelText: expectedLabelText, aLocked: locked}, async function({aExpectedLabelText, aLocked}) {
    let list = content.document.getElementById("addon-list");
    let flashEntry = list.getElementsByAttribute("name", "Shockwave Flash")[0];
    let dropDown = content.document.getAnonymousElementByAttribute(flashEntry, "anonid", "state-menulist");

    is(dropDown.label, aExpectedLabelText,
       "Flash setting text should match the expected value");
    is(dropDown.disabled, aLocked,
       "Flash controls disabled state should match policy locked state");
  });
  BrowserTestUtils.removeTab(tab);

  is(Services.prefs.prefIsLocked("plugin.state.flash"), locked,
     "Flash pref lock state should match policy lock state");
}

add_task(async function test_enabled() {
  await setupPolicyEngineWithJson({
    "policies": {
      "FlashPlugin": {
        "Default": true,
      },
    },
  });

  await test_flash_status({
    expectedLabelText: labelTextAlwaysActivate,
    locked: false,
  });

  restore_prefs();
});

add_task(async function test_enabled_locked() {
  await setupPolicyEngineWithJson({
    "policies": {
      "FlashPlugin": {
        "Default": true,
        "Locked": true,
      },
    },
  });

  await test_flash_status({
    expectedLabelText: labelTextAlwaysActivate,
    locked: true,
  });

  restore_prefs();
});

add_task(async function test_disabled() {
  await setupPolicyEngineWithJson({
    "policies": {
      "FlashPlugin": {
        "Default": false,
      },
    },
  });

  await test_flash_status({
    expectedLabelText: labelTextNeverActivate,
    locked: false,
  });

  restore_prefs();
});

add_task(async function test_disabled_locked() {
  await setupPolicyEngineWithJson({
    "policies": {
      "FlashPlugin": {
        "Default": false,
        "Locked": true,
      },
    },
  });

  await test_flash_status({
    expectedLabelText: labelTextNeverActivate,
    locked: true,
  });

  restore_prefs();
});

add_task(async function test_ask() {
  await setupPolicyEngineWithJson({
    "policies": {
      "FlashPlugin": {
      },
    },
  });

  await test_flash_status({
    expectedLabelText: labelTextAskToActivate,
    locked: false,
  });

  restore_prefs();
});

add_task(async function test_ask_locked() {
  await setupPolicyEngineWithJson({
    "policies": {
      "FlashPlugin": {
        "Locked": true,
      },
    },
  });

  await test_flash_status({
    expectedLabelText: labelTextAskToActivate,
    locked: true,
  });

  restore_prefs();
});
