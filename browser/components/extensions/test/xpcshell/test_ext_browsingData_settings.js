/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sanitizer",
                                  "resource:///modules/Sanitizer.jsm");

const PREF_DOMAIN = "privacy.cpd.";
const SETTINGS_LIST = ["cache", "cookies", "history", "formData", "downloads"].sort();

add_task(async function testSettingsProperties() {
  function background() {
    browser.test.onMessage.addListener(msg => {
      browser.browsingData.settings().then(settings => {
        browser.test.sendMessage("settings", settings);
      });
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  await extension.startup();

  extension.sendMessage("settings");
  let settings = await extension.awaitMessage("settings");

  // Verify that we get the keys back we expect.
  deepEqual(Object.keys(settings.dataToRemove).sort(),
            SETTINGS_LIST,
            "dataToRemove contains expected properties.");
  deepEqual(Object.keys(settings.dataRemovalPermitted).sort(),
            SETTINGS_LIST,
            "dataToRemove contains expected properties.");

  let dataTypeSet = settings.dataToRemove;
  for (let key of Object.keys(dataTypeSet)) {
    equal(Preferences.get(`${PREF_DOMAIN}${key.toLowerCase()}`),
          dataTypeSet[key],
          `${key} property of dataToRemove matches the expected pref.`);
  }

  dataTypeSet = settings.dataRemovalPermitted;
  for (let key of Object.keys(dataTypeSet)) {
    equal(true, dataTypeSet[key], `${key} property of dataRemovalPermitted is true.`);
  }

  // Explicitly set a pref to both true and false and then check.
  const SINGLE_OPTION = "cache";
  const SINGLE_PREF = "privacy.cpd.cache";

  registerCleanupFunction(() => {
    Preferences.reset(SINGLE_PREF);
  });

  Preferences.set(SINGLE_PREF, true);

  extension.sendMessage("settings");
  settings = await extension.awaitMessage("settings");
  equal(settings.dataToRemove[SINGLE_OPTION], true, "Preference that was set to true returns true.");

  Preferences.set(SINGLE_PREF, false);

  extension.sendMessage("settings");
  settings = await extension.awaitMessage("settings");
  equal(settings.dataToRemove[SINGLE_OPTION], false, "Preference that was set to false returns false.");

  await extension.unload();
});

add_task(async function testSettingsSince() {
  const TIMESPAN_PREF = "privacy.sanitize.timeSpan";
  const TEST_DATA = {
    TIMESPAN_5MIN: Date.now() - 5 * 60 * 1000,
    TIMESPAN_HOUR: Date.now() - 60 * 60 * 1000,
    TIMESPAN_2HOURS: Date.now() - 2 * 60 * 60 * 1000,
    TIMESPAN_EVERYTHING: 0,
  };

  function background() {
    browser.test.onMessage.addListener(msg => {
      browser.browsingData.settings().then(settings => {
        browser.test.sendMessage("settings", settings);
      });
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  await extension.startup();

  registerCleanupFunction(() => {
    Preferences.reset(TIMESPAN_PREF);
  });

  for (let timespan in TEST_DATA) {
    Preferences.set(TIMESPAN_PREF, Sanitizer[timespan]);

    extension.sendMessage("settings");
    let settings = await extension.awaitMessage("settings");

    // Because it is based on the current timestamp, we cannot know the exact
    // value to expect for since, so allow a 10s variance.
    ok(Math.abs(settings.options.since - TEST_DATA[timespan]) < 10000,
       "settings.options contains the expected since value.");
  }

  await extension.unload();
});
