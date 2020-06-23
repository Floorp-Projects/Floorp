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

/* import-globals-from ../../browser/head-common.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/ext/browser/head-common.js",
  this
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Preferences: "resource://gre/modules/Preferences.jsm",
});

const SCHEMA_BASENAME = "schema.json";
const SCRIPT_BASENAME = "api.js";

const SCHEMA_PATH = getTestFilePath(SCHEMA_BASENAME);
const SCRIPT_PATH = getTestFilePath(SCRIPT_BASENAME);

let schemaSource;
let scriptSource;

add_task(async function loadSource() {
  schemaSource = await (await fetch("file://" + SCHEMA_PATH)).text();
  scriptSource = await (await fetch("file://" + SCRIPT_PATH)).text();
});

/**
 * Loads a mock extension with our browser.experiments.urlbar API and a
 * background script.  Be sure to call `await ext.unload()` when you're done
 * with it.
 *
 * @param {function} background
 *   This function is serialized and becomes the background script.
 * @param {object} extraFiles
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

function add_settings_tasks(prefName, background) {
  let defaultPreferences = new Preferences({ defaultBranch: true });

  let originalValue = defaultPreferences.get(prefName);
  registerCleanupFunction(() => {
    defaultPreferences.set(prefName, originalValue);
  });

  add_task(async function get() {
    let ext = await loadExtension({ background });

    defaultPreferences.set(prefName, false);
    ext.sendMessage("get", {});
    let result = await ext.awaitMessage("done");
    Assert.strictEqual(result.value, false);

    defaultPreferences.set(prefName, true);
    ext.sendMessage("get", {});
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result.value, true);

    await ext.unload();
  });

  add_task(async function set() {
    let ext = await loadExtension({ background });

    defaultPreferences.set(prefName, false);
    ext.sendMessage("set", { value: true });
    let result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), true);

    ext.sendMessage("set", { value: false });
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), false);

    await ext.unload();
  });

  add_task(async function clear() {
    // no set()
    defaultPreferences.set(prefName, false);
    let ext = await loadExtension({ background });
    ext.sendMessage("clear", {});
    let result = await ext.awaitMessage("done");
    Assert.strictEqual(result, false);
    Assert.strictEqual(defaultPreferences.get(prefName), false);
    await ext.unload();

    // false -> true
    defaultPreferences.set(prefName, false);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: true });
    await ext.awaitMessage("done");
    ext.sendMessage("clear", {});
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), false);
    await ext.unload();

    // true -> false
    defaultPreferences.set(prefName, true);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: false });
    await ext.awaitMessage("done");
    ext.sendMessage("clear", {});
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), true);
    await ext.unload();

    // false -> false
    defaultPreferences.set(prefName, false);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: false });
    await ext.awaitMessage("done");
    ext.sendMessage("clear", {});
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), false);
    await ext.unload();

    // true -> true
    defaultPreferences.set(prefName, true);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: true });
    await ext.awaitMessage("done");
    ext.sendMessage("clear", {});
    result = await ext.awaitMessage("done");
    Assert.strictEqual(result, true);
    Assert.strictEqual(defaultPreferences.get(prefName), true);
    await ext.unload();
  });

  add_task(async function shutdown() {
    // no set()
    defaultPreferences.set(prefName, false);
    let ext = await loadExtension({ background });
    await ext.unload();
    Assert.strictEqual(defaultPreferences.get(prefName), false);

    // false -> true
    defaultPreferences.set(prefName, false);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: true });
    await ext.awaitMessage("done");
    await ext.unload();
    Assert.strictEqual(defaultPreferences.get(prefName), false);

    // true -> false
    defaultPreferences.set(prefName, true);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: false });
    await ext.awaitMessage("done");
    await ext.unload();
    Assert.strictEqual(defaultPreferences.get(prefName), true);

    // false -> false
    defaultPreferences.set(prefName, false);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: false });
    await ext.awaitMessage("done");
    await ext.unload();
    Assert.strictEqual(defaultPreferences.get(prefName), false);

    // true -> true
    defaultPreferences.set(prefName, true);
    ext = await loadExtension({ background });
    ext.sendMessage("set", { value: true });
    await ext.awaitMessage("done");
    await ext.unload();
    Assert.strictEqual(defaultPreferences.get(prefName), true);
  });
}
