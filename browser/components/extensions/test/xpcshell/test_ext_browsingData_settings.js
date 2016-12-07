/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Sanitizer",
                                  "resource:///modules/Sanitizer.jsm");

const PREF_DOMAIN = "privacy.cpd.";

add_task(function* testSettings() {
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

  yield extension.startup();

  let branch = Services.prefs.getBranch(PREF_DOMAIN);

  extension.sendMessage("settings");
  let settings = yield extension.awaitMessage("settings");

  let since = Sanitizer.getClearRange()[0] / 1000;

  // Because it is based on the current timestamp, we cannot know the exact
  // value to expect for since, so allow a 10s variance.
  ok(Math.abs(settings.options.since - since) < 10000,
     "settings.options contains the expected since value.");

  let dataTypeSet = settings.dataToRemove;
  for (let key of Object.keys(dataTypeSet)) {
    equal(branch.getBoolPref(key.toLowerCase()), dataTypeSet[key], `${key} property of dataToRemove matches the expected pref.`);
  }

  dataTypeSet = settings.dataRemovalPermitted;
  for (let key of Object.keys(dataTypeSet)) {
    equal(true, dataTypeSet[key], `${key} property of dataRemovalPermitted is true.`);
  }

  // Explicitly set a pref to both true and false and then check.
  const SINGLE_PREF = "cache";

  do_register_cleanup(() => {
    branch.clearUserPref(SINGLE_PREF);
  });

  branch.setBoolPref(SINGLE_PREF, true);

  extension.sendMessage("settings");
  settings = yield extension.awaitMessage("settings");

  equal(settings.dataToRemove[SINGLE_PREF], true, "Preference that was set to true returns true.");

  branch.setBoolPref(SINGLE_PREF, false);

  extension.sendMessage("settings");
  settings = yield extension.awaitMessage("settings");

  equal(settings.dataToRemove[SINGLE_PREF], false, "Preference that was set to false returns false.");

  do_register_cleanup(() => {
    branch.clearUserPref(SINGLE_PREF);
  });

  yield extension.unload();
});
