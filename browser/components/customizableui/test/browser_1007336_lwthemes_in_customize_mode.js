/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEFAULT_THEME_ID = "default-theme@mozilla.org";
const LIGHT_THEME_ID = "firefox-compact-light@mozilla.org";
const DARK_THEME_ID = "firefox-compact-dark@mozilla.org";
const MAX_THEME_COUNT = 6; // Not exposed from CustomizeMode.jsm

async function installTheme(id) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id}},
      manifest_version: 2,
      name: "Theme " + id,
      description: "wow. such theme.",
      author: "Pixel Pusher",
      version: "1",
      theme: {},
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  return extension;
}

add_task(async function() {
  await startCustomizing();
  // Check restore defaults button is disabled.
  ok(document.getElementById("customization-reset-button").disabled,
     "Reset button should start out disabled");

  let themesButton = document.getElementById("customization-lwtheme-button");
  let themesButtonIcon = themesButton.icon;
  let iconURL = themesButtonIcon.style.backgroundImage;
  // If we've run other tests before, we might have set the image to the
  // default theme's icon explicitly, otherwise it might be empty, in which
  // case the icon is determined by CSS (which will be the default
  // theme's icon).
  if (iconURL) {
    ok((/default/i).test(themesButtonIcon.style.backgroundImage),
       `Button should show default theme thumbnail - was: "${iconURL}"`);
  } else {
    is(iconURL, "",
       `Button should show default theme thumbnail (empty string) - was: "${iconURL}"`);
  }
  let popup = document.getElementById("customization-lwtheme-menu");

  let popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button");
  await popupShownPromise;

  // close current tab and re-open Customize menu to confirm correct number of Themes
  await endCustomizing();
  info("Exited customize mode");
  await startCustomizing();
  info("Started customizing a second time");
  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button a second time");
  await popupShownPromise;

  let header = document.getElementById("customization-lwtheme-menu-header");
  let footer = document.getElementById("customization-lwtheme-menu-footer");

  is(header.nextElementSibling.nextElementSibling.nextElementSibling.nextElementSibling, footer,
     "There should only be three themes (default, light, dark) in the 'My Themes' section by default");
  is(header.nextElementSibling.theme.id, DEFAULT_THEME_ID,
     "The first theme should be the default theme");
  is(header.nextElementSibling.nextElementSibling.theme.id, LIGHT_THEME_ID,
     "The second theme should be the light theme");
  is(header.nextElementSibling.nextElementSibling.nextElementSibling.theme.id, DARK_THEME_ID,
     "The third theme should be the dark theme");

  let themeChangedPromise = promiseObserverNotified("lightweight-theme-styling-update");
  header.nextElementSibling.nextElementSibling.doCommand(); // Select light theme
  info("Clicked on light theme");
  await themeChangedPromise;

  let button = document.getElementById("customization-reset-button");
  await TestUtils.waitForCondition(() => !button.disabled);

  // Check restore defaults button is enabled.
  ok(!button.disabled,
     "Reset button should not be disabled anymore");
  ok((/light/i).test(themesButtonIcon.style.backgroundImage),
     `Button should show light theme thumbnail - was: "${themesButtonIcon.style.backgroundImage}"`);

  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button a third time");
  await popupShownPromise;

  let activeThemes = popup.querySelectorAll("toolbarbutton.customization-lwtheme-menu-theme[active]");
  is(activeThemes.length, 1, "Exactly 1 theme should be selected");
  if (activeThemes.length > 0) {
    is(activeThemes[0].theme.id, LIGHT_THEME_ID, "Light theme should be selected");
  }
  popup.hidePopup();

  // Install 5 themes:
  let addons = [];
  for (let n = 1; n <= 5; n++) {
    addons.push(await installTheme("my-theme-" + n + "@example.com"));
  }
  addons = await Promise.all(addons);

  ok(!themesButtonIcon.style.backgroundImage,
     `Button should show fallback theme thumbnail - was: "${themesButtonIcon.style.backgroundImage}"`);

  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button a fourth time");
  await popupShownPromise;

  activeThemes = popup.querySelectorAll("toolbarbutton.customization-lwtheme-menu-theme[active]");
  is(activeThemes.length, 1, "Exactly 1 theme should be selected");
  if (activeThemes.length > 0) {
    is(activeThemes[0].theme.id, "my-theme-5@example.com", "Last installed theme should be selected");
  }

  let firstLWTheme = footer.previousElementSibling;
  let firstLWThemeId = firstLWTheme.theme.id;
  themeChangedPromise = promiseObserverNotified("lightweight-theme-styling-update");
  firstLWTheme.doCommand();
  info("Clicked on first theme");
  await themeChangedPromise;

  await new Promise(executeSoon);

  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button");
  await popupShownPromise;

  activeThemes = popup.querySelectorAll("toolbarbutton.customization-lwtheme-menu-theme[active]");
  is(activeThemes.length, 1, "Exactly 1 theme should be selected");
  if (activeThemes.length > 0) {
    is(activeThemes[0].theme.id, firstLWThemeId, "First theme should be selected");
  }

  is(header.nextElementSibling.theme.id, DEFAULT_THEME_ID, "The first theme should be the Default theme");
  let themeCount = 0;
  let iterNode = header;
  while (iterNode.nextElementSibling && iterNode.nextElementSibling.theme) {
    themeCount++;
    iterNode = iterNode.nextElementSibling;
  }
  is(themeCount, MAX_THEME_COUNT,
     "There should be the max number of themes in the 'My Themes' section");

  let defaultTheme = header.nextElementSibling;
  defaultTheme.doCommand();
  await new Promise(SimpleTest.executeSoon);

  // ensure current theme isn't set to "Default"
  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button a sixth time");
  await popupShownPromise;

  // check that "Restore Defaults" button resets theme
  await gCustomizeMode.reset();

  defaultTheme = await AddonManager.getAddonByID(DEFAULT_THEME_ID);
  is(defaultTheme.isActive, true, "Current theme reset to default");

  await endCustomizing();
  await startCustomizing();
  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button a seventh time");
  await popupShownPromise;
  header = document.getElementById("customization-lwtheme-menu-header");
  is(header.hidden, false, "Header should never be hidden");
  let themeNode = header.nextElementSibling;
  is(themeNode.theme.id, DEFAULT_THEME_ID, "The first theme should be the Default theme");
  is(themeNode.hidden, false, "The default theme should never be hidden");

  themeNode = themeNode.nextElementSibling;
  is(themeNode.theme.id, LIGHT_THEME_ID, "The second theme should be the Light theme");
  is(themeNode.hidden, false, "The light theme should never be hidden");

  themeNode = themeNode.nextElementSibling;
  is(themeNode.theme.id, DARK_THEME_ID, "The third theme should be the Dark theme");
  is(themeNode.hidden, false, "The dark theme should never be hidden");

  await Promise.all(addons.map(a => a.unload()));
});

add_task(async function asyncCleanup() {
  await endCustomizing();
});
