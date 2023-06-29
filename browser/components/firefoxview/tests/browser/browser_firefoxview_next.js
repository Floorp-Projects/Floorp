/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function about_firefoxview_next_pref() {
  // Verify pref enables new Firefox view
  Services.prefs.setBoolPref("browser.tabs.firefox-view-next", true);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");
  });
  // Verify pref enables new Firefox view even when old is disabled
  Services.prefs.setBoolPref("browser.tabs.firefox-view", false);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");
  });
  Services.prefs.setBoolPref("browser.tabs.firefox-view", true);
  // Verify pref disales new Firefox view
  Services.prefs.setBoolPref("browser.tabs.firefox-view-next", false);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview");
  });
});
