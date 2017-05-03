"use strict";

Cu.import("resource://shield-recipe-client/lib/ShieldRecipeClient.jsm", this);
Cu.import("resource://shield-recipe-client/lib/RecipeRunner.jsm", this);
Cu.import("resource://shield-recipe-client/lib/PreferenceExperiments.jsm", this);

add_task(async function testStartup() {
  sinon.stub(RecipeRunner, "init");
  sinon.stub(PreferenceExperiments, "init");

  await ShieldRecipeClient.startup();
  ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
  ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");

  PreferenceExperiments.init.restore();
  RecipeRunner.init.restore();
});

add_task(async function testStartupPrefInitFail() {
  sinon.stub(RecipeRunner, "init");
  sinon.stub(PreferenceExperiments, "init").returns(Promise.reject(new Error("oh no")));

  await ShieldRecipeClient.startup();
  ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
  // Even if PreferenceExperiments.init fails, RecipeRunner.init should be called.
  ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");

  PreferenceExperiments.init.restore();
  RecipeRunner.init.restore();
});
