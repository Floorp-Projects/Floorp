/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  const tests = [
    {
      enableVariable: "originsAlternativeEnable",
      enablePref: "places.frecency.origins.alternative.featureGate",
      variables: {
        originsDaysCutOff: "places.frecency.origins.alternative.daysCutOff",
      },
    },
    {
      enableVariable: "pagesAlternativeEnable",
      enablePref: "places.frecency.pages.alternative.featureGate",
      variables: {
        pagesNumSampledVisits:
          "places.frecency.pages.alternative.numSampledVisits",
        pagesHalfLifeDays: "places.frecency.pages.alternative.halfLifeDays",
        pagesHighWeight: "places.frecency.pages.alternative.highWeight",
        pagesMediumWeight: "places.frecency.pages.alternative.mediumWeight",
        pagesLowWeight: "places.frecency.pages.alternative.lowWeight",
      },
    },
  ];
  for (let test of tests) {
    await doTest(test.enableVariable, test.enablePref, test.variables);
  }
});

async function doTest(enableVariable, enablePref, otherVariables) {
  info(`Testing ${enableVariable}`);
  let reset = await UrlbarTestUtils.initNimbusFeature(
    {
      // Empty for sanity check.
    },
    "urlbar",
    "config"
  );
  Assert.ok(!Services.prefs.prefHasUserValue(enablePref));
  Assert.ok(!Services.prefs.getBoolPref(enablePref, false));
  for (let pref of Object.values(otherVariables)) {
    Assert.ok(!Services.prefs.prefHasUserValue(pref));
  }
  await reset();

  reset = await UrlbarTestUtils.initNimbusFeature(
    {
      [enableVariable]: true,
    },
    "urlbar",
    "config"
  );
  Assert.ok(Services.prefs.prefHasUserValue(enablePref));
  Assert.equal(Services.prefs.getBoolPref(enablePref), true);
  for (let pref of Object.values(otherVariables)) {
    Assert.ok(!Services.prefs.prefHasUserValue(pref));
  }
  await reset();

  const FAKE_VALUE = 777;
  let config = {
    [enableVariable]: true,
  };
  for (let variable of Object.keys(otherVariables)) {
    config[variable] = FAKE_VALUE;
  }
  reset = await UrlbarTestUtils.initNimbusFeature(config, "urlbar", "config");
  Assert.ok(Services.prefs.prefHasUserValue(enablePref));
  Assert.equal(Services.prefs.getBoolPref(enablePref), true);
  for (let pref of Object.values(otherVariables)) {
    Assert.ok(Services.prefs.prefHasUserValue(pref));
    Assert.equal(Services.prefs.getIntPref(pref, 90), FAKE_VALUE);
  }

  await reset();
}
