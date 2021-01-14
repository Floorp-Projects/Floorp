/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global browser */

// This tests the browser.experiments.urlbar.engagementTelemetry WebExtension
// Experiment API.

"use strict";

add_settings_tasks("browser.urlbar.extension.timeout", "number", () => {
  browser.test.onMessage.addListener(async (method, arg) => {
    let result = await browser.experiments.urlbar.extensionTimeout[method](arg);
    browser.test.sendMessage("done", result);
  });
});
