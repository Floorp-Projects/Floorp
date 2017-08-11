/**
 * Test enabling the feature in specific locales and regions.
 */

"use strict";

// Load bootstrap.js into a sandbox to be able to test `isAvailable`
let sandbox = {};
Services.scriptloader.loadSubScript(bootstrapURI, sandbox, "utf-8");
do_print("bootstrapURI: " + bootstrapURI);

add_task(async function test_defaultTestEnvironment() {
  do_check_true(sandbox.isAvailable());
});

add_task(async function test_unsupportedRegion() {
  Services.prefs.setCharPref("extensions.formautofill.available", "detect");
  Services.prefs.setCharPref("browser.search.region", "ZZ");
  do_register_cleanup(function cleanupRegion() {
    Services.prefs.clearUserPref("browser.search.region");
  });
  do_check_false(sandbox.isAvailable());
});

add_task(async function test_supportedRegion() {
  Services.prefs.setCharPref("extensions.formautofill.available", "detect");
  Services.prefs.setCharPref("browser.search.region", "US");
  do_register_cleanup(function cleanupRegion() {
    Services.prefs.clearUserPref("browser.search.region");
  });
  do_check_true(sandbox.isAvailable());
});
