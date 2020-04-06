/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_clickData() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {},
    },

    background() {
      function onClicked(tab, info) {
        let button = info.button;
        let modifiers = info.modifiers;
        browser.test.sendMessage("onClick", { button, modifiers });
      }

      browser.browserAction.onClicked.addListener(onClicked);
      browser.test.sendMessage("ready");
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

  for (let area of [CustomizableUI.AREA_NAVBAR, getCustomizableUIPanelID()]) {
    let widget = getBrowserActionWidget(extension);
    CustomizableUI.addWidgetToArea(widget.id, area);

    for (let modifier of Object.keys(map)) {
      for (let i = 0; i < 2; i++) {
        // On Mac, ctrl-click will send a context menu event from the widget,
        // we won't send xul command event and won't have onClick message, either.
        if (
          AppConstants.platform == "macosx" &&
          i == 0 &&
          modifier == "ctrlKey"
        ) {
          continue;
        }

        let clickEventData = { button: i };
        clickEventData[modifier] = true;
        await clickBrowserAction(extension, window, clickEventData);
        let info = await extension.awaitMessage("onClick");

        is(info.button, i, `Correct button in ${area} click`);
        assertSingleModifier(info, modifier, area);
      }

      let keypressEventData = {};
      keypressEventData[modifier] = true;
      await triggerBrowserActionWithKeyboard(
        extension,
        "KEY_Enter",
        keypressEventData
      );
      let info = await extension.awaitMessage("onClick");

      is(info.button, 0, `Key command emulates left click`);
      assertSingleModifier(info, modifier, area);
    }
  }

  await extension.unload();
});

add_task(async function test_clickData_reset() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {},
      page_action: {},
    },

    async background() {
      function onBrowserActionClicked(tab, info) {
        browser.test.sendMessage("onClick", info);
      }

      function onPageActionClicked(tab, info) {
        // openPopup requires user interaction, such as a page action click.
        browser.browserAction.openPopup();
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
  });

  // Pollute the state of the browserAction's lastClickInfo
  async function clickBrowserActionWithModifiers() {
    await clickBrowserAction(extension, window, { button: 1, shiftKey: true });
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

  await clickBrowserActionWithModifiers();

  await clickPageAction(extension);
  assertInfoReset(await extension.awaitMessage("onClick"));

  await clickBrowserActionWithModifiers();

  await triggerBrowserActionWithKeyboard(extension, "KEY_Enter");
  assertInfoReset(await extension.awaitMessage("onClick"));

  await clickBrowserActionWithModifiers();

  await triggerBrowserActionWithKeyboard(extension, " ");
  assertInfoReset(await extension.awaitMessage("onClick"));

  await extension.unload();
});
