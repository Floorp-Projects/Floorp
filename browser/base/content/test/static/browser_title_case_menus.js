/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test file checks that our en-US builds use APA-style Title Case strings
 * where appropriate.
 */

// MINOR_WORDS are words that are okay to not be capitalized when they're
// mid-string.
//
// Source: https://apastyle.apa.org/style-grammar-guidelines/capitalization/title-case
const MINOR_WORDS = [
  "a",
  "an",
  "and",
  "as",
  "at",
  "but",
  "by",
  "for",
  "if",
  "in",
  "nor",
  "of",
  "off",
  "on",
  "or",
  "per",
  "so",
  "the",
  "to",
  "up",
  "via",
  "yet",
];

/**
 * Returns a generator that will yield all of the <xul:menupopups>
 * beneath <xul:menu> elements within a given <xul:menubar>. Each
 * <xul:menupopup> will have the "popupshowing" and "popupshown"
 * event fired on them to give them an opportunity to fully populate
 * themselves before being yielded.
 *
 * @generator
 * @param {<xul:menubar>} menubar The <xul:menubar> to get <xul:menupopup>s
 *        for.
 * @yields {<xul:menupopup>} The next <xul:menupopup> under the <xul:menubar>.
 */
async function* iterateMenuPopups(menubar) {
  let menus = menubar.querySelectorAll("menu");

  for (let menu of menus) {
    for (let menupopup of menu.querySelectorAll("menupopup")) {
      // We fake the popupshowing and popupshown events to give the menupopups
      // an opportunity to fully populate themselves. We don't actually open
      // the menupopups because this is not possible on macOS.
      menupopup.dispatchEvent(
        new MouseEvent("popupshowing", { bubbles: true })
      );
      menupopup.dispatchEvent(new MouseEvent("popupshown", { bubbles: true }));

      yield menupopup;

      // Just for good measure, we'll fire the popuphiding/popuphidden events
      // after we close the menupopups.
      menupopup.dispatchEvent(new MouseEvent("popuphiding", { bubbles: true }));
      menupopup.dispatchEvent(new MouseEvent("popuphidden", { bubbles: true }));
    }
  }
}

/**
 * Given a <xul:menupopup>, checks all of the child elements with label
 * properties to see if those labels are Title Cased. Skips any elements that
 * have an empty or undefined label property.
 *
 * @param {<xul:menupopup>} menupopup The <xul:menupopup> to check.
 */
function checkMenuItems(menupopup) {
  info("Checking menupopup with id " + menupopup.id);
  for (let child of menupopup.children) {
    if (child.label) {
      info("Checking menupopup child with id " + child.id);
      checkTitleCase(child.label, child.id);
    }
  }
}

/**
 * Given a string, checks that the string is in Title Case.
 *
 * @param {String} string The string to check.
 * @param {String} elementID The ID of the element associated with the string.
 *        This is included in the assertion message.
 */
function checkTitleCase(string, elementID) {
  if (!string || !elementID /* document this */) {
    return;
  }

  let words = string.trim().split(/\s+/);

  // We extract the first word, and always expect it to be capitalized,
  // even if it's a short word like one of MINOR_WORDS.
  let firstWord = words.shift();
  let result = hasExpectedCapitalization(firstWord, true);
  if (result) {
    for (let word of words) {
      if (word) {
        let expectCapitalized = !MINOR_WORDS.includes(word);
        result = hasExpectedCapitalization(word, expectCapitalized);
        if (!result) {
          break;
        }
      }
    }
  }

  Assert.ok(result, `${string} for ${elementID} should have Title Casing.`);
}

/**
 * On Windows, macOS and GTK/KDE Linux, menubars are expected to be in Title
 * Case in order to feel native. This test iterates the menuitem labels of the
 * main menubar to ensure the en-US strings are all in Title Case.
 *
 * We use APA-style Title Case for the menubar, rather than Photon-style Title
 * Case (https://design.firefox.com/photon/copy/capitalization.html) to match
 * the native platform conventions.
 */
add_task(async function apa_test_title_case_menubar() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menuToolbar = newWin.document.getElementById("main-menubar");

  for await (const menupopup of iterateMenuPopups(menuToolbar)) {
    checkMenuItems(menupopup);
  }

  await BrowserTestUtils.closeWindow(newWin);
});

/**
 * This test iterates the menuitem labels of the macOS dock menu for the
 * application to ensure the en-US strings are all in Title Case.
 */
add_task(async function apa_test_title_case_macos_dock_menu() {
  if (AppConstants.platform != "macosx") {
    return;
  }

  let hiddenWindow = Services.appShell.hiddenDOMWindow;
  Assert.ok(hiddenWindow, "Could get at hidden window");
  let menupopup = hiddenWindow.document.getElementById("menu_mac_dockmenu");
  checkMenuItems(menupopup);
});
