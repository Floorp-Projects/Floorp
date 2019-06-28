/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function restore_prefs() {
  Services.prefs.clearUserPref("dom.disable_open_during_load");
}

let ORIGINAL_PREF_VALUE = undefined;
add_task(async function setup() {
  // It seems that this pref is given a special testing value for some reason.
  // Unset that value for this test, but save the old value
  if (Services.prefs.prefHasUserValue("dom.disable_open_during_load")) {
    ORIGINAL_PREF_VALUE = Services.prefs.getBoolPref(
      "dom.disable_open_during_load"
    );
    Services.prefs.clearUserPref("dom.disable_open_during_load");
  }
});
registerCleanupFunction(async function cleanup_prefs() {
  if (ORIGINAL_PREF_VALUE === undefined) {
    Services.prefs.clearUserPref("dom.disable_open_during_load");
  } else {
    Services.prefs.setBoolPref(
      "dom.disable_open_during_load",
      ORIGINAL_PREF_VALUE
    );
  }
});

async function test_popup_blocker_disabled({ disabled, locked }) {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences#privacy"
  );
  await ContentTask.spawn(
    tab.linkedBrowser,
    { disabled, locked },
    // eslint-disable-next-line no-shadow
    async function({ disabled, locked }) {
      let checkbox = content.document.getElementById("popupPolicy");
      is(
        checkbox.checked,
        !disabled,
        "Checkbox checked state should match policy's Block status"
      );
      is(
        checkbox.disabled,
        locked,
        "Checkbox disabled state should match policy's Locked status"
      );
    }
  );
  BrowserTestUtils.removeTab(tab);

  is(
    Services.prefs.prefIsLocked("dom.disable_open_during_load"),
    locked,
    "Flash pref lock state should match policy lock state"
  );
}

add_task(async function test_initial_state() {
  await test_popup_blocker_disabled({ disabled: false, locked: false });
});

add_task(async function test_empty_policy() {
  await setupPolicyEngineWithJson({
    policies: {
      PopupBlocking: {},
    },
  });

  await test_popup_blocker_disabled({ disabled: false, locked: false });

  restore_prefs();
});

add_task(async function test_block() {
  await setupPolicyEngineWithJson({
    policies: {
      PopupBlocking: {
        Default: true,
      },
    },
  });

  await test_popup_blocker_disabled({ disabled: false, locked: false });

  restore_prefs();
});

add_task(async function test_block_locked() {
  await setupPolicyEngineWithJson({
    policies: {
      PopupBlocking: {
        Default: true,
        Locked: true,
      },
    },
  });

  await test_popup_blocker_disabled({ disabled: false, locked: true });

  restore_prefs();
});

add_task(async function test_locked() {
  await setupPolicyEngineWithJson({
    policies: {
      PopupBlocking: {
        Locked: true,
      },
    },
  });

  await test_popup_blocker_disabled({ disabled: false, locked: true });

  restore_prefs();
});

add_task(async function test_disabled() {
  await setupPolicyEngineWithJson({
    policies: {
      PopupBlocking: {
        Default: false,
      },
    },
  });

  await test_popup_blocker_disabled({ disabled: true, locked: false });

  restore_prefs();
});

add_task(async function test_disabled_locked() {
  await setupPolicyEngineWithJson({
    policies: {
      PopupBlocking: {
        Default: false,
        Locked: true,
      },
    },
  });

  await test_popup_blocker_disabled({ disabled: true, locked: true });

  restore_prefs();
});
