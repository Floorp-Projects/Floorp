/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that navigating to a about:debugging#invalid-hash should show up an
// error page.
// Every url navigating including #invalid-hash should be kept in history and
// navigate back as expected.
add_task(function* () {
  let { tab, document } = yield openAboutDebugging("invalid-hash");
  let element = document.querySelector(".header-name");
  is(element.textContent, "Page not found", "Show error page");

  info("Opening addons-panel panel");
  document.querySelector("[aria-controls='addons-panel']").click();
  yield waitUntilElement("#addons-panel", document);

  yield waitForInitialAddonList(document);
  element = document.querySelector(".header-name");
  is(element.textContent, "Add-ons", "Show Addons");

  info("Opening about:debugging#invalid-hash");
  window.openUILinkIn("about:debugging#invalid-hash", "current");
  yield waitUntilElement(".error-page", document);

  element = document.querySelector(".header-name");
  is(element.textContent, "Page not found", "Show error page");

  gBrowser.goBack();
  yield waitUntilElement("#addons-panel", document);
  yield waitForInitialAddonList(document);
  element = document.querySelector(".header-name");
  is(element.textContent, "Add-ons", "Show Addons");

  gBrowser.goBack();
  yield waitUntilElement(".error-page", document);
  element = document.querySelector(".header-name");
  is(element.textContent, "Page not found", "Show error page");

  yield closeAboutDebugging(tab);
});
