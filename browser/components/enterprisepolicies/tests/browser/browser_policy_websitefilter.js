/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const SUPPORT_FILES_PATH =
  "http://mochi.test:8888/browser/browser/components/enterprisepolicies/tests/browser/";
const BLOCKED_PAGE = "policy_websitefilter_block.html";
const EXCEPTION_PAGE = "policy_websitefilter_exception.html";

add_task(async function test_http() {
  await setupPolicyEngineWithJson({
    policies: {
      WebsiteFilter: {
        Block: ["*://mochi.test/*policy_websitefilter_*"],
        Exceptions: ["*://mochi.test/*_websitefilter_exception*"],
      },
    },
  });

  await checkBlockedPage(SUPPORT_FILES_PATH + BLOCKED_PAGE, true);
  await checkBlockedPage(
    "view-source:" + SUPPORT_FILES_PATH + BLOCKED_PAGE,
    true
  );
  await checkBlockedPage(SUPPORT_FILES_PATH + EXCEPTION_PAGE, false);
});

add_task(async function test_http_mixed_case() {
  await setupPolicyEngineWithJson({
    policies: {
      WebsiteFilter: {
        Block: ["*://mochi.test/*policy_websitefilter_*"],
        Exceptions: ["*://mochi.test/*_websitefilter_exception*"],
      },
    },
  });

  await checkBlockedPage(SUPPORT_FILES_PATH + BLOCKED_PAGE.toUpperCase(), true);
  await checkBlockedPage(
    SUPPORT_FILES_PATH + EXCEPTION_PAGE.toUpperCase(),
    false
  );
});

add_task(async function test_file() {
  await setupPolicyEngineWithJson({
    policies: {
      WebsiteFilter: {
        Block: ["file:///*"],
      },
    },
  });

  await checkBlockedPage("file:///this_should_be_blocked", true);
});
