/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_disable_shield() {
  const { RecipeRunner } = ChromeUtils.importESModule(
    "resource://normandy/lib/RecipeRunner.sys.mjs"
  );
  const { BaseAction } = ChromeUtils.importESModule(
    "resource://normandy/actions/BaseAction.sys.mjs"
  );
  const { BaseStudyAction } = ChromeUtils.importESModule(
    "resource://normandy/actions/BaseStudyAction.sys.mjs"
  );

  const baseAction = new BaseAction();
  const baseStudyAction = new BaseStudyAction();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["app.normandy.api_url", "https://localhost/selfsupport-dummy/"],
      ["app.shield.optoutstudies.enabled", true],
    ],
  });

  ok(RecipeRunner, "RecipeRunner exists");

  RecipeRunner.checkPrefs();
  ok(RecipeRunner.enabled, "RecipeRunner is enabled");

  baseAction._preExecution();
  is(
    baseAction.state,
    BaseAction.STATE_PREPARING,
    "Base action is not disabled"
  );

  baseStudyAction._preExecution();
  is(
    baseStudyAction.state,
    BaseAction.STATE_PREPARING,
    "Base study action is not disabled"
  );

  await setupPolicyEngineWithJson({
    policies: {
      DisableFirefoxStudies: true,
    },
  });

  RecipeRunner.checkPrefs();
  ok(RecipeRunner.enabled, "RecipeRunner is still enabled");

  baseAction._preExecution();
  is(
    baseAction.state,
    BaseAction.STATE_PREPARING,
    "Base action is not disabled"
  );

  baseStudyAction._preExecution();
  is(
    baseStudyAction.state,
    BaseAction.STATE_DISABLED,
    "Base study action is disabled"
  );
});
