/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "valid_json.json";

add_task(function* () {
  info("Test JSON theme started.");

  let oldPref = SpecialPowers.getCharPref("devtools.theme");
  SpecialPowers.setCharPref("devtools.theme", "light");

  yield addJsonViewTab(TEST_JSON_URL);

  is(yield getTheme(), "theme-light", "The initial theme is light");

  SpecialPowers.setCharPref("devtools.theme", "dark");
  is(yield getTheme(), "theme-dark", "Theme changed to dark");

  SpecialPowers.setCharPref("devtools.theme", "firebug");
  is(yield getTheme(), "theme-firebug", "Theme changed to firebug");

  SpecialPowers.setCharPref("devtools.theme", "light");
  is(yield getTheme(), "theme-light", "Theme changed to light");

  SpecialPowers.setCharPref("devtools.theme", oldPref);
});

function getTheme() {
  return getElementAttr(":root", "class");
}
