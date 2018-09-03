/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const SUPPORT_FILES_PATH = "http://mochi.test:8888/browser/browser/components/enterprisepolicies/tests/browser";
const BLOCKED_PAGE   = `${SUPPORT_FILES_PATH}/policy_websitefilter_block.html`;
const EXCEPTION_PAGE = `${SUPPORT_FILES_PATH}/policy_websitefilter_exception.html`;

add_task(async function test() {
  await setupPolicyEngineWithJson({
    "policies": {
      "WebsiteFilter": {
        "Block": [
          "*://mochi.test/*policy_websitefilter_*",
        ],
        "Exceptions": [
          "*://mochi.test/*_websitefilter_exception*",
        ],
      },
    },
  });

  await checkBlockedPage(BLOCKED_PAGE, true);
  await checkBlockedPage(EXCEPTION_PAGE, false);
});
