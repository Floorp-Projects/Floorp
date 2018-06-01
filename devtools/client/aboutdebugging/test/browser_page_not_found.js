/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that navigating to a about:debugging#invalid-hash should show up an
// error page.
// Every url navigating including #invalid-hash should be kept in history and
// navigate back as expected.
add_task(async function() {
  const { tab, document } = await openAboutDebugging("invalid-hash");
  let element = document.querySelector(".header-name");
  is(element.textContent, "Page not found", "Show error page");

  info("Opening addons-panel panel");
  document.querySelector("[aria-controls='addons-panel']").click();
  await waitUntilElement("#addons-panel", document);

  await waitForInitialAddonList(document);
  element = document.querySelector(".header-name");
  is(element.textContent, "Add-ons", "Show Addons");

  info("Opening about:debugging#invalid-hash");
  window.openTrustedLinkIn("about:debugging#invalid-hash", "current");
  await waitUntilElement(".error-page", document);

  element = document.querySelector(".header-name");
  is(element.textContent, "Page not found", "Show error page");

  gBrowser.goBack();
  await waitUntilElement("#addons-panel", document);
  await waitForInitialAddonList(document);
  element = document.querySelector(".header-name");
  is(element.textContent, "Add-ons", "Show Addons");

  gBrowser.goBack();
  await waitUntilElement(".error-page", document);
  element = document.querySelector(".header-name");
  is(element.textContent, "Page not found", "Show error page");

  await closeAboutDebugging(tab);
});
