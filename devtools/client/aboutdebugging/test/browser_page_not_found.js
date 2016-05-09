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

  yield openPanel(document, "addons-panel");
  yield waitForInitialAddonList(document);
  element = document.querySelector(".header-name");
  is(element.textContent, "Add-ons", "Show Addons");

  yield changeAboutDebuggingHash(document, "invalid-hash");
  element = document.querySelector(".header-name");
  is(element.textContent, "Page not found", "Show error page");

  gBrowser.goBack();
  yield waitForMutation(
    document.querySelector(".main-content"), {childList: true});
  yield waitForInitialAddonList(document);
  element = document.querySelector(".header-name");
  is(element.textContent, "Add-ons", "Show Addons");

  gBrowser.goBack();
  yield waitForMutation(
    document.querySelector(".main-content"), {childList: true});
  element = document.querySelector(".header-name");
  is(element.textContent, "Page not found", "Show error page");

  yield closeAboutDebugging(tab);
});
