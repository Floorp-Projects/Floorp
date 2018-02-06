/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_disable_shield() {
  const { RecipeRunner } = ChromeUtils.import("resource://shield-recipe-client/lib/RecipeRunner.jsm", {});

  await SpecialPowers.pushPrefEnv({ set: [["extensions.shield-recipe-client.api_url",
                                            "https://localhost/selfsupport-dummy/"]] });

  ok(RecipeRunner, "RecipeRunner exists");
  RecipeRunner.checkPrefs();
  is(RecipeRunner.enabled, true, "RecipeRunner is enabled");

  await setupPolicyEngineWithJson({
    "policies": {
      "DisableFirefoxStudies": true
    }
  });

  RecipeRunner.checkPrefs();
  is(RecipeRunner.enabled, false, "RecipeRunner is disabled");
});
