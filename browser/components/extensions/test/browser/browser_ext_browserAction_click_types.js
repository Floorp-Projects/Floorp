/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function test_clickData({ manifest_version, persistent }) {
  const action = manifest_version < 3 ? "browser_action" : "action";
  const background = { scripts: ["background.js"] };

  if (persistent != null) {
    background.persistent = persistent;
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version,
      [action]: {},
      background,
    },

    files: {
      "background.js": function backgroundScript() {
        function onClicked(tab, info) {
          let button = info.button;
          let modifiers = info.modifiers;
          browser.test.sendMessage("onClick", { button, modifiers });
        }

        const apiNS =
          browser.runtime.getManifest().manifest_version >= 3
            ? "action"
            : "browserAction";

        browser[apiNS].onClicked.addListener(onClicked);
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

  function assertSingleModifier(info, modifier, area) {
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

    is(info.modifiers[0], map[modifier], `Correct modifier on ${area} click`);
  }

  await extension.startup();
  await extension.awaitMessage("ready");

  if (manifest_version >= 3 || persistent === false) {
    // NOTE: even in MV3 where the API namespace is technically "action",
    // the event listeners will be persisted into the startup data
    // with "browserAction" as the module, because that is the name
    // of the module shared by both MV2 browserAction and MV3 action APIs.
    assertPersistentListeners(extension, "browserAction", "onClicked", {
      primed: false,
    });
    await extension.terminateBackground();
    assertPersistentListeners(extension, "browserAction", "onClicked", {
      primed: true,
    });
  }

  for (let area of [CustomizableUI.AREA_NAVBAR, getCustomizableUIPanelID()]) {
    let widget = getBrowserActionWidget(extension);
    CustomizableUI.addWidgetToArea(widget.id, area);

    for (let modifier of Object.keys(map)) {
      for (let i = 0; i < 2; i++) {
        let clickEventData = { button: i };
        clickEventData[modifier] = true;
        await clickBrowserAction(extension, window, clickEventData);
        let info = await extension.awaitMessage("onClick");

        is(info.button, i, `Correct button in ${area} click`);
        assertSingleModifier(info, modifier, area);
      }

      let keypressEventData = {};
      keypressEventData[modifier] = true;
      await triggerBrowserActionWithKeyboard(extension, " ", keypressEventData);
      let info = await extension.awaitMessage("onClick");

      is(info.button, 0, `Key command emulates left click`);
      assertSingleModifier(info, modifier, area);
    }
  }

  if (manifest_version >= 3 || persistent === false) {
    // The background event page is expected to have been
    // spawned again to handle the action onClicked event.
    await extension.awaitMessage("ready");
  }

  await extension.unload();
}

async function test_clickData_reset({ manifest_version }) {
  const action = manifest_version < 3 ? "browser_action" : "action";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version,
      [action]: {},
      page_action: {},
    },

    async background() {
      function onBrowserActionClicked(tab, info) {
        browser.test.sendMessage("onClick", info);
      }

      function onPageActionClicked(tab, info) {
        // openPopup requires user interaction, such as a page action click.
        // NOTE: this triggers the browserAction onClicked event as a side-effect
        // of triggering the browserAction popup through browserAction.openPopup.
        browser.browserAction.openPopup();
      }

      const { manifest_version } = browser.runtime.getManifest();
      const apiNS = manifest_version >= 3 ? "action" : "browserAction";

      browser[apiNS].onClicked.addListener(onBrowserActionClicked);

      // pageAction should only be available in MV2 extensions.
      if (manifest_version < 3) {
        browser.pageAction.onClicked.addListener(onPageActionClicked);

        let [tab] = await browser.tabs.query({
          active: true,
          currentWindow: true,
        });
        await browser.pageAction.show(tab.id);
      }

      browser.test.sendMessage("ready");
    },
  });

  // Pollute the state of the browserAction's lastClickInfo
  async function clickBrowserActionWithModifiers() {
    await clickBrowserAction(extension, window, { button: 1, shiftKey: true });
    let info = await extension.awaitMessage("onClick");
    is(info.button, 1, "Got expected ClickData button details");
    is(info.modifiers[0], "Shift", "Got expected ClickData modifiers details");
  }

  function assertInfoReset(info) {
    is(info.button, 0, `ClickData button reset properly`);
    is(info.modifiers.length, 0, `ClickData modifiers reset properly`);
  }

  await extension.startup();
  await extension.awaitMessage("ready");

  if (manifest_version >= 3) {
    // NOTE: even in MV3 where the API namespace is technically "action",
    // the event listeners will be persisted into the startup data
    // with "browserAction" as the module, because that is the name
    // of the module shared by both MV2 browserAction and MV3 action APIs.
    assertPersistentListeners(extension, "browserAction", "onClicked", {
      primed: false,
    });
    await extension.terminateBackground();
    assertPersistentListeners(extension, "browserAction", "onClicked", {
      primed: true,
    });
  }

  await clickBrowserActionWithModifiers();

  if (manifest_version >= 3) {
    // The background event page is expected to have been
    // spawned again to handle the action onClicked event.
    await extension.awaitMessage("ready");
  } else {
    // pageAction should only be available in MV2 extensions.
    await clickPageAction(extension);
    // NOTE: the pageAction event listener then triggers browserAction.onClicked
    // as a side effect of calling browserAction.openPopup.
    assertInfoReset(await extension.awaitMessage("onClick"));
  }

  await clickBrowserActionWithModifiers();

  await triggerBrowserActionWithKeyboard(extension, " ");
  assertInfoReset(await extension.awaitMessage("onClick"));

  await clickBrowserActionWithModifiers();

  await triggerBrowserActionWithKeyboard(extension, " ");
  assertInfoReset(await extension.awaitMessage("onClick"));

  await extension.unload();
}

add_task(function test_clickData_MV2() {
  return test_clickData({ manifest_version: 2 });
});

add_task(async function test_clickData_MV2_eventpage() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.eventPages.enabled", true]],
  });
  await test_clickData({
    manifest_version: 2,
    persistent: false,
  });
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_clickData_MV3() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });
  await test_clickData({ manifest_version: 3 });
  await SpecialPowers.popPrefEnv();
});

add_task(function test_clickData_reset_MV2() {
  return test_clickData_reset({ manifest_version: 2 });
});

add_task(async function test_clickData_reset_MV3() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });
  await test_clickData_reset({ manifest_version: 3 });
  await SpecialPowers.popPrefEnv();
});
