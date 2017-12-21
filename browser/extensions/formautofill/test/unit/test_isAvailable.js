/**
 * Test enabling the feature in specific locales and regions.
 */

"use strict";

// Load bootstrap.js into a sandbox to be able to test `isAvailable`
let sandbox = {};
Services.scriptloader.loadSubScript(bootstrapURI, sandbox, "utf-8");
info("bootstrapURI: " + bootstrapURI);

add_task(async function test_defaultTestEnvironment() {
  Assert.ok(sandbox.isAvailable());
});

add_task(async function test_unsupportedRegion() {
  Services.prefs.setCharPref("extensions.formautofill.available", "detect");
  Services.prefs.setCharPref("browser.search.region", "ZZ");
  registerCleanupFunction(function cleanupRegion() {
    Services.prefs.clearUserPref("browser.search.region");
  });
  Assert.ok(!sandbox.isAvailable());
});

add_task(async function test_supportedRegion() {
  Services.prefs.setCharPref("extensions.formautofill.available", "detect");
  Services.prefs.setCharPref("browser.search.region", "US");
  registerCleanupFunction(function cleanupRegion() {
    Services.prefs.clearUserPref("browser.search.region");
  });
  Assert.ok(sandbox.isAvailable());
});
