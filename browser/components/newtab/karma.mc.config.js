/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const path = require("path");
const webpack = require("webpack");
const { ResourceUriPlugin } = require("./tools/resourceUriPlugin");

const PATHS = {
  // Where is the entry point for the unit tests?
  testEntryFile: path.resolve(__dirname, "test/unit/unit-entry.js"),

  // A glob-style pattern matching all unit tests
  testFilesPattern: "test/unit/**/*.js",

  // The base directory of all source files (used for path resolution in webpack importing)
  moduleResolveDirectory: __dirname,

  // a RegEx matching all Cu.import statements of local files
  resourcePathRegEx: /^resource:\/\/activity-stream\//,

  coverageReportingPath: "logs/coverage/",
};

// When tweaking here, be sure to review the docs about the execution ordering
// semantics of the preprocessors array, as they are somewhat odd.
const preprocessors = {};
preprocessors[PATHS.testFilesPattern] = [
  "webpack", // require("karma-webpack")
  "sourcemap", // require("karma-sourcemap-loader")
];

module.exports = function (config) {
  const isTDD = config.tdd;
  const browsers = isTDD ? ["Firefox"] : ["FirefoxHeadless"]; // require("karma-firefox-launcher")
  config.set({
    singleRun: !isTDD,
    browsers,
    customLaunchers: {
      FirefoxHeadless: {
        base: "Firefox",
        flags: ["--headless"],
      },
    },
    frameworks: [
      "chai", // require("chai") require("karma-chai")
      "mocha", // require("mocha") require("karma-mocha")
      "sinon", // require("sinon") require("karma-sinon")
    ],
    reporters: [
      "coverage-istanbul", // require("karma-coverage")
      "mocha", // require("karma-mocha-reporter")

      // for bin/try-runner.js to parse the output easily
      "json", // require("karma-json-reporter")
    ],
    jsonReporter: {
      // So this doesn't get interleaved with other karma output
      stdout: false,
      outputFile: path.join("logs", "karma-run-results.json"),
    },
    coverageIstanbulReporter: {
      reports: ["lcov", "text-summary"], // for some reason "lcov" reallys means "lcov" and "html"
      "report-config": {
        // so the full m-c path gets printed; needed for https://coverage.moz.tools/ integration
        lcov: {
          projectRoot: "../../..",
        },
      },
      dir: PATHS.coverageReportingPath,
      // This will make karma fail if coverage reporting is less than the minimums here
      thresholds: !isTDD && {
        each: {
          statements: 100,
          lines: 100,
          functions: 100,
          branches: 66,
          overrides: {
            "lib/AboutPreferences.sys.mjs": {
              statements: 98,
              lines: 98,
              functions: 94,
              branches: 66,
            },
            /**
             * TelemetryFeed.sys.mjs is tested via an xpcshell test
             */
            "lib/TelemetryFeed.sys.mjs": {
              statements: 10,
              lines: 10,
              functions: 9,
              branches: 0,
            },
            "content-src/lib/init-store.js": {
              statements: 98,
              lines: 98,
              functions: 100,
              branches: 100,
            },
            "lib/ActivityStreamStorage.sys.mjs": {
              statements: 100,
              lines: 100,
              functions: 100,
              branches: 83,
            },
            "lib/DownloadsManager.sys.mjs": {
              statements: 100,
              lines: 100,
              functions: 100,
              branches: 78,
            },
            /**
             * PlacesFeed.sys.mjs is tested via an xpcshell test
             */
            "lib/PlacesFeed.sys.mjs": {
              statements: 7,
              lines: 7,
              functions: 8,
              branches: 0,
            },
            "lib/UTEventReporting.sys.mjs": {
              statements: 100,
              lines: 100,
              functions: 100,
              branches: 75,
            },
            "lib/Screenshots.sys.mjs": {
              statements: 94,
              lines: 94,
              functions: 75,
              branches: 84,
            },
            /**
             * Store.sys.mjs is tested via an xpcshell test
             */
            "lib/Store.sys.mjs": {
              statements: 8,
              lines: 8,
              functions: 0,
              branches: 0,
            },
            /**
             * TopSitesFeed.sys.mjs is tested via an xpcshell test
             */
            "lib/TopSitesFeed.sys.mjs": {
              statements: 9,
              lines: 9,
              functions: 5,
              branches: 0,
            },
            /**
             * TopStoresFeed.sys.mjs is not tested in automation and is slated
             * for eventual removal.
             */
            "lib/TopStoriesFeed.sys.mjs": {
              statements: 0,
              lines: 0,
              functions: 0,
              branches: 0,
            },
            "content-src/components/DiscoveryStreamComponents/**/*.jsx": {
              statements: 90.48,
              lines: 90.48,
              functions: 85.71,
              branches: 68.75,
            },
            "content-src/asrouter/**/*.jsx": {
              statements: 57,
              lines: 58,
              functions: 60,
              branches: 50,
            },
            "content-src/components/DiscoveryStreamAdmin/*.jsx": {
              statements: 0,
              lines: 0,
              functions: 0,
              branches: 0,
            },
            "content-src/components/CustomizeMenu/**/*.jsx": {
              statements: 0,
              lines: 0,
              functions: 0,
              branches: 0,
            },
            "content-src/components/CustomizeMenu/*.jsx": {
              statements: 0,
              lines: 0,
              functions: 0,
              branches: 0,
            },
            "content-src/lib/link-menu-options.js": {
              statements: 96,
              lines: 96,
              functions: 96,
              branches: 70,
            },
            "content-src/components/**/*.jsx": {
              statements: 51.1,
              lines: 52.38,
              functions: 31.2,
              branches: 31.2,
            },
          },
        },
      },
    },
    files: [PATHS.testEntryFile],
    preprocessors,
    webpack: {
      mode: "none",
      devtool: "inline-source-map",
      // This resolve config allows us to import with paths relative to the root directory, e.g. "lib/ActivityStream.sys.mjs"
      resolve: {
        extensions: [".js", ".jsx", ".mjs"],
        modules: [PATHS.moduleResolveDirectory, "node_modules"],
        alias: {
          asrouter: path.join(__dirname, "../asrouter"),
        },
      },
      plugins: [
        // The ResourceUriPlugin handles translating resource URIs in import
        // statements in .mjs files to paths on the filesystem.
        new ResourceUriPlugin({
          resourcePathRegExes: [
            [
              new RegExp("^resource://activity-stream/"),
              path.join(__dirname, "./"),
            ],
          ],
        }),
        new webpack.DefinePlugin({
          "process.env.NODE_ENV": JSON.stringify("development"),
        }),
      ],
      externals: {
        // enzyme needs these for backwards compatibility with 0.13.
        // see https://github.com/airbnb/enzyme/blob/master/docs/guides/webpack.md#using-enzyme-with-webpack
        "react/addons": true,
        "react/lib/ReactContext": true,
        "react/lib/ExecutionEnvironment": true,
      },
      module: {
        rules: [
          {
            test: /\.js$/,
            exclude: [/node_modules\/(?!@fluent\/).*/, /test/],
            loader: "babel-loader",
          },
          {
            test: /\.jsx$/,
            exclude: /node_modules/,
            loader: "babel-loader",
            options: {
              presets: ["@babel/preset-react"],
            },
          },
          {
            test: /\.md$/,
            use: "raw-loader",
          },
          {
            enforce: "post",
            test: /\.js[x]?$/,
            loader: "@jsdevtools/coverage-istanbul-loader",
            options: { esModules: true },
            include: [
              path.resolve("content-src"),
              path.resolve("lib"),
              path.resolve("common"),
            ],
            exclude: [path.resolve("test"), path.resolve("vendor")],
          },
        ],
      },
    },
    // Silences some overly-verbose logging of individual module builds
    webpackMiddleware: { noInfo: true },
  });
};
