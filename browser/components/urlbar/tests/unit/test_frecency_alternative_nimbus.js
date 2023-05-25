/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ORIGINS_FEATUREGATE = "places.frecency.origins.alternative.featureGate";
const ORIGINS_DAYSCUTOFF = "places.frecency.origins.alternative.daysCutOff";

add_task(async function () {
  let reset = await UrlbarTestUtils.initNimbusFeature(
    {
      // Empty for sanity check.
    },
    "frecency",
    "config"
  );
  Assert.equal(Services.prefs.getBoolPref(ORIGINS_FEATUREGATE), false);
  Assert.throws(
    () => Services.prefs.getIntPref(ORIGINS_DAYSCUTOFF),
    /NS_ERROR_UNEXPECTED/
  );
  await reset();

  reset = await UrlbarTestUtils.initNimbusFeature(
    {
      originsAlternativeEnable: true,
    },
    "frecency",
    "config"
  );
  Assert.equal(Services.prefs.getBoolPref(ORIGINS_FEATUREGATE), true);
  Assert.ok(Services.prefs.prefHasUserValue(ORIGINS_FEATUREGATE));
  Assert.throws(
    () => Services.prefs.getIntPref(ORIGINS_DAYSCUTOFF),
    /NS_ERROR_UNEXPECTED/
  );
  await reset();

  reset = await UrlbarTestUtils.initNimbusFeature(
    {
      originsAlternativeEnable: true,
      originsDaysCutOff: 60,
    },
    "frecency",
    "config"
  );
  Assert.equal(Services.prefs.getBoolPref(ORIGINS_FEATUREGATE), true);
  Assert.ok(Services.prefs.prefHasUserValue(ORIGINS_FEATUREGATE));
  Assert.equal(Services.prefs.getIntPref(ORIGINS_DAYSCUTOFF, 90), 60);
  Assert.ok(Services.prefs.prefHasUserValue(ORIGINS_DAYSCUTOFF));
  await reset();
});
