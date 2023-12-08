/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
requestLongerTimeout(2);

const kBaseUrlForContent = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const kContentFileName = "file_toplevel.html";
const kContentFileUrl = kBaseUrlForContent + kContentFileName;
const kIsMac = navigator.platform.indexOf("Mac") > -1;

async function waitForPasteContextMenu() {
  await waitForPasteMenuPopupEvent("shown");
  let pasteButton = document.getElementById(kPasteMenuItemId);
  info("Wait for paste button enabled");
  await BrowserTestUtils.waitForMutationCondition(
    pasteButton,
    { attributeFilter: ["disabled"] },
    () => !pasteButton.disabled,
    "Wait for paste button enabled"
  );
}

async function readText(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], async () => {
    content.document.notifyUserGestureActivation();
    return content.eval(`navigator.clipboard.readText();`);
  });
}

async function testPasteContextMenu(
  aBrowser,
  aClipboardText,
  aShouldShow = true
) {
  let pasteButtonIsShown;
  if (aShouldShow) {
    pasteButtonIsShown = waitForPasteContextMenu();
  }
  let readTextRequest = readText(aBrowser);
  if (aShouldShow) {
    await pasteButtonIsShown;
  }

  info("Click paste button, request should be resolved");
  if (aShouldShow) {
    await promiseClickPasteButton();
  }
  is(await readTextRequest, aClipboardText, "Request should be resolved");
}

async function installAndStartExtension(aContentScript) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          js: ["content_script.js"],
          matches: ["https://example.com/*/file_toplevel.html"],
        },
      ],
    },
    files: {
      "content_script.js": aContentScript,
    },
  });

  await extension.startup();

  return extension;
}

function testExtensionContentScript(aContentScript, aMessage) {
  add_task(async function test_context_menu_suppression_ext() {
    info(`${aMessage}`);
    const extension = await installAndStartExtension(aContentScript);
    await BrowserTestUtils.withNewTab(
      kContentFileUrl,
      async function (browser) {
        const clipboardText = "X" + Math.random();
        await SpecialPowers.spawn(browser, [clipboardText], async text => {
          info(`Set clipboard text to ${text}`);
          let div = content.document.createElement("div");
          div.id = "container";
          div.innerText = text;
          content.document.documentElement.appendChild(div);
        });

        let writePromise = extension.awaitMessage("write-data-ready");
        // trigger keyboard shortcut to copy.
        await EventUtils.synthesizeAndWaitKey(
          "c",
          kIsMac ? { accelKey: true } : { ctrlKey: true }
        );
        // Wait a bit for clipboard write.
        await writePromise;

        info("Test read from same frame");
        await testPasteContextMenu(browser, clipboardText, false);

        info("Test read from same-origin subframe");
        await testPasteContextMenu(
          browser.browsingContext.children[0],
          clipboardText,
          false
        );

        info("Test read from cross-origin subframe");
        await testPasteContextMenu(
          browser.browsingContext.children[1],
          clipboardText,
          true
        );
      }
    );

    await extension.unload();
  });
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.events.asyncClipboard.readText", true],
      ["dom.events.asyncClipboard.clipboardItem", true],
      ["test.events.async.enabled", true],
      // Avoid paste button delay enabling making test too long.
      ["security.dialog_enable_delay", 0],
    ],
  });
});

testExtensionContentScript(() => {
  document.addEventListener("copy", function (e) {
    e.preventDefault();
    let div = document.getElementById("container");
    let text = div.innerText;
    e.clipboardData.setData("text/plain", text);
    browser.test.sendMessage("write-data-ready");
  });
}, "Write data by DataTransfer API in extension");

testExtensionContentScript(() => {
  document.addEventListener("copy", async function (e) {
    e.preventDefault();
    let div = document.getElementById("container");
    let text = div.innerText;
    await navigator.clipboard.writeText(text);
    browser.test.sendMessage("write-data-ready");
  });
}, "Write data by Async Clipboard API in extension");
