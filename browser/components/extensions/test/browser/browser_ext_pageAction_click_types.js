/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_setup(async function () {
  // The page action button is hidden by default.
  // This tests the use of pageAction when the button is visible.
  //
  // TODO(Bug 1704171): this should technically be removed in a follow up
  // and the tests in this file adapted to keep into account that:
  // - The pageAction is pinned on the urlbar by default
  //   when shown, and hidden when is not available (same for the
  //   overflow menu when enabled)
  BrowserPageActions.mainButtonNode.style.visibility = "visible";
  registerCleanupFunction(() => {
    BrowserPageActions.mainButtonNode.style.removeProperty("visibility");
  });
});

async function test_clickData(testAsNonPersistent = false) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {},
      background: {
        persistent: !testAsNonPersistent,
        scripts: ["background.js"],
      },
    },

    files: {
      "background.js": async function background() {
        function onClicked(_tab, info) {
          let button = info.button;
          let modifiers = info.modifiers;
          browser.test.sendMessage("onClick", { button, modifiers });
        }

        browser.pageAction.onClicked.addListener(onClicked);

        let [tab] = await browser.tabs.query({
          active: true,
          currentWindow: true,
        });
        await browser.pageAction.show(tab.id);
        browser.test.sendMessage("ready");
      },
    },
  });

  const map = {
    shiftKey: "Shift",
    altKey: "Alt",
    metaKey: "Command",
    ctrlKey: "Ctrl",
  };

  function assertSingleModifier(info, modifier) {
    if (modifier === "ctrlKey" && AppConstants.platform === "macosx") {
      is(
        info.modifiers.length,
        2,
        `MacCtrl modifier with control click on Mac`
      );
      is(
        info.modifiers[1],
        "MacCtrl",
        `MacCtrl modifier with control click on Mac`
      );
    } else {
      is(
        info.modifiers.length,
        1,
        `No unnecessary modifiers for exactly one key on event`
      );
    }

    is(info.modifiers[0], map[modifier], `Correct modifier on click event`);
  }

  async function testClickPageAction(doClick, doEnterKey) {
    for (let modifier of Object.keys(map)) {
      for (let i = 0; i < 2; i++) {
        let clickEventData = { button: i };
        clickEventData[modifier] = true;
        await doClick(extension, window, clickEventData);
        let info = await extension.awaitMessage("onClick");

        is(info.button, i, `Correct button on click event`);
        assertSingleModifier(info, modifier);
      }

      let keypressEventData = {};
      keypressEventData[modifier] = true;
      await doEnterKey(extension, keypressEventData);
      let info = await extension.awaitMessage("onClick");

      is(info.button, 0, `Key command emulates left click`);
      assertSingleModifier(info, modifier);
    }
  }

  await extension.startup();
  await extension.awaitMessage("ready");

  if (testAsNonPersistent) {
    assertPersistentListeners(extension, "pageAction", "onClicked", {
      primed: false,
    });
    info("Terminating the background event page");
    await extension.terminateBackground();
    assertPersistentListeners(extension, "pageAction", "onClicked", {
      primed: true,
    });
  }

  info("Clicking the pageAction");
  await testClickPageAction(clickPageAction, triggerPageActionWithKeyboard);

  if (testAsNonPersistent) {
    await extension.awaitMessage("ready");
    assertPersistentListeners(extension, "pageAction", "onClicked", {
      primed: false,
    });
  }

  await testClickPageAction(
    clickPageActionInPanel,
    triggerPageActionWithKeyboardInPanel
  );

  await extension.unload();
}

async function test_clickData_reset(testAsNonPersistent = false) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_area: "navbar",
      },
      page_action: {},
      background: {
        persistent: !testAsNonPersistent,
        scripts: ["background.js"],
      },
    },

    files: {
      "background.js": async function background() {
        function onBrowserActionClicked() {
          // openPopup requires user interaction, such as a browser action click.
          browser.pageAction.openPopup();
        }

        function onPageActionClicked(tab, info) {
          browser.test.sendMessage("onClick", info);
        }

        browser.browserAction.onClicked.addListener(onBrowserActionClicked);
        browser.pageAction.onClicked.addListener(onPageActionClicked);

        let [tab] = await browser.tabs.query({
          active: true,
          currentWindow: true,
        });
        await browser.pageAction.show(tab.id);
        browser.test.sendMessage("ready");
      },
    },
  });

  async function clickPageActionWithModifiers() {
    await clickPageAction(extension, window, { button: 1, shiftKey: true });
    let info = await extension.awaitMessage("onClick");
    is(info.button, 1);
    is(info.modifiers[0], "Shift");
  }

  function assertInfoReset(info) {
    is(info.button, 0, `ClickData button reset properly`);
    is(info.modifiers.length, 0, `ClickData modifiers reset properly`);
  }

  await extension.startup();
  await extension.awaitMessage("ready");

  if (testAsNonPersistent) {
    assertPersistentListeners(extension, "pageAction", "onClicked", {
      primed: false,
    });
    info("Terminating the background event page");
    await extension.terminateBackground();
    assertPersistentListeners(extension, "pageAction", "onClicked", {
      primed: true,
    });
  }

  info("Clicking the pageAction");
  await clickPageActionWithModifiers();

  if (testAsNonPersistent) {
    await extension.awaitMessage("ready");
    assertPersistentListeners(extension, "pageAction", "onClicked", {
      primed: false,
    });
  }

  await clickBrowserAction(extension);
  assertInfoReset(await extension.awaitMessage("onClick"));

  await clickPageActionWithModifiers();

  await triggerPageActionWithKeyboard(extension);
  assertInfoReset(await extension.awaitMessage("onClick"));

  await extension.unload();
}

add_task(function test_clickData_MV2() {
  return test_clickData(/* testAsNonPersistent */ false);
});

add_task(async function test_clickData_MV2_eventPage() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.eventPages.enabled", true]],
  });
  await test_clickData(/* testAsNonPersistent */ true);
  await SpecialPowers.popPrefEnv();
});

add_task(function test_clickData_reset_MV2() {
  return test_clickData_reset(/* testAsNonPersistent */ false);
});

add_task(async function test_clickData_reset_MV2_eventPage() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.eventPages.enabled", true]],
  });
  await test_clickData_reset(/* testAsNonPersistent */ true);
  await SpecialPowers.popPrefEnv();
});
