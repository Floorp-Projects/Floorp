import {ASRouterTargeting} from "lib/ASRouterTargeting.jsm";

// Note that tests for the ASRouterTargeting environment can be found in
// test/functional/mochitest/browser_asrouter_targeting.js

describe("ASRouterTargeting#isInExperimentCohort", () => {
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.sandbox.create();
  });
  afterEach(() => sandbox.restore());
  it("should return the correct if the onboardingCohort pref value", () => {
    sandbox.stub(global.Services.prefs, "getStringPref").returns(JSON.stringify([{id: "onboarding", cohort: 1}]));
    const result = ASRouterTargeting.Environment.isInExperimentCohort;
    assert.equal(result, 1);
  });
  it("should return 0 if it cannot find the pref", () => {
    sandbox.stub(global.Services.prefs, "getStringPref").returns("");
    const result = ASRouterTargeting.Environment.isInExperimentCohort;
    assert.equal(result, 0);
  });
  it("should return 0 if it fails to parse the pref", () => {
    sandbox.stub(global.Services.prefs, "getStringPref").returns(17);
    const result = ASRouterTargeting.Environment.isInExperimentCohort;
    assert.equal(result, 0);
  });
});
