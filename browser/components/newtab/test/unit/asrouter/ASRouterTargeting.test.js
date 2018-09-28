import {ASRouterTargeting, CachedTargetingGetter} from "lib/ASRouterTargeting.jsm";
import {ASRouterPreferences} from "lib/ASRouterPreferences.jsm";

// Note that tests for the ASRouterTargeting environment can be found in
// test/functional/mochitest/browser_asrouter_targeting.js

describe("ASRouterTargeting#isInExperimentCohort", () => {
  let sandbox;
  let time;
  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    time = sinon.useFakeTimers();
  });
  afterEach(() => {
    sandbox.restore();
    time.restore();
    ASRouterPreferences.uninit();
  });
  it("should return the correct if the onboardingCohort pref value", () => {
    sandbox.stub(global.Services.prefs, "getStringPref").returns(JSON.stringify([{id: "onboarding", cohort: 1}]));
  });
  it("should return the correct cohort if the onboarding cohort pref value is defined", () => {
    sandbox.stub(ASRouterPreferences, "providers").get(() => [{id: "onboarding", cohort: 1}]);
    const result = ASRouterTargeting.Environment.isInExperimentCohort;
    assert.equal(result, 1);
  });
  it("should return 0 if it cannot find the pref", () => {
    sandbox.stub(ASRouterPreferences, "providers").get(() => [{id: "foo"}]);
    const result = ASRouterTargeting.Environment.isInExperimentCohort;
    assert.equal(result, 0);
  });
  it("should combine customContext and TargetingGetters", async () => {
    sandbox.stub(global.FilterExpressions, "eval");

    await ASRouterTargeting.isMatch("true == true", {foo: true});

    assert.calledOnce(global.FilterExpressions.eval);
    const [, context] = global.FilterExpressions.eval.firstCall.args;
    // Assert direct properties instead of inherited
    assert.equal(context.foo, true);
    assert.match(context.currentDate, sinon.match.date);
  });
});

describe("#CachedTargetingGetter", () => {
  const sixHours = 6 * 60 * 60 * 1000;
  let sandbox;
  let clock;
  let frecentStub;
  let topsitesCache;
  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    clock = sinon.useFakeTimers();
    frecentStub = sandbox.stub(global.NewTabUtils.activityStreamProvider, "getTopFrecentSites");
    sandbox.stub(global.Cu, "reportError");
    topsitesCache = new CachedTargetingGetter("getTopFrecentSites");
  });

  afterEach(() => {
    sandbox.restore();
    clock.restore();
  });

  it("should only make a request every 6 hours", async () => {
    frecentStub.resolves();
    clock.tick(sixHours);

    await topsitesCache.get();
    await topsitesCache.get();

    assert.calledOnce(global.NewTabUtils.activityStreamProvider.getTopFrecentSites);

    clock.tick(sixHours);

    await topsitesCache.get();

    assert.calledTwice(global.NewTabUtils.activityStreamProvider.getTopFrecentSites);
  });
  it("should report errors", async () => {
    frecentStub.rejects(new Error("fake error"));
    clock.tick(sixHours);

    // assert.throws expect a function as the first parameter, try/catch is a
    // workaround
    try {
      await topsitesCache.get();
      assert.isTrue(false);
    } catch (e) {
      assert.calledOnce(global.Cu.reportError);
    }
  });
});
