/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyServiceGetters(this, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
});

async function showAboutWelcomeModal() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.aboutwelcome.showModal", true]],
  });

  BrowserHandler.firstRunProfile = true;

  const data = [
    {
      id: "TEST_SCREEN",
      content: {
        position: "split",
        logo: {},
        title: "test",
      },
    },
  ];

  return {
    data,
    async cleanup() {
      await SpecialPowers.popPrefEnv();
      BrowserHandler.firstRunProfile = false;
    },
  };
}

add_task(async function show_about_welcome_modal() {
  const { data } = await showAboutWelcomeModal();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.aboutwelcome.screens", JSON.stringify(data)]],
  });
  BROWSER_GLUE._maybeShowDefaultBrowserPrompt();
  const [win] = await TestUtils.topicObserved("subdialog-loaded");
  const modal = win.document.querySelector(".onboardingContainer");
  ok(!!modal, "About Welcome modal shown");
  win.close();
});
