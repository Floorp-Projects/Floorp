/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEFAULT_THEME_ID = "default-theme@mozilla.org";
const LIGHT_THEME_ID = "firefox-compact-light@mozilla.org";
const DARK_THEME_ID = "firefox-compact-dark@mozilla.org";
const {LightweightThemeManager} = ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm", {});

add_task(async function() {
  Services.prefs.clearUserPref("lightweightThemes.usedThemes");
  Services.prefs.clearUserPref("lightweightThemes.recommendedThemes");

  await startCustomizing();

  let themesButton = document.getElementById("customization-lwtheme-button");
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
  let recommendedHeader = document.getElementById("customization-lwtheme-menu-recommended");

  is(header.nextSibling.nextSibling.nextSibling.nextSibling, recommendedHeader,
     "There should only be three themes (default, light, dark) in the 'My Themes' section by default");
  is(header.nextSibling.theme.id, DEFAULT_THEME_ID,
     "The first theme should be the default theme");
  is(header.nextSibling.nextSibling.theme.id, LIGHT_THEME_ID,
     "The second theme should be the light theme");
  is(header.nextSibling.nextSibling.nextSibling.theme.id, DARK_THEME_ID,
     "The third theme should be the dark theme");

  let themeChangedPromise = promiseObserverNotified("lightweight-theme-changed");
  header.nextSibling.nextSibling.doCommand(); // Select light theme
  info("Clicked on light theme");
  await themeChangedPromise;

  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button a third time");
  await popupShownPromise;

  let activeThemes = popup.querySelectorAll("toolbarbutton.customization-lwtheme-menu-theme[active]");
  is(activeThemes.length, 1, "Exactly 1 theme should be selected");
  if (activeThemes.length > 0) {
    is(activeThemes[0].theme.id, LIGHT_THEME_ID, "Light theme should be selected");
  }

  let firstLWTheme = recommendedHeader.nextSibling;
  let firstLWThemeId = firstLWTheme.theme.id;
  themeChangedPromise = promiseObserverNotified("lightweight-theme-changed");
  firstLWTheme.doCommand();
  info("Clicked on first theme");
  await themeChangedPromise;

  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button");
  await popupShownPromise;

  activeThemes = popup.querySelectorAll("toolbarbutton.customization-lwtheme-menu-theme[active]");
  is(activeThemes.length, 1, "Exactly 1 theme should be selected");
  if (activeThemes.length > 0) {
    is(activeThemes[0].theme.id, firstLWThemeId, "First theme should be selected");
  }

  is(header.nextSibling.theme.id, DEFAULT_THEME_ID, "The first theme should be the Default theme");
  let installedThemeId = header.nextSibling.nextSibling.nextSibling.nextSibling.theme.id;
  ok(installedThemeId.startsWith(firstLWThemeId),
     "The second theme in the 'My Themes' section should be the newly installed theme: " +
     "Installed theme id: " + installedThemeId + "; First theme ID: " + firstLWThemeId);
  let themeCount = 0;
  let iterNode = header;
  while (iterNode.nextSibling && iterNode.nextSibling.theme) {
    themeCount++;
    iterNode = iterNode.nextSibling;
  }
  is(themeCount, 4,
     "There should be four themes in the 'My Themes' section");

  let defaultTheme = header.nextSibling;
  defaultTheme.doCommand();
  await new Promise(SimpleTest.executeSoon);
  is(Services.prefs.getCharPref("lightweightThemes.selectedThemeID"),
     DEFAULT_THEME_ID, "Default theme should be selected");

  // ensure current theme isn't set to "Default"
  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button a fourth time");
  await popupShownPromise;

  firstLWTheme = recommendedHeader.nextSibling;
  themeChangedPromise = promiseObserverNotified("lightweight-theme-changed");
  firstLWTheme.doCommand();
  info("Clicked on first theme again");
  await themeChangedPromise;

  // check that "Restore Defaults" button resets theme
  await gCustomizeMode.reset();
  is(LightweightThemeManager.currentTheme.id, DEFAULT_THEME_ID, "Current theme reset to default");

  await endCustomizing();
  Services.prefs.setCharPref("lightweightThemes.usedThemes", "[]");
  Services.prefs.setCharPref("lightweightThemes.recommendedThemes", "[]");
  info("Removed all recommended themes");
  await startCustomizing();
  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button a fifth time");
  await popupShownPromise;
  header = document.getElementById("customization-lwtheme-menu-header");
  is(header.hidden, false, "Header should never be hidden");
  let themeNode = header.nextSibling;
  is(themeNode.theme.id, DEFAULT_THEME_ID, "The first theme should be the Default theme");
  is(themeNode.hidden, false, "The default theme should never be hidden");

  themeNode = themeNode.nextSibling;
  is(themeNode.theme.id, LIGHT_THEME_ID, "The second theme should be the Light theme");
  is(themeNode.hidden, false, "The light theme should never be hidden");

  themeNode = themeNode.nextSibling;
  is(themeNode.theme.id, DARK_THEME_ID, "The third theme should be the Dark theme");
  is(themeNode.hidden, false, "The dark theme should never be hidden");

  recommendedHeader = document.getElementById("customization-lwtheme-menu-recommended");
  is(themeNode.nextSibling, recommendedHeader,
     "There should only be three themes (default, light, dark) in the 'My Themes' section now");
  let footer = document.getElementById("customization-lwtheme-menu-footer");
  is(recommendedHeader.nextSibling.id, footer.id, "There should be no recommended themes in the menu");
  is(recommendedHeader.hidden, true, "The recommendedHeader should be hidden since there are no recommended themes");
});

add_task(async function asyncCleanup() {
  await endCustomizing();

  Services.prefs.clearUserPref("lightweightThemes.usedThemes");
  Services.prefs.clearUserPref("lightweightThemes.recommendedThemes");
});
