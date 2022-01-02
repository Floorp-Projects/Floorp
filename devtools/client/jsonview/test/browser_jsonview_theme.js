/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "valid_json.json";

add_task(async function() {
  info("Test JSON theme started.");

  const oldPref = Services.prefs.getCharPref("devtools.theme");
  Services.prefs.setCharPref("devtools.theme", "light");

  await addJsonViewTab(TEST_JSON_URL);

  is(await getTheme(), "theme-light", "The initial theme is light");

  Services.prefs.setCharPref("devtools.theme", "dark");
  is(await getTheme(), "theme-dark", "Theme changed to dark");

  Services.prefs.setCharPref("devtools.theme", "light");
  is(await getTheme(), "theme-light", "Theme changed to light");

  Services.prefs.setCharPref("devtools.theme", oldPref);
});

function getTheme() {
  return getElementAttr(":root", "class");
}
