/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-env browser */

add_task(async function() {
  pushPref("devtools.enabled", false);

  const {tab, doc, win} = await openAboutDevTools();

  const installPage = doc.getElementById("install-page");
  const welcomePage = doc.getElementById("welcome-page");

  info("Check that about:devtools is in the correct state with devtools.enabled=false");
  ok(!installPage.hasAttribute("hidden"), "install screen is visible");
  ok(welcomePage.hasAttribute("hidden"), "welcome screen is hidden");

  info("Click on the install button to enable DevTools.");
  const installButton = doc.getElementById("install");
  EventUtils.synthesizeMouseAtCenter(installButton, {}, win);

  info("Wait until the UI updates");
  await waitUntil(() => installPage.hasAttribute("hidden") === true);
  ok(!welcomePage.hasAttribute("hidden"), "welcome screen is visible");
  ok(Services.prefs.getBoolPref("devtools.enabled"),
    "The preference devtools.enabled has been flipped to true.");

  // Flip the devtools.enabled preference back to false, otherwise the pushPref cleanup
  // times out.
  Services.prefs.setBoolPref("devtools.enabled", false);

  await removeTab(tab);
});
