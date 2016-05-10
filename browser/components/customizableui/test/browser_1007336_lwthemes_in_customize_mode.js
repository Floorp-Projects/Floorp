/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEFAULT_THEME_ID = "{972ce4c6-7e08-4474-a285-3208198ce6fd}";
const {LightweightThemeManager} = Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", {});

add_task(function* () {
  Services.prefs.clearUserPref("lightweightThemes.usedThemes");
  Services.prefs.clearUserPref("lightweightThemes.recommendedThemes");
  LightweightThemeManager.clearBuiltInThemes();

  yield startCustomizing();

  let themesButton = document.getElementById("customization-lwtheme-button");
  let popup = document.getElementById("customization-lwtheme-menu");

  let popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button");
  yield popupShownPromise;

  // close current tab and re-open Customize menu to confirm correct number of Themes
  yield endCustomizing();
  info("Exited customize mode");
  yield startCustomizing();
  info("Started customizing a second time");
  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button a second time");
  yield popupShownPromise;

  let header = document.getElementById("customization-lwtheme-menu-header");
  let recommendedHeader = document.getElementById("customization-lwtheme-menu-recommended");

  is(header.nextSibling.nextSibling, recommendedHeader,
     "There should only be one theme (default) in the 'My Themes' section by default");
  is(header.nextSibling.theme.id, DEFAULT_THEME_ID, "That theme should be the default theme");

  let firstLWTheme = recommendedHeader.nextSibling;
  let firstLWThemeId = firstLWTheme.theme.id;
  let themeChangedPromise = promiseObserverNotified("lightweight-theme-changed");
  firstLWTheme.doCommand();
  info("Clicked on first theme");
  yield themeChangedPromise;

  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(themesButton, {});
  info("Clicked on themes button");
  yield popupShownPromise;

  is(header.nextSibling.theme.id, DEFAULT_THEME_ID, "The first theme should be the Default theme");
  let installedThemeId = header.nextSibling.nextSibling.theme.id;
  ok(installedThemeId.startsWith(firstLWThemeId),
     "The second theme in the 'My Themes' section should be the newly installed theme: " +
     "Installed theme id: " + installedThemeId + "; First theme ID: " + firstLWThemeId);
  is(header.nextSibling.nextSibling.nextSibling, recommendedHeader,
     "There should be two themes in the 'My Themes' section");

  let defaultTheme = header.nextSibling;
  defaultTheme.doCommand();
  is(Services.prefs.getCharPref("lightweightThemes.selectedThemeID"), "", "No lwtheme should be selected");
});

add_task(function* asyncCleanup() {
  yield endCustomizing();

  Services.prefs.clearUserPref("lightweightThemes.usedThemes");
  Services.prefs.clearUserPref("lightweightThemes.recommendedThemes");
});
