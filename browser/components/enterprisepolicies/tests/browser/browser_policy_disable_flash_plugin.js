/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const labelTextAskToActivate = "Ask to Activate";
const labelTextNeverActivate = "Never Activate";

function restore_prefs() {
  Services.prefs.clearUserPref("plugin.state.flash");
}
registerCleanupFunction(restore_prefs);

async function assert_flash_locked_status(win, locked, expectedLabelText) {
  let addonCard = await BrowserTestUtils.waitForCondition(async () => {
    let doc = win.getHtmlBrowser().contentDocument;
    await win.htmlBrowserLoaded;
    return doc.querySelector(`addon-card[addon-id*="Shockwave Flash"]`);
  }, "Get HTML about:addons card for flash plugin");

  const pluginOptions = addonCard.querySelector("plugin-options");
  const pluginAction = pluginOptions.querySelector("panel-item[checked]");
  ok(
    pluginAction.textContent.includes(expectedLabelText),
    `Got plugin action "${expectedLabelText}"`
  );

  // All other buttons (besides the checked one and the expand action)
  // are expected to be disabled if locked is true.
  for (const item of pluginOptions.querySelectorAll("panel-item")) {
    const actionName = item.getAttribute("action");
    if (actionName.includes("always")) {
      ok(item.hidden, `Plugin action "${actionName}" should be hidden.`);
    } else if (
      !item.hasAttribute("checked") &&
      actionName !== "expand" &&
      actionName !== "preferences"
    ) {
      is(
        item.shadowRoot.querySelector("button").disabled,
        locked,
        `Plugin action "${actionName}" should be ${
          locked ? "disabled" : "enabled"
        }`
      );
    }
  }
}

async function test_flash_status({ expectedLabelText, locked }) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  const win = await BrowserOpenAddonsMgr("addons://list/plugin");

  await assert_flash_locked_status(win, locked, expectedLabelText);

  BrowserTestUtils.removeTab(tab);

  is(
    Services.prefs.prefIsLocked("plugin.state.flash"),
    locked,
    "Flash pref lock state should match policy lock state"
  );
}

add_task(async function test_enabled() {
  await setupPolicyEngineWithJson({
    policies: {
      FlashPlugin: {
        Default: true,
      },
    },
  });

  await test_flash_status({
    expectedLabelText: labelTextAskToActivate,
    locked: false,
  });

  restore_prefs();
});

add_task(async function test_enabled_locked() {
  await setupPolicyEngineWithJson({
    policies: {
      FlashPlugin: {
        Default: true,
        Locked: true,
      },
    },
  });

  await test_flash_status({
    expectedLabelText: labelTextAskToActivate,
    locked: true,
  });

  restore_prefs();
});

add_task(async function test_disabled() {
  await setupPolicyEngineWithJson({
    policies: {
      FlashPlugin: {
        Default: false,
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
    policies: {
      FlashPlugin: {
        Default: false,
        Locked: true,
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
    policies: {
      FlashPlugin: {},
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
    policies: {
      FlashPlugin: {
        Locked: true,
      },
    },
  });

  await test_flash_status({
    expectedLabelText: labelTextAskToActivate,
    locked: true,
  });

  restore_prefs();
});
