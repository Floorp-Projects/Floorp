/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function setup() {
  await setupPolicyEngineWithJson({
    policies: {
      SupportMenu: {
        Title: "Title",
        URL: "https://example.com/",
        AccessKey: "T",
      },
    },
  });
});

add_task(async function test_help_menu() {
  is(
    Services.policies.getSupportMenu().URL.href,
    "https://example.com/",
    "The policy should have the correct URL."
  );
  buildHelpMenu();
  let supportMenu = document.getElementById("helpPolicySupport");
  is(supportMenu.hidden, false, "The policy menu should be visible.");
  is(
    supportMenu.getAttribute("label"),
    "Title",
    "The policy menu should have the correct title."
  );
  is(
    supportMenu.getAttribute("accesskey"),
    "T",
    "The policy menu should have the correct access key."
  );
});
