/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node */

/**
 * This file contains a webpack loader which rewrites JS source files to use CSS
 * imports when running in Storybook. This allows JS files loaded in Storybook to use
 * chrome:// URIs when loading external stylesheets without having to worry
 * about Storybook being able to find and detect changes to the files.
 *
 * This loader allows Lit-based custom element code like this to work with
 * Storybook:
 *
 *    render() {
 *      return html`
 *        <link rel="stylesheet" href="chrome://global/content/elements/moz-toggle.css" />
 *        ...
 *      `;
 *    }
 *
 * By rewriting the source to this:
 *
 *    import moztoggleStyles from "toolkit/content/widgets/moz-toggle/moz-toggle.css";
 *    ...
 *    render() {
 *      return html`
 *        <link rel="stylesheet" href=${moztoggleStyles} />
 *        ...
 *      `;
 *    }
 *
 * It works similarly for vanilla JS custom elements that utilize template
 * strings. The following code:
 *
 *    static get markup() {
 *      return`
 *        <template>
 *          <link rel="stylesheet" href="chrome://browser/skin/migration/migration-wizard.css">
 *          ...
 *        </template>
 *      `;
 *    }
 *
 * Gets rewritten to:
 *
 *    import migrationwizardStyles from "browser/themes/shared/migration/migration-wizard.css";
 *    ...
 *    static get markup() {
 *      return`
 *        <template>
 *          <link rel="stylesheet" href=${migrationwizardStyles}>
 *          ...
 *        </template>
 *      `;
 *    }
 */

const path = require("path");
const projectRoot = path.join(process.cwd(), "../../..");
const rewriteChromeUri = require("./chrome-uri-utils.js");

/**
 * Return an array of the unique chrome:// CSS URIs referenced in this file.
 *
 * @param {string} source - The source file to scan.
 * @returns {string[]} Unique list of chrome:// CSS URIs
 */
function getReferencedChromeUris(source) {
  const chromeRegex = /chrome:\/\/.*?\.css/g;
  const matches = new Set();
  for (let match of source.matchAll(chromeRegex)) {
    // Add the full URI to the set of matches.
    matches.add(match[0]);
  }
  return [...matches];
}

/**
 * Replace references to chrome:// URIs with the relative path on disk from the
 * project root.
 *
 * @this {WebpackLoader} https://webpack.js.org/api/loaders/
 * @param {string} source - The source file to update.
 * @returns {string} The updated source.
 */
async function rewriteChromeUris(source) {
  const chromeUriToLocalPath = new Map();
  // We're going to rewrite the chrome:// URIs, find all referenced URIs.
  let chromeDependencies = getReferencedChromeUris(source);
  for (let chromeUri of chromeDependencies) {
    let localRelativePath = rewriteChromeUri(chromeUri);
    if (localRelativePath) {
      localRelativePath = localRelativePath.replaceAll("\\", "/");
      // Store the mapping to a local path for this chrome URI.
      chromeUriToLocalPath.set(chromeUri, localRelativePath);
      // Tell webpack the file being handled depends on the referenced file.
      this.addMissingDependency(path.join(projectRoot, localRelativePath));
    }
  }
  // Rewrite the source file with mapped chrome:// URIs.
  let rewrittenSource = source;
  for (let [chromeUri, localPath] of chromeUriToLocalPath.entries()) {
    // Generate an import friendly variable name for the default export from
    // the CSS file e.g. __chrome_styles_loader__moztoggleStyles.
    let cssImport = `__chrome_styles_loader__${path
      .basename(localPath, ".css")
      .replaceAll("-", "")}Styles`;

    // MozTextLabel is a special case for now since we don't use a template.
    if (
      this.resourcePath.endsWith("/moz-label.mjs") ||
      this.resourcePath.endsWith(".js")
    ) {
      rewrittenSource = rewrittenSource.replaceAll(`"${chromeUri}"`, cssImport);
    } else {
      rewrittenSource = rewrittenSource.replaceAll(
        chromeUri,
        `\$\{${cssImport}\}`
      );
    }

    // Add a CSS import statement as the first line in the file.
    rewrittenSource =
      `import ${cssImport} from "${localPath}";\n` + rewrittenSource;
  }
  return rewrittenSource;
}

/**
 * The WebpackLoader export. Runs async since apparently that's preferred.
 *
 * @param {string} source - The source to rewrite.
 * @param {Map} sourceMap - Source map data, unused.
 * @param {Object} meta - Metadata, unused.
 */
module.exports = async function chromeUriLoader(source) {
  // Get a callback to tell webpack when we're done.
  const callback = this.async();
  // Rewrite the source async since that appears to be preferred (and will be
  // necessary once we support rewriting CSS/SVG/etc).
  const newSource = await rewriteChromeUris.call(this, source);
  // Give webpack the rewritten content.
  callback(null, newSource);
};
