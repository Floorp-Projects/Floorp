"use strict";

const ENCODED_IMAGE_DATA = "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCIgZW5hYmxlLWJhY2tncm91bmQ9Im5ldyAwIDAgNjQgNjQiPjxwYXRoIGQ9Im01NS45IDMyLjFsLTIyLjctMTQuOWMwIDAgMTIuOS0xNy40IDE5LjQtMTQuOSAzLjEgMS4xIDUuNCAyNS4xIDMuMyAyOS44IiBmaWxsPSIjM2U0MzQ3Ii8+PHBhdGggZD0ibTU0LjkgMzMuOWwtOS00LjFjMCAwLTUuMy0xNCA2LjEtMjQuMSAyLjQgMiA1LjEgMjUgMi45IDI4LjIiIGZpbGw9IiNmZmYiLz48cGF0aCBkPSJtOC4xIDMyLjFsMjIuNi0xNC45YzAgMC0xMi45LTE3LjQtMTkuNC0xNC45LTMgMS4xLTUuMyAyNS4xLTMuMiAyOS44IiBmaWxsPSIjM2U0MzQ3Ii8+PHBhdGggZD0ibTkuMSAzMy45bDktNC4xYzAgMCA1LjMtMTQtNi4xLTI0LjEtMi40IDItNS4xIDI1LTIuOSAyOC4yIiBmaWxsPSIjZmZmIi8+PHBhdGggZD0iTTMyLDEzQzE4LjksMTMsMiwzMy42LDIsNDUuNEMyMC41LDQ1LjQsMTkuNyw2MiwzMiw2MnMxMS41LTE2LjYsMzAtMTYuNkM2MiwzMy42LDQ1LjEsMTMsMzIsMTN6IiBmaWxsPSIjZmY4NzM2Ii8+PGcgZmlsbD0iI2ZmZiI+PHBhdGggZD0iTTMyLDU2LjJjMCw1LjEsOS42LDQuMiw5LjUtMi45YzYuNy05LjQsMTkuOS04LjcsMTkuOS04LjdDMzkuNiwzMi40LDMyLDU2LjIsMzIsNTYuMnoiLz48cGF0aCBkPSJNMzIsNTYuMmMwLDUuMS05LjYsNC4yLTkuNS0yLjlDMTUuOCw0NCwyLjYsNDQuNywyLjYsNDQuN0MyNC40LDMyLjQsMzIsNTYuMiwzMiw1Ni4yeiIvPjwvZz48ZyBmaWxsPSIjZmY4NzM2Ij48cGF0aCBkPSJtNTMuNCAxOC41Yy00IC43LTQuOSA2LjMtNC45IDYuM2w2IDUuM2MtMi4zLTUuOS0xLjEtMTEuNi0xLjEtMTEuNiIvPjxwYXRoIGQ9Im01MS4xIDEzLjVjLTQuNCAzLjktNS4xIDguNy01LjEgOC43bDYgNS4zYy0yLjQtNS44LS45LTE0LS45LTE0Ii8+PHBhdGggZD0ibTEwLjYgMTguNWM0IC43IDQuOSA2LjMgNC45IDYuM2wtNiA1LjNjMi4zLTUuOSAxLjEtMTEuNiAxLjEtMTEuNiIvPjxwYXRoIGQ9Im0xMi45IDEzLjVjNC40IDMuOSA1LjEgOC43IDUuMSA4LjdsLTYgNS4zYzIuNC01LjguOS0xNCAuOS0xNCIvPjwvZz48cGF0aCBkPSJtNTIuOCAzMS4xYy01LjctMS44LTEwLjktMy40LTEzLjguOS0yLjQgMy43LjcgOS40LjcgOS40IDExLjIgMS4yIDEzLjEtMTAuMyAxMy4xLTEwLjMiIGZpbGw9IiMzZTQzNDciLz48ZWxsaXBzZSBjeD0iNDMiIGN5PSIzNi4zIiByeD0iNC4yIiByeT0iNC4xIiBmaWxsPSIjZDVmZjgzIi8+PGcgZmlsbD0iIzNlNDM0NyI+PGVsbGlwc2UgY3g9IjQzIiBjeT0iMzYuMyIgcng9IjIuNyIgcnk9IjIuNyIvPjxwYXRoIGQ9Im0xMS4yIDMxLjFjNS43LTEuOCAxMC45LTMuNCAxMy43LjkgMi40IDMuNy0uNyA5LjQtLjcgOS40LTExLjEgMS4yLTEzLTEwLjMtMTMtMTAuMyIvPjwvZz48ZWxsaXBzZSBjeD0iMjEiIGN5PSIzNi4zIiByeD0iNC4yIiByeT0iNC4xIiBmaWxsPSIjZDVmZjgzIi8+PGcgZmlsbD0iIzNlNDM0NyI+PGVsbGlwc2UgY3g9IjIxIiBjeT0iMzYuMyIgcng9IjIuNyIgcnk9IjIuNyIvPjxwYXRoIGQ9Im00MS4yIDQ3LjljLS43LTIuMy0xLjgtNC40LTMtNi41IDEuMSAyLjEgMiA0LjMgMi41IDYuNi41IDIuMy43IDQuNyAwIDYuOC0uNCAxLTEgMi0xLjggMi42LS44LjYtMS44IDEtMi43IDEtLjkgMC0xLjktLjMtMi41LTEtLjYtLjctLjktMS42LS44LTIuNmwtLjkuMmgtLjljMCAxLS4yIDEuOS0uOCAyLjYtLjYuNy0xLjUgMS0yLjUgMS0uOSAwLTEuOS0uNC0yLjctMS0uOC0uNi0xLjQtMS42LTEuOC0yLjYtLjgtMi4xLS42LTQuNiAwLTYuOC41LTIuMyAxLjUtNC41IDIuNS02LjYtMS4yIDItMi4zIDQuMS0zIDYuNS0uNyAyLjMtMS4xIDQuOC0uNCA3LjMuMyAxLjIgMSAyLjQgMS45IDMuMy45LjkgMi4xIDEuNCAzLjQgMS41IDEuMi4xIDIuNi0uMiAzLjctMS4yLjMtLjIuNS0uNS43LS44LjIuMy40LjYuNy44IDEgMSAyLjQgMS4zIDMuNyAxLjIgMS4zLS4xIDIuNC0uNyAzLjQtMS41LjktLjkgMS42LTIgMS45LTMuMy41LTIuNi4xLTUuMi0uNi03LjUiLz48cGF0aCBkPSJtMzcuNiA1MC4zYy0xLjEtMS4xLTQuNS0xLjItNS42LTEuMi0xIDAtNC41LjEtNS42IDEuMi0uOC44LS4yIDIuOCAxLjkgNC41IDEuMyAxLjEgMi42IDEuNCAzLjYgMS40IDEgMCAyLjMtLjMgMy42LTEuNCAyLjMtMS43IDIuOS0zLjcgMi4xLTQuNSIvPjwvZz48L3N2Zz4=";

  /**
   * Verifies that the button uses the expected icon.
   *
   * @param {string} selector The CSS selector used to find the button
   *   within the DOM.
   * @param {boolean} shouldHaveCustomStyling True if the button should
   *   have custom styling, False otherwise.
   * @param {string} message The message that is printed to the console
   *   by the verifyFn.
   */
function verifyButtonProperties(selector, shouldHaveCustomStyling, message) {
  try {
    let element;
    // This selector is different than the others because it's the only
    // toolbarbutton that we ship by default that has type="menu-button",
    // which don't place a unique ID on the associated dropmarker-icon.
    if (selector == "#bookmarks-menu-button > .toolbarbutton-menubutton-dropmarker > .dropmarker-icon") {
      if (message.includes("panel")) {
        // The dropmarker isn't shown in the menupanel.
        return;
      }
      element = document.querySelector("#bookmarks-menu-button");
      element = document.getAnonymousElementByAttribute(element, "class", "toolbarbutton-menubutton-dropmarker");
      element = document.getAnonymousElementByAttribute(element, "class", "dropmarker-icon");
    } else {
      element = document.querySelector(selector);
    }

    let listStyleImage = getComputedStyle(element).listStyleImage;
    info(`listStyleImage for fox.svg is ${listStyleImage}`);
    is(listStyleImage.includes("fox.svg"), shouldHaveCustomStyling, message);
  } catch (ex) {
    ok(false, `Unable to verify ${selector}: ${ex}`);
  }
}

  /**
   * Verifies that the button uses default styling.
   *
   * @param {string} selector The CSS selector used to find the button
   *   within the DOM.
   * @param {string} message The message that is printed to the console
   *   by the verifyFn.
   */
function verifyButtonWithoutCustomStyling(selector, message) {
  verifyButtonProperties(selector, false, message);
}

  /**
   * Verifies that the button uses non-default styling.
   *
   * @param {string} selector The CSS selector used to find the button
   *   within the DOM.
   * @param {string} message The message that is printed to the console
   *   by the verifyFn.
   */
function verifyButtonWithCustomStyling(selector, message) {
  verifyButtonProperties(selector, true, message);
}

  /**
   * Loops through all of the buttons to confirm that they are styled
   * as expected (either with or without custom styling).
   *
   * @param {object} icons Array of an array that specifies which buttons should
   *   have custom icons.
   * @param {object} iconInfo An array of arrays that maps API names to
   *   CSS selectors.
   * @param {string} area The name of the area that the button resides in.
   */
function checkButtons(icons, iconInfo, area) {
  for (let button of iconInfo) {
    let iconInfo = icons.find(arr => arr[0] == button[0]);
    if (iconInfo[1]) {
      verifyButtonWithCustomStyling(button[1],
        `The ${button[1]} should have it's icon customized in the ${area}`);
    } else {
      verifyButtonWithoutCustomStyling(button[1],
        `The ${button[1]} should not have it's icon customized in the ${area}`);
    }
  }
}

async function runTestWithIcons(icons) {
  const FRAME_COLOR = [71, 105, 91];
  const TAB_TEXT_COLOR = [207, 221, 192, .9];
  let manifest = {
    "theme": {
      "images": {
        "theme_frame": "fox.svg",
      },
      "colors": {
        "frame": FRAME_COLOR,
        "tab_text": TAB_TEXT_COLOR,
      },
      "icons": {},
    },
  };
  let files = {
    "fox.svg": imageBufferFromDataURI(ENCODED_IMAGE_DATA),
  };

  // Each item in this array has the following setup:
  // At position 0: The name that is used in the theme manifest.
  // At position 1: The CSS selector for the button in the DOM.
  // At position 2: The CustomizableUI name for the widget, only defined
  //                if customizable.
  const ICON_INFO = [
    ["back", "#back-button"],
    ["forward", "#forward-button"],
    ["reload", "#reload-button"],
    ["stop", "#stop-button"],
    ["bookmark_star", "#bookmarks-menu-button", "bookmarks-menu-button"],
    ["bookmark_menu", "#bookmarks-menu-button > .toolbarbutton-menubutton-dropmarker > .dropmarker-icon"],
    ["downloads", "#downloads-button", "downloads-button"],
    ["home", "#home-button", "home-button"],
    ["app_menu", "#PanelUI-menu-button"],
    ["cut", "#cut-button", "edit-controls"],
    ["copy", "#copy-button"],
    ["paste", "#paste-button"],
    ["new_window", "#new-window-button", "new-window-button"],
    ["new_private_window", "#privatebrowsing-button", "privatebrowsing-button"],
    ["save_page", "#save-page-button", "save-page-button"],
    ["print", "#print-button", "print-button"],
    ["history", "#history-panelmenu", "history-panelmenu"],
    ["full_screen", "#fullscreen-button", "fullscreen-button"],
    ["find", "#find-button", "find-button"],
    ["options", "#preferences-button", "preferences-button"],
    ["addons", "#add-ons-button", "add-ons-button"],
    ["developer", "#developer-button", "developer-button"],
    ["synced_tabs", "#sync-button", "sync-button"],
    ["open_file", "#open-file-button", "open-file-button"],
    ["sidebars", "#sidebar-button", "sidebar-button"],
    ["share_page", "#social-share-button", "social-share-button"],
    ["subscribe", "#feed-button", "feed-button"],
    ["text_encoding", "#characterencoding-button", "characterencoding-button"],
    ["email_link", "#email-link-button", "email-link-button"],
    ["forget", "#panic-button", "panic-button"],
    ["pocket", "#pocket-button", "pocket-button"],
  ];

  window.maximize();

  for (let button of ICON_INFO) {
    if (button[2]) {
      CustomizableUI.addWidgetToArea(button[2], CustomizableUI.AREA_NAVBAR);
    }

    verifyButtonWithoutCustomStyling(button[1],
      `The ${button[1]} should not have it's icon customized when the test starts`);

    let iconInfo = icons.find(arr => arr[0] == button[0]);
    manifest.theme.icons[button[0]] = iconInfo[1];
  }

  let extension = ExtensionTestUtils.loadExtension({manifest, files});

  await extension.startup();

  checkButtons(icons, ICON_INFO, "toolbar");

  if (!gPhotonStructure) {
    for (let button of ICON_INFO) {
      if (button[2]) {
        CustomizableUI.addWidgetToArea(button[2], CustomizableUI.AREA_PANEL);
      }
    }

    await PanelUI.show();

    checkButtons(icons, ICON_INFO, "panel");

    await PanelUI.hide();
  }

  await extension.unload();

  for (let button of ICON_INFO) {
    verifyButtonWithoutCustomStyling(button[1],
      `The ${button[1]} should not have it's icon customized when the theme is unloaded`);
  }
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true],
          ["extensions.webextensions.themes.icons.enabled", true]],
  });
});

add_task(async function test_all_icons() {
  let icons = [
    ["back", "fox.svg"],
    ["forward", "fox.svg"],
    ["reload", "fox.svg"],
    ["stop", "fox.svg"],
    ["bookmark_star", "fox.svg"],
    ["bookmark_menu", "fox.svg"],
    ["downloads", "fox.svg"],
    ["home", "fox.svg"],
    ["app_menu", "fox.svg"],
    ["cut", "fox.svg"],
    ["copy", "fox.svg"],
    ["paste", "fox.svg"],
    ["new_window", "fox.svg"],
    ["new_private_window", "fox.svg"],
    ["save_page", "fox.svg"],
    ["print", "fox.svg"],
    ["history", "fox.svg"],
    ["full_screen", "fox.svg"],
    ["find", "fox.svg"],
    ["options", "fox.svg"],
    ["addons", "fox.svg"],
    ["developer", "fox.svg"],
    ["synced_tabs", "fox.svg"],
    ["open_file", "fox.svg"],
    ["sidebars", "fox.svg"],
    ["share_page", "fox.svg"],
    ["subscribe", "fox.svg"],
    ["text_encoding", "fox.svg"],
    ["email_link", "fox.svg"],
    ["forget", "fox.svg"],
    ["pocket", "fox.svg"],
  ];
  await runTestWithIcons(icons);
});

add_task(async function teardown() {
  CustomizableUI.reset();
  window.restore();
});

add_task(async function test_some_icons() {
  let icons = [
    ["back", ""],
    ["forward", ""],
    ["reload", "fox.svg"],
    ["stop", ""],
    ["bookmark_star", ""],
    ["bookmark_menu", ""],
    ["downloads", ""],
    ["home", "fox.svg"],
    ["app_menu", "fox.svg"],
    ["cut", ""],
    ["copy", ""],
    ["paste", ""],
    ["new_window", ""],
    ["new_private_window", ""],
    ["save_page", ""],
    ["print", ""],
    ["history", ""],
    ["full_screen", ""],
    ["find", ""],
    ["options", ""],
    ["addons", ""],
    ["developer", ""],
    ["synced_tabs", ""],
    ["open_file", ""],
    ["sidebars", ""],
    ["share_page", ""],
    ["subscribe", ""],
    ["text_encoding", ""],
    ["email_link", ""],
    ["forget", ""],
    ["pocket", "fox.svg"],
  ];
  await runTestWithIcons(icons);
});

add_task(async function teardown() {
  CustomizableUI.reset();
  window.restore();
});
