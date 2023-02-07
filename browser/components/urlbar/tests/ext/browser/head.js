/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * The files in this directory test the browser.urlbarExperiments WebExtension
 * Experiment APIs, which are the WebExtension APIs we ship in our urlbar
 * experiment extensions.  Unlike the WebExtension APIs we ship in mozilla-
 * central, which have continuous test coverage [1], our WebExtension Experiment
 * APIs would not have continuous test coverage were it not for the fact that we
 * copy and test them here.  This is especially useful for APIs that are used in
 * experiments that target multiple versions of Firefox, and for APIs that are
 * reused in multiple experiments.  See [2] and [3] for more info on
 * experiments.
 *
 * [1] See browser/components/extensions/test
 * [2] browser/components/urlbar/docs/experiments.rst
 * [3] https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/basics.html#webextensions-experiments
 */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-common.js",
  this
);

ChromeUtils.defineESModuleGetters(this, {
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
});

const SCHEMA_BASENAME = "schema.json";
const SCRIPT_BASENAME = "api.js";

const SCHEMA_PATH = getTestFilePath(SCHEMA_BASENAME);
const SCRIPT_PATH = getTestFilePath(SCRIPT_BASENAME);

let schemaSource;
let scriptSource;

add_setup(async function loadSource() {
  schemaSource = await (await fetch("file://" + SCHEMA_PATH)).text();
  scriptSource = await (await fetch("file://" + SCRIPT_PATH)).text();
});

/**
 * Loads a mock extension with our browser.experiments.urlbar API and a
 * background script.  Be sure to call `await ext.unload()` when you're done
 * with it.
 *
 * @param {object} options
 *   Options object
 * @param {Function} options.background
 *   This function is serialized and becomes the background script.
 * @param {object} [options.extraFiles]
 *   Extra files to load in the extension.
 * @returns {object}
 *   The extension.
 */
async function loadExtension({ background, extraFiles = {} }) {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
      experiment_apis: {
        experiments_urlbar: {
          schema: SCHEMA_BASENAME,
          parent: {
            scopes: ["addon_parent"],
            paths: [["experiments", "urlbar"]],
            script: SCRIPT_BASENAME,
          },
        },
      },
    },
    files: {
      [SCHEMA_BASENAME]: schemaSource,
      [SCRIPT_BASENAME]: scriptSource,
      ...extraFiles,
    },
    isPrivileged: true,
    background,
  });
  await ext.startup();
  return ext;
}

/**
 * Tests toggling a preference value via an experiments.urlbar API.
 *
 * @param {string} prefName
 *   The name of the pref to be tested.
 * @param {string} type
 *   The type of the pref being set. One of "string", "boolean", or "number".
 * @param {Function} background
 *   Boilerplate function that returns the value from calling the
 *   browser.experiments.urlbar.prefName[method] APIs.
 */
function add_settings_tasks(prefName, type, background) {
  let defaultPreferences = new Preferences({ defaultBranch: true });

  let originalValue = defaultPreferences.get(prefName);
  registerCleanupFunction(() => {
    defaultPreferences.set(prefName, originalValue);
  });

  let firstValue, secondValue;
  switch (type) {
    case "string":
      firstValue = "test value 1";
      secondValue = "test value 2";
      break;
    case "number":
      firstValue = 10;
      secondValue = 100;
      break;
    case "boolean":
      firstValue = false;
      secondValue = true;
      break;
    default:
      Assert.ok(
        false,
        `"type" parameter must be one of "string", "number", or "boolean"`
      );
  }

  add_task(async function get() {
    let ext = await loadExtension({ background });

    defaultPreferences.set(prefName, firstValue);
    ext.sendMessage("get", {});
    let result = await ext.awaitMessage("done");
    Assert.strictEqual(result.value, firstValue);

    defaultPreferences.set(prefName, secondValue);
    ext.sendMessage("get", {});
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result.value, secondValue);

    await ext.unload();
  });

  add_task(async function set() {
    let ext = await loadExtension({ background });

    defaultPreferences.set(prefName, firstValue);
    ext.sendMessage("set", { value: secondValue });
    let result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), secondValue);

    ext.sendMessage("set", { value: firstValue });
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), firstValue);

    await ext.unload();
  });

  add_task(async function clear() {
    // no set()
    defaultPreferences.set(prefName, firstValue);
    let ext = await loadExtension({ background });
    ext.sendMessage("clear", {});
    let result = await ext.awaitMessage("done");
    Assert.strictEqual(result, false);
    Assert.strictEqual(defaultPreferences.get(prefName), firstValue);
    await ext.unload();

    // firstValue -> secondValue
    defaultPreferences.set(prefName, firstValue);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: secondValue });
    await ext.awaitMessage("done");
    ext.sendMessage("clear", {});
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), firstValue);
    await ext.unload();

    // secondValue -> firstValue
    defaultPreferences.set(prefName, secondValue);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: firstValue });
    await ext.awaitMessage("done");
    ext.sendMessage("clear", {});
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), secondValue);
    await ext.unload();

    // firstValue -> firstValue
    defaultPreferences.set(prefName, firstValue);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: firstValue });
    await ext.awaitMessage("done");
    ext.sendMessage("clear", {});
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), firstValue);
    await ext.unload();

    // secondValue -> secondValue
    defaultPreferences.set(prefName, secondValue);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: secondValue });
    await ext.awaitMessage("done");
    ext.sendMessage("clear", {});
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), secondValue);
    await ext.unload();
  });

  add_task(async function shutdown() {
    // no set()
    defaultPreferences.set(prefName, firstValue);
    let ext = await loadExtension({ background });
    await ext.unload();
    Assert.strictEqual(defaultPreferences.get(prefName), firstValue);

    // firstValue -> secondValue
    defaultPreferences.set(prefName, firstValue);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: secondValue });
    await ext.awaitMessage("done");
    await ext.unload();
    Assert.strictEqual(defaultPreferences.get(prefName), firstValue);

    // secondValue -> firstValue
    defaultPreferences.set(prefName, secondValue);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: firstValue });
    await ext.awaitMessage("done");
    await ext.unload();
    Assert.strictEqual(defaultPreferences.get(prefName), secondValue);

    // firstValue -> firstValue
    defaultPreferences.set(prefName, firstValue);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: firstValue });
    await ext.awaitMessage("done");
    await ext.unload();
    Assert.strictEqual(defaultPreferences.get(prefName), firstValue);

    // secondValue -> secondValue
    defaultPreferences.set(prefName, secondValue);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: secondValue });
    await ext.awaitMessage("done");
    await ext.unload();
    Assert.strictEqual(defaultPreferences.get(prefName), secondValue);
  });
}
