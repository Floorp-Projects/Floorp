/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Like focusButtonAndPressKey, but leaves time between keydown and keyup
// rather than dispatching both synchronously. This allows time for the
// element's `open` property to go back to being `false` if forced to true
// synchronously in response to keydown.
async function focusButtonAndPressKeyWithDelay(key, elem, modifiers) {
  elem.setAttribute("tabindex", "-1");
  elem.focus();
  elem.removeAttribute("tabindex");

  EventUtils.synthesizeKey(key, { type: "keydown", modifiers });
  await new Promise(executeSoon);
  EventUtils.synthesizeKey(key, { type: "keyup", modifiers });
  elem.blur();
}

// This test verifies that pressing enter while a page action is focused
// triggers the action once and only once.
add_task(async function testKeyBrowserAction() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {},
      background: {
        persistent: false,
        scripts: ["background.js"],
      },
    },

    files: {
      "background.js": async function background() {
        let counter = 0;

        function onClicked() {
          dump("clicked\n");
          counter++;
        }

        browser.browserAction.onClicked.addListener(onClicked);

        browser.test.onMessage.addListener(async msg => {
          browser.test.sendMessage("counter", counter);
        });

        browser.test.sendMessage("ready");
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  let elem = getBrowserActionWidget(extension).forWindow(window).node;
  await focusButtonAndPressKeyWithDelay(" ", elem, {});

  extension.sendMessage("checkCounter", {});
  let counter = await extension.awaitMessage("counter");
  is(counter, 1, "Key only triggered button once");

  await extension.unload();
});
