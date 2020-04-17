/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-console */
const fs = require("fs");
const { mkdir } = require("shelljs");
const path = require("path");

// Note: DEFAULT_OPTIONS.baseUrl should match BASE_URL in aboutNewTabService.js
//       in mozilla-central.
const DEFAULT_OPTIONS = {
  addonPath: "..",
  baseUrl: "resource://activity-stream/",
};

/**
 * templateHTML - Generates HTML for activity stream, given some options and
 * prerendered HTML if necessary.
 *
 * @param  {obj} options
 *         {str} options.baseUrl        The base URL for all local assets
 *         {bool} options.debug         Should we use dev versions of JS libraries?
 *         {bool} options.noscripts     Should we include scripts in the prerendered files?
 * @return {str}         An HTML document as a string
 */
function templateHTML(options) {
  const debugString = options.debug ? "-dev" : "";
  // This list must match any similar ones in AboutNewTabService.jsm.
  const scripts = [
    "chrome://browser/content/contentSearchUI.js",
    "chrome://browser/content/contentSearchHandoffUI.js",
    "chrome://browser/content/contentTheme.js",
    `${options.baseUrl}vendor/react${debugString}.js`,
    `${options.baseUrl}vendor/react-dom${debugString}.js`,
    `${options.baseUrl}vendor/prop-types.js`,
    `${options.baseUrl}vendor/redux.js`,
    `${options.baseUrl}vendor/react-redux.js`,
    `${options.baseUrl}vendor/react-transition-group.js`,
    `${options.baseUrl}data/content/activity-stream.bundle.js`,
    `${options.baseUrl}data/content/newtab-render.js`,
  ];

  // Add spacing and script tags
  const scriptRender = `\n${scripts
    .map(script => `    <script src="${script}"></script>`)
    .join("\n")}`;

  return `
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta http-equiv="Content-Security-Policy" content="default-src 'none'; object-src 'none'; script-src resource: chrome:; connect-src https:; img-src https: data: blob:; style-src 'unsafe-inline';">
    <title data-l10n-id="newtab-page-title"></title>
    <link rel="icon" type="image/png" href="chrome://branding/content/icon32.png"/>
    <link rel="localization" href="branding/brand.ftl" />
    <link rel="localization" href="browser/branding/brandings.ftl" />
    <link rel="localization" href="browser/newtab/newtab.ftl" />
    <link rel="stylesheet" href="chrome://browser/content/contentSearchUI.css" />
    <link rel="stylesheet" href="${options.baseUrl}css/activity-stream.css" />
  </head>
  <body class="activity-stream">
    <div id="header-asrouter-container" role="presentation"></div>
    <div id="root"></div>
    <div id="footer-asrouter-container" role="presentation"></div>${
      options.noscripts ? "" : scriptRender
    }
  </body>
</html>
`.trimLeft();
}

/**
 * writeFiles - Writes to the desired files the result of a template given
 * various prerendered data and options.
 *
 * @param {string} destPath      Path to write the files to
 * @param {Map}    filesMap      Mapping of a string file name to templater
 * @param {Object} options       Various options for the templater
 */
function writeFiles(destPath, filesMap, options) {
  for (const [file, templater] of filesMap) {
    console.log("\x1b[32m", `✓ ${file}`, "\x1b[0m");
    fs.writeFileSync(path.join(destPath, file), templater({ options }));
  }
}

const STATIC_FILES = new Map([
  ["activity-stream.html", ({ options }) => templateHTML(options)],
  [
    "activity-stream-debug.html",
    ({ options }) => templateHTML(Object.assign({}, options, { debug: true })),
  ],
  [
    "activity-stream-noscripts.html",
    ({ options }) =>
      templateHTML(Object.assign({}, options, { noscripts: true })),
  ],
]);

/**
 * main - Parses command line arguments, generates html and js with templates,
 *        and writes files to their specified locations.
 */
function main() {
  // eslint-disable-line max-statements
  // This code parses command line arguments passed to this script.
  // Note: process.argv.slice(2) is necessary because the first two items in
  // process.argv are paths
  const args = require("minimist")(process.argv.slice(2), {
    alias: {
      addonPath: "a",
      baseUrl: "b",
    },
  });

  const options = Object.assign({ debug: false }, DEFAULT_OPTIONS, args || {});
  const addonPath = path.resolve(__dirname, options.addonPath);
  const prerenderedPath = path.join(addonPath, "prerendered");
  console.log(`Writing prerendered files to ${prerenderedPath}:`);

  mkdir("-p", prerenderedPath);
  writeFiles(prerenderedPath, STATIC_FILES, options);
}

main();
