/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function setup() {
  // The page action button is hidden by default for proton.
  // This tests the use of pageAction when the button is visible.
  //
  // TODO(Bug 1704171): this should technically be removed in a follow up
  // and the tests in this file adapted to keep into account that:
  // - in Proton the pageAction is pinned on the urlbar by default
  //   when shown, and hidden when is not available (same for the
  //   overflow menu when enabled)
  // - with Proton disabled, the pageAction is always part of the overflow
  //   panel (either if the pageAction is enabled or disabled) and on the urlbar
  //   only if explicitly pinned.
  if (gProton) {
    BrowserPageActions.mainButtonNode.style.visibility = "visible";
    registerCleanupFunction(() => {
      BrowserPageActions.mainButtonNode.style.removeProperty("visibility");
    });
  }
});

add_task(async function test_clickData() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {},
    },

    async background() {
      function onClicked(tab, info) {
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

  await testClickPageAction(clickPageAction, triggerPageActionWithKeyboard);
  await testClickPageAction(
    clickPageActionInPanel,
    triggerPageActionWithKeyboardInPanel
  );

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

  await clickPageActionWithModifiers();

  await clickBrowserAction(extension);
  assertInfoReset(await extension.awaitMessage("onClick"));

  await clickPageActionWithModifiers();

  await triggerPageActionWithKeyboard(extension);
  assertInfoReset(await extension.awaitMessage("onClick"));

  await extension.unload();
});

add_task(async function test_click_disabled() {
  // In Proton the disabled pageAction are hidden in the urlbar
  // and in the overflow menu, and so the pageAction context menu
  // cannot be triggered on a disabled pageACtion.
  //
  // When we will sunset the proton about:config pref, this test
  // won't be necessary anymore since the user won't be able to
  // open the context menu on disabled actions.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.proton.enabled", false]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {},
    },

    background() {
      let expectClick = false;
      function onClicked(tab, info) {
        if (expectClick) {
          browser.test.sendMessage("onClick");
        } else {
          browser.test.fail(
            `Unexpected click on disabled page action, button=${info.button}`
          );
        }
      }

      async function onMessage(msg, toggle) {
        if (msg == "hide" || msg == "show") {
          expectClick = msg == "show";

          let [tab] = await browser.tabs.query({
            active: true,
            currentWindow: true,
          });
          if (expectClick) {
            await browser.pageAction.show(tab.id);
          } else {
            await browser.pageAction.hide(tab.id);
          }
          browser.test.sendMessage("visibilitySet");
        } else {
          browser.test.fail("Unexpected message");
        }
      }

      browser.pageAction.onClicked.addListener(onClicked);
      browser.test.onMessage.addListener(onMessage);
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  extension.sendMessage("hide");
  await extension.awaitMessage("visibilitySet");

  await clickPageActionInPanel(extension, window, { button: 0 });
  await clickPageActionInPanel(extension, window, { button: 1 });

  extension.sendMessage("show");
  await extension.awaitMessage("visibilitySet");

  await clickPageActionInPanel(extension, window, { button: 0 });
  await extension.awaitMessage("onClick");
  await clickPageActionInPanel(extension, window, { button: 1 });
  await extension.awaitMessage("onClick");

  // Undo the Proton pref change.
  await SpecialPowers.popPrefEnv();
  await extension.unload();
});
