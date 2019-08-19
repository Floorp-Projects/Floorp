/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_DEVTOOLS_THEME = "devtools.theme";

registerCleanupFunction(() => {
  // Set preferences back to their original values
  Services.prefs.clearUserPref(PREF_DEVTOOLS_THEME);
});

add_task(async function testDevtoolsTheme() {
  info("Checking stylesheet and :root attributes based on devtools theme.");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");
  is(
    document.getElementById("appcontent").getAttribute("devtoolstheme"),
    "light",
    "The element has an attribute based on devtools theme."
  );

  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");
  is(
    document.getElementById("appcontent").getAttribute("devtoolstheme"),
    "dark",
    "The element has an attribute based on devtools theme."
  );

  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "unknown");
  is(
    document.getElementById("appcontent").getAttribute("devtoolstheme"),
    "light",
    "The element has 'light' as a default for the devtoolstheme attribute."
  );
});
