/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const path = require("path");
const fs = require("fs");
const { rollup } = require("rollup");
const nodeResolve = require("@rollup/plugin-node-resolve");
const commonjs = require("@rollup/plugin-commonjs");
const injectProcessEnv = require("rollup-plugin-inject-process-env");
const nodePolyfills = require("rollup-plugin-node-polyfills");

const webpack = require("webpack");

const projectPath = path.resolve(__dirname, "..");
const bundlePath = path.join(projectPath, "./dist");

process.env.NODE_ENV = "production";

function getEntry(filename) {
  return path.join(__dirname, "..", filename);
}

/**
 * The `bundle` module will build the following:
 * - vendors.js and vendors.css:
 *     Bundle for all the external packages still used by the Debugger frontend.
 *     Source at devtools/client/debugger/src/vendors.js
 * - parser-worker.js, pretty-print-worker.js, search-worker:
 *     Workers used only by the debugger.
 *     Sources at devtools/client/debugger/src/workers/*
 */
(async function bundle() {
  const rollupSucceeded = await bundleRollup();
  const webpackSucceeded = await bundleWebpack();
  const failed = !rollupSucceeded || !webpackSucceeded;
  process.exit(failed ? 1 : 0);
})();

/**
 * Generates all dist/*-worker.js files
 */
async function bundleRollup() {
  console.log(`[bundle|rollup] Start bundling…`);

  let success = true;

  // We need to handle workers 1 by 1 to be able to generate umd bundles.
  const entries = {
    "parser-worker": getEntry("src/workers/parser/worker.js"),
    "pretty-print-worker": getEntry("src/workers/pretty-print/worker.js"),
    "search-worker": getEntry("src/workers/search/worker.js"),
  };

  for (const [entryName, input] of Object.entries(entries)) {
    let bundle;
    try {
      // create a bundle
      bundle = await rollup({
        input: {
          [entryName]: input,
        },
        plugins: [
          commonjs({
            transformMixedEsModules: true,
            strictRequires: true,
          }),
          injectProcessEnv({ NODE_ENV: "production" }),
          nodeResolve(),
          // read-wasm.js is part of source-map and is only for Node environment.
          // we need to ignore it, otherwise __dirname is inlined with the path the bundle
          // is generated from, which makes the verify-bundle task fail
          nodePolyfills({ exclude: [/read-wasm\.js/] }),
        ],
      });
      await bundle.write({
        dir: bundlePath,
        entryFileNames: "[name].js",
        format: "umd",
      });
    } catch (error) {
      success = false;
      // do some error reporting
      console.error("[bundle|rollup] Something went wrong.", error);
    }
    if (bundle) {
      // closes the bundle
      await bundle.close();
    }
  }

  console.log(`[bundle|rollup] Done bundling`);
  return success;
}

/**
 * Generates vendors.js and vendors.css
 * Should be removed in Bug 1826501
 */
async function bundleWebpack() {
  let success = true;
  console.log(`[bundle|webpack] Start bundling…`);
  process.env.TARGET = "firefox-panel";
  process.env.OUTPUT_PATH = bundlePath;

  const webpackConfig = require(path.resolve(projectPath, "webpack.config.js"));
  const webpackCompiler = webpack(webpackConfig);

  const result = await new Promise(resolve => {
    webpackCompiler.run((error, stats) => resolve(stats));
  });

  if (result?.hasErrors()) {
    success = false;
    console.log(
      "[bundle|webpack] Something went wrong. The error was written to assets-error.log"
    );

    fs.writeFileSync(
      "assets-error.log",
      JSON.stringify(result.toJson("verbose"), null, 2)
    );
  }

  console.log(`[bundle|webpack] Done bundling`);
  return success;
}
