/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests less common mouse/keyboard manipulations of the address bar input
 * field selection, for example:
 *  - Home/Del
 *  - Shift+Right/Left
 *  - Drag selection
 *  - Double-click on word
 *
 * All the tests set up some initial conditions, and check it. Then optionally
 * they can manipulate the selection further, and check the results again.
 * We want to ensure the final selection is the expected one, even if in the
 * future we change our trimming strategy for the input field value.
 */

const tests = [
  {
    description: "Test HOME starting from full selection",
    openPanel() {
      EventUtils.synthesizeKey("l", { accelKey: true });
    },
    get selection() {
      return [0, gURLBar.value.length];
    },
    manipulate() {
      // Cursor must move to the first visible character, regardless of any
      // "untrimming" we could be doing.
      this._visibleValue = gURLBar.value;
      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey("KEY_ArrowLeft", { metaKey: true });
      } else {
        EventUtils.synthesizeKey("KEY_Home");
      }
    },
    get modifiedSelection() {
      let start = gURLBar.value.indexOf(this._visibleValue);
      return [start, start];
    },
  },
  {
    description: "Test END starting from full selection",
    openPanel() {
      EventUtils.synthesizeKey("l", { accelKey: true });
    },
    get selection() {
      return [0, gURLBar.value.length];
    },
    manipulate() {
      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey("KEY_ArrowRight", { metaKey: true });
      } else {
        EventUtils.synthesizeKey("KEY_End", {});
      }
    },
    get modifiedSelection() {
      return [gURLBar.value.length, gURLBar.value.length];
    },
  },
  {
    description: "Test SHIFT+LEFT starting from full selection",
    openPanel() {
      EventUtils.synthesizeKey("l", { accelKey: true });
    },
    get selection() {
      return [0, gURLBar.value.length];
    },
    manipulate() {
      EventUtils.synthesizeKey("KEY_ArrowLeft", { shiftKey: true });
    },
    get modifiedSelection() {
      return [0, gURLBar.value.length - 1];
    },
  },
  {
    description: "Test SHIFT+RIGHT starting from full selection",
    openPanel() {
      EventUtils.synthesizeKey("l", { accelKey: true });
    },
    get selection() {
      return [0, gURLBar.value.length];
    },
    manipulate() {
      EventUtils.synthesizeKey("KEY_ArrowRight", { shiftKey: true });
    },
    get modifiedSelection() {
      return [0, gURLBar.value.length];
    },
  },
  {
    description: "Test Drag Selection from the first character",
    async openPanel() {
      this._expectedSelectedText = gURLBar.value.substring(0, 5);
      await selectWithMouseDrag(
        getTextWidth(gURLBar.value[0]) / 2 - 1,
        getTextWidth(gURLBar.value.substring(0, 5))
      );
    },
    get selection() {
      return [
        0,
        gURLBar.value.indexOf(this._expectedSelectedText) +
          this._expectedSelectedText.length,
      ];
    },
  },
  {
    description: "Test Drag Selection from the last character",
    async openPanel() {
      this._expectedSelectedText = gURLBar.value.substring(-5);
      await selectWithMouseDrag(
        getTextWidth(gURLBar.value) + 1,
        getTextWidth(this._expectedSelectedText)
      );
    },
    get selection() {
      return [
        gURLBar.value.indexOf(this._expectedSelectedText),
        gURLBar.value.length,
      ];
    },
  },
  {
    description: "Test Drag Selection in the middle of the string",
    async openPanel() {
      this._expectedSelectedText = gURLBar.value.substring(5, 10);
      await selectWithMouseDrag(
        getTextWidth(gURLBar.value.substring(0, 5)),
        getTextWidth(gURLBar.value.substring(0, 10))
      );
    },
    get selection() {
      let start = gURLBar.value.indexOf(this._expectedSelectedText);
      return [start, start + this._expectedSelectedText.length];
    },
  },
  {
    description: "Test Double-click on word",
    async openPanel() {
      let wordBoundaryIndex = gURLBar.value.search(/\btest/);
      this._expectedSelectedText = "test";
      await selectWithDoubleClick(
        getTextWidth(gURLBar.value.substring(0, wordBoundaryIndex))
      );
    },
    get selection() {
      let start = gURLBar.value.indexOf(this._expectedSelectedText);
      return [start, start + this._expectedSelectedText.length];
    },
  },
  {
    description: "Click at the right of the text",
    openPanel() {
      EventUtils.synthesizeKey("l", { accelKey: true });
    },
    get selection() {
      return [0, gURLBar.value.length];
    },
    manipulate() {
      let rect = gURLBar.inputField.getBoundingClientRect();
      EventUtils.synthesizeMouse(
        gURLBar.inputField,
        getTextWidth(gURLBar.value) + 10,
        rect.height / 2,
        {}
      );
    },
    get modifiedSelection() {
      return [gURLBar.value.length, gURLBar.value.length];
    },
  },
];

add_setup(async function () {
  gURLBar.inputField.style.font = "14px monospace";
  registerCleanupFunction(() => {
    gURLBar.inputField.style.font = null;
  });
});

add_task(async function https() {
  await doTest("https://example.com/test/some/page.htm");
});

add_task(async function http() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await doTest("http://example.com/test/other/page.htm");
});

async function doTest(url) {
  await BrowserTestUtils.withNewTab(url, async () => {
    for (let test of tests) {
      gURLBar.blur();
      info(test.description);
      await UrlbarTestUtils.promisePopupOpen(window, async () => {
        await test.openPanel();
      });
      info(
        `Selected text is <${gURLBar.value.substring(
          gURLBar.selectionStart,
          gURLBar.selectionEnd
        )}>`
      );
      Assert.deepEqual(
        test.selection,
        [gURLBar.selectionStart, gURLBar.selectionEnd],
        "Check selection"
      );

      if (test.manipulate) {
        await test.manipulate();
        info(
          `Selected text is <${gURLBar.value.substring(
            gURLBar.selectionStart,
            gURLBar.selectionEnd
          )}>`
        );
        Assert.deepEqual(
          test.modifiedSelection,
          [gURLBar.selectionStart, gURLBar.selectionEnd],
          "Check selection after manipulation"
        );
      }
    }
  });
}

function getTextWidth(inputText) {
  const canvas =
    getTextWidth.canvas ||
    (getTextWidth.canvas = document.createElement("canvas"));
  let context = canvas.getContext("2d");
  context.font = window
    .getComputedStyle(gURLBar.inputField)
    .getPropertyValue("font");
  return context.measureText(inputText).width;
}
