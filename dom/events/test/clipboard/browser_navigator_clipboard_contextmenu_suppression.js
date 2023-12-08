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

function testPasteContextMenuSuppression(aWriteFun, aMsg) {
  add_task(async function test_context_menu_suppression_sameorigin() {
    await BrowserTestUtils.withNewTab(
      kContentFileUrl,
      async function (browser) {
        info(`Write data by ${aMsg}`);
        let clipboardText = await aWriteFun(browser);

        info("Test read from same-origin frame");
        let listener = function (e) {
          if (e.target.getAttribute("id") == kPasteMenuPopupId) {
            ok(false, "paste contextmenu should not be shown");
          }
        };
        document.addEventListener("popupshown", listener);
        is(
          await readText(browser.browsingContext.children[0]),
          clipboardText,
          "read should just be resolved without paste contextmenu shown"
        );
        document.removeEventListener("popupshown", listener);
      }
    );
  });

  add_task(async function test_context_menu_suppression_crossorigin() {
    await BrowserTestUtils.withNewTab(
      kContentFileUrl,
      async function (browser) {
        info(`Write data by ${aMsg}`);
        let clipboardText = await aWriteFun(browser);

        info("Test read from cross-origin frame");
        let pasteButtonIsShown = waitForPasteContextMenu();
        let readTextRequest = readText(browser.browsingContext.children[1]);
        await pasteButtonIsShown;

        info("Click paste button, request should be resolved");
        await promiseClickPasteButton();
        is(await readTextRequest, clipboardText, "Request should be resolved");
      }
    );
  });

  add_task(async function test_context_menu_suppression_multiple() {
    await BrowserTestUtils.withNewTab(
      kContentFileUrl,
      async function (browser) {
        info(`Write data by ${aMsg}`);
        let clipboardText = await aWriteFun(browser);

        info("Test read from cross-origin frame");
        let pasteButtonIsShown = waitForPasteContextMenu();
        let readTextRequest1 = readText(browser.browsingContext.children[1]);
        await pasteButtonIsShown;

        info(
          "Test read from same-origin frame before paste contextmenu is closed"
        );
        is(
          await readText(browser.browsingContext.children[0]),
          clipboardText,
          "read from same-origin should just be resolved without showing paste contextmenu shown"
        );

        info("Dismiss paste button, cross-origin request should be rejected");
        await promiseDismissPasteButton();
        await Assert.rejects(
          readTextRequest1,
          /NotAllowedError/,
          "cross-origin request should be rejected"
        );
      }
    );
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

testPasteContextMenuSuppression(async aBrowser => {
  const clipboardText = "X" + Math.random();
  await SpecialPowers.spawn(aBrowser, [clipboardText], async text => {
    content.document.notifyUserGestureActivation();
    return content.eval(`navigator.clipboard.writeText("${text}");`);
  });
  return clipboardText;
}, "clipboard.writeText()");

testPasteContextMenuSuppression(async aBrowser => {
  const clipboardText = "X" + Math.random();
  await SpecialPowers.spawn(aBrowser, [clipboardText], async text => {
    content.document.notifyUserGestureActivation();
    return content.eval(`
      const itemInput = new ClipboardItem({["text/plain"]: "${text}"});
      navigator.clipboard.write([itemInput]);
    `);
  });
  return clipboardText;
}, "clipboard.write()");

testPasteContextMenuSuppression(async aBrowser => {
  const clipboardText = "X" + Math.random();
  await SpecialPowers.spawn(aBrowser, [clipboardText], async text => {
    let div = content.document.createElement("div");
    div.innerText = text;
    content.document.documentElement.appendChild(div);
    // select text
    content
      .getSelection()
      .setBaseAndExtent(div.firstChild, text.length, div.firstChild, 0);
  });
  // trigger keyboard shortcut to copy.
  await EventUtils.synthesizeAndWaitKey(
    "c",
    kIsMac ? { accelKey: true } : { ctrlKey: true }
  );
  return clipboardText;
}, "keyboard shortcut");

testPasteContextMenuSuppression(async aBrowser => {
  const clipboardText = "X" + Math.random();
  await SpecialPowers.spawn(aBrowser, [clipboardText], async text => {
    return content.eval(`
      document.addEventListener("copy", function(e) {
        e.preventDefault();
        e.clipboardData.setData("text/plain", "${text}");
      }, { once: true });
    `);
  });
  // trigger keyboard shortcut to copy.
  await EventUtils.synthesizeAndWaitKey(
    "c",
    kIsMac ? { accelKey: true } : { ctrlKey: true }
  );
  return clipboardText;
}, "keyboard shortcut with custom data");

testPasteContextMenuSuppression(async aBrowser => {
  const clipboardText = "X" + Math.random();
  await SpecialPowers.spawn(aBrowser, [clipboardText], async text => {
    let div = content.document.createElement("div");
    div.innerText = text;
    content.document.documentElement.appendChild(div);
    // select text
    content
      .getSelection()
      .setBaseAndExtent(div.firstChild, text.length, div.firstChild, 0);
    return SpecialPowers.doCommand(content, "cmd_copy");
  });
  return clipboardText;
}, "copy command");

async function readTypes(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [], async () => {
    content.document.notifyUserGestureActivation();
    let items = await content.eval(`navigator.clipboard.read();`);
    return items[0].types;
  });
}

add_task(async function test_context_menu_suppression_image() {
  await BrowserTestUtils.withNewTab(kContentFileUrl, async function (browser) {
    await SpecialPowers.spawn(browser, [], async () => {
      let image = content.document.createElement("img");
      let copyImagePromise = new Promise(resolve => {
        image.addEventListener(
          "load",
          e => {
            let documentViewer = content.docShell.docViewer.QueryInterface(
              SpecialPowers.Ci.nsIDocumentViewerEdit
            );
            documentViewer.setCommandNode(image);
            documentViewer.copyImage(documentViewer.COPY_IMAGE_ALL);
            resolve();
          },
          { once: true }
        );
      });
      image.src =
        "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAD4AAABHCAIAAADQjmMaAA" +
        "AACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3goUAwAgSAORBwAAABl0RVh0Q29tbW" +
        "VudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAABPSURBVGje7c4BDQAACAOga//OmuMbJG" +
        "AurTbq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6u" +
        "rq6s31B0IqAY2/tQVCAAAAAElFTkSuQmCC";
      content.document.documentElement.appendChild(image);
      await copyImagePromise;
    });

    info("Test read from cross-origin frame");
    let pasteButtonIsShown = waitForPasteContextMenu();
    let readTypesRequest1 = readTypes(browser.browsingContext.children[1]);
    await pasteButtonIsShown;

    info("Test read from same-origin frame before paste contextmenu is closed");
    const clipboarCacheEnabled = SpecialPowers.getBoolPref(
      "widget.clipboard.use-cached-data.enabled",
      false
    );
    // If the cached data is used, it uses type order in cached transferable.
    SimpleTest.isDeeply(
      await readTypes(browser.browsingContext.children[0]),
      clipboarCacheEnabled
        ? ["text/plain", "text/html", "image/png"]
        : ["text/html", "text/plain", "image/png"],
      "read from same-origin should just be resolved without showing paste contextmenu shown"
    );

    info("Dismiss paste button, cross-origin request should be rejected");
    await promiseDismissPasteButton();
    // XXX edgar: not sure why first promiseDismissPasteButton doesn't work on Windows opt build.
    await promiseDismissPasteButton();
    await Assert.rejects(
      readTypesRequest1,
      /NotAllowedError/,
      "cross-origin request should be rejected"
    );
  });
});
