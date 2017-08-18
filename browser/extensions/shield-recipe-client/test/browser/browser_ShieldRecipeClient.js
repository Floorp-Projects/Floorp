"use strict";

Cu.import("resource://shield-recipe-client/lib/ShieldRecipeClient.jsm", this);
Cu.import("resource://shield-recipe-client/lib/RecipeRunner.jsm", this);
Cu.import("resource://shield-recipe-client/lib/PreferenceExperiments.jsm", this);
Cu.import("resource://shield-recipe-client-content/AboutPages.jsm", this);
Cu.import("resource://shield-recipe-client/lib/AddonStudies.jsm", this);

function withStubInits(testFunction) {
  return decorate(
    withStub(AboutPages, "init"),
    withStub(AddonStudies, "init"),
    withStub(PreferenceExperiments, "init"),
    withStub(RecipeRunner, "init"),
    testFunction
  );
}

decorate_task(
  withStubInits,
  async function testStartup() {
    await ShieldRecipeClient.startup();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
  }
);

decorate_task(
  withStubInits,
  async function testStartupPrefInitFail() {
    PreferenceExperiments.init.returns(Promise.reject(new Error("oh no")));

    await ShieldRecipeClient.startup();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
  }
);

decorate_task(
  withStubInits,
  async function testStartupAboutPagesInitFail() {
    AboutPages.init.returns(Promise.reject(new Error("oh no")));

    await ShieldRecipeClient.startup();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
  }
);

decorate_task(
  withStubInits,
  async function testStartupAddonStudiesInitFail() {
    AddonStudies.init.returns(Promise.reject(new Error("oh no")));

    await ShieldRecipeClient.startup();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
  }
);
