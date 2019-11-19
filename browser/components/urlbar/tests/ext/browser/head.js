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

async function loadExtension(background) {
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
    },
    isPrivileged: true,
    background,
  });
  await ext.startup();
  return ext;
}
