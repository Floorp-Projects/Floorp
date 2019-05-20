/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const toolbox = require("devtools-launchpad/index");
const sourceMapAssets = require("devtools-source-map/assets");
const CopyWebpackPlugin = require("copy-webpack-plugin");

const getConfig = require("./bin/getConfig");
const mozillaCentralMappings = require("./configs/mozilla-central-mappings");
const path = require("path");
const ObjectRestSpreadPlugin = require("@sucrase/webpack-object-rest-spread-plugin");

const isProduction = process.env.NODE_ENV === "production";

/*
 * builds a path that's relative to the project path
 * returns an array so that we can prepend
 * hot-module-reloading in local development
 */
function getEntry(filename) {
  return [path.join(__dirname, filename)];
}

const webpackConfig = {
  entry: {
    // We always generate the debugger bundle, but we will only copy the CSS
    // artifact over to mozilla-central.
    "parser-worker": getEntry("src/workers/parser/worker.js"),
    "pretty-print-worker": getEntry("src/workers/pretty-print/worker.js"),
    "search-worker": getEntry("src/workers/search/worker.js"),
    "source-map-worker": getEntry("packages/devtools-source-map/src/worker.js"),
    "source-map-index": getEntry("packages/devtools-source-map/src/index.js"),
  },

  output: {
    path: path.join(__dirname, "assets/build"),
    filename: "[name].js",
    publicPath: "/assets/build",
  },

  plugins: [
    new CopyWebpackPlugin(
      Object.entries(sourceMapAssets).map(([name, filePath]) => ({
        from: filePath,
        to: `source-map-worker-assets/${name}`,
      }))
    ),
  ],
};

if (isProduction) {
  // In the firefox panel, build the vendored dependencies as a bundle instead,
  // the other debugger modules will be transpiled to a format that is
  // compatible with the DevTools Loader.
  webpackConfig.entry.vendors = getEntry("src/vendors.js");
  webpackConfig.entry.reps = getEntry("packages/devtools-reps/src/index.js");
} else {
  webpackConfig.entry.debugger = getEntry("src/main.development.js");
}

const envConfig = getConfig();

const extra = {
  babelIncludes: ["react-aria-components"],
};

webpackConfig.plugins.push(new ObjectRestSpreadPlugin());

if (!isProduction) {
  webpackConfig.module = webpackConfig.module || {};
  webpackConfig.module.rules = webpackConfig.module.rules || [];
} else {
  webpackConfig.output.libraryTarget = "umd";
  extra.excludeMap = mozillaCentralMappings;
  extra.recordsPath = "bin/module-manifest.json";
}

module.exports = toolbox.toolboxConfig(webpackConfig, envConfig, extra);
