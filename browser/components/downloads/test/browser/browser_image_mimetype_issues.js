/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

/*
 * Popular websites implement image optimization as serving files with
 * extension ".jpg" but content type "image/webp". If we save such images,
 * we should actually save them with a .webp extension as that is what
 * they are.
 */

/**
 * Test the above with the "save image as" context menu.
 */
add_task(async function test_save_image_webp_with_jpeg_extension() {
  await BrowserTestUtils.withNewTab(
    `data:text/html,<img src="${TEST_ROOT}/not-really-a-jpeg.jpeg?convert=webp">`,
    async browser => {
      let menu = document.getElementById("contentAreaContextMenu");
      let popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      BrowserTestUtils.synthesizeMouse(
        "img",
        5,
        5,
        { type: "contextmenu", button: 2 },
        browser
      );
      await popupShown;

      await new Promise(resolve => {
        MockFilePicker.showCallback = function(fp) {
          ok(
            fp.defaultString.endsWith("webp"),
            `filepicker for image has "${fp.defaultString}", should end in webp`
          );
          setTimeout(resolve, 0);
          return Ci.nsIFilePicker.returnCancel;
        };
        let menuitem = menu.querySelector("#context-saveimage");
        menu.activateItem(menuitem);
      });
    }
  );
});

/**
 * Test with the "save link as" context menu.
 */
add_task(async function test_save_link_webp_with_jpeg_extension() {
  await BrowserTestUtils.withNewTab(
    `data:text/html,<a href="${TEST_ROOT}/not-really-a-jpeg.jpeg?convert=webp">Nice image</a>`,
    async browser => {
      let menu = document.getElementById("contentAreaContextMenu");
      let popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      BrowserTestUtils.synthesizeMouse(
        "a[href]",
        5,
        5,
        { type: "contextmenu", button: 2 },
        browser
      );
      await popupShown;

      await new Promise(resolve => {
        MockFilePicker.showCallback = function(fp) {
          ok(
            fp.defaultString.endsWith("webp"),
            `filepicker for link has "${fp.defaultString}", should end in webp`
          );
          setTimeout(resolve, 0);
          return Ci.nsIFilePicker.returnCancel;
        };
        let menuitem = menu.querySelector("#context-savelink");
        menu.activateItem(menuitem);
      });
    }
  );
});

/**
 * Test with the main "save page" command.
 */
add_task(async function test_save_page_on_image_document() {
  await BrowserTestUtils.withNewTab(
    `${TEST_ROOT}/not-really-a-jpeg.jpeg?convert=webp`,
    async browser => {
      await new Promise(resolve => {
        MockFilePicker.showCallback = function(fp) {
          ok(
            fp.defaultString.endsWith("webp"),
            `filepicker for "save page" has "${fp.defaultString}", should end in webp`
          );
          setTimeout(resolve, 0);
          return Ci.nsIFilePicker.returnCancel;
        };
        document.getElementById("Browser:SavePage").doCommand();
      });
    }
  );
});

/**
 * Make sure that a valid JPEG image using the .JPG extension doesn't
 * get it replaced with .jpeg.
 */
add_task(async function test_save_page_on_JPEG_image_document() {
  await BrowserTestUtils.withNewTab(`${TEST_ROOT}/blank.JPG`, async browser => {
    await new Promise(resolve => {
      MockFilePicker.showCallback = function(fp) {
        ok(
          fp.defaultString.endsWith("JPG"),
          `filepicker for "save page" has "${fp.defaultString}", should end in JPG`
        );
        setTimeout(resolve, 0);
        return Ci.nsIFilePicker.returnCancel;
      };
      document.getElementById("Browser:SavePage").doCommand();
    });
  });
});
