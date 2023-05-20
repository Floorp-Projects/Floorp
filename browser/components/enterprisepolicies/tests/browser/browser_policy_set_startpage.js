/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_setup(async function () {
  // browser.startup.page is set by unittest-required/user.js,
  // but we need the default value
  await SpecialPowers.pushPrefEnv({
    clear: [["browser.startup.page"]],
  });
});

add_task(async function homepage_test_startpage_homepage() {
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        URL: "http://example1.com/#test",
        StartPage: "homepage",
      },
    },
  });
  await check_homepage({
    expectedURL: "http://example1.com/#test",
    expectedPageVal: 1,
  });
});

add_task(async function homepage_test_startpage_homepage_locked() {
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        URL: "http://example1.com/#test",
        StartPage: "homepage-locked",
        Locked: true,
      },
    },
  });
  await check_homepage({
    expectedURL: "http://example1.com/#test",
    expectedPageVal: 1,
    locked: true,
  });
});

add_task(async function homepage_test_startpage_none() {
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        StartPage: "none",
      },
    },
  });
  await check_homepage({
    expectedURL: "chrome://browser/content/blanktab.html",
    expectedPageVal: 1,
  });
});

add_task(async function homepage_test_startpage_restore() {
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        StartPage: "previous-session",
      },
    },
  });
  await check_homepage({ expectedPageVal: 3 });
});
