/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function test() {
  Assert.throws(
    () => UrlbarPrefs.get("browser.migration.version"),
    /Trying to access an unknown pref/,
    "Should throw when passing an untracked pref"
  );

  Assert.throws(
    () => UrlbarPrefs.set("browser.migration.version", 100),
    /Trying to access an unknown pref/,
    "Should throw when passing an untracked pref"
  );
  Assert.throws(
    () => UrlbarPrefs.set("maxRichResults", "10"),
    /Invalid value/,
    "Should throw when passing an invalid value type"
  );

  Assert.deepEqual(UrlbarPrefs.get("formatting.enabled"), true);
  UrlbarPrefs.set("formatting.enabled", false);
  Assert.deepEqual(UrlbarPrefs.get("formatting.enabled"), false);

  Assert.deepEqual(UrlbarPrefs.get("maxRichResults"), 10);
  UrlbarPrefs.set("maxRichResults", 6);
  Assert.deepEqual(UrlbarPrefs.get("maxRichResults"), 6);

  Assert.deepEqual(UrlbarPrefs.get("autoFill.stddevMultiplier"), 0.0);
  UrlbarPrefs.set("autoFill.stddevMultiplier", 0.01);
  // Due to rounding errors, floats are slightly imprecise, so we can't
  // directly compare what we set to what we retrieve.
  Assert.deepEqual(
    parseFloat(UrlbarPrefs.get("autoFill.stddevMultiplier").toFixed(2)),
    0.01
  );
});

add_task(function experiment_update2() {
  UrlbarPrefs.set("experiment.update2", false);
  UrlbarPrefs.set("update2", false);
  Assert.equal(UrlbarPrefs.get("update2"), false);

  UrlbarPrefs.set("update2", true);
  Assert.deepEqual(
    UrlbarPrefs.get("update2"),
    false,
    "If experiment.update2 is false, it overrides update2."
  );

  UrlbarPrefs.set("experiment.update2", true);
  UrlbarPrefs.set("update2", false);
  Assert.deepEqual(
    UrlbarPrefs.get("update2"),
    false,
    "If experiment.update2 is true, it does not override update2."
  );

  UrlbarPrefs.set("update2", true);
  Assert.deepEqual(UrlbarPrefs.get("update2"), true);
});
