const path = require("path");

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

module.exports = function(config) {
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
    ],
    coverageIstanbulReporter: {
      reports: ["html", "text-summary"],
      dir: PATHS.coverageReportingPath,
      // This will make karma fail if coverage reporting is less than the minimums here
      thresholds: !isTDD && {
        each: {
          statements: 100,
          lines: 100,
          functions: 100,
          branches: 66,
          overrides: {
            "lib/ActivityStreamStorage.jsm": {
              statements: 100,
              lines: 100,
              functions: 100,
              branches: 83,
            },
            "lib/UTEventReporting.jsm": {
              statements: 100,
              lines: 100,
              functions: 100,
              branches: 75,
            },
            "lib/*.jsm": {
              statements: 100,
              lines: 100,
              functions: 100,
              branches: 84,
            },
            "content-src/components/DiscoveryStreamComponents/**/*.jsx": {
              statements: 65.2,
              lines: 65.2,
              functions: 50,
              branches: 50,
            },
            "content-src/asrouter/**/*.jsx": {
              statements: 57,
              lines: 58,
              functions: 60,
              branches: 50,
            },
            "content-src/components/ASRouterAdmin/*.jsx": {
              statements: 0,
              lines: 0,
              functions: 0,
              branches: 0,
            },
            "content-src/components/**/*.jsx": {
              statements: 51.1,
              lines: 53.6,
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
      // This loader allows us to override required files in tests
      resolveLoader: {alias: {inject: path.join(__dirname, "loaders/inject-loader")}},
      // This resolve config allows us to import with paths relative to the root directory, e.g. "lib/ActivityStream.jsm"
      resolve: {
        extensions: [".js", ".jsx"],
        modules: [
          PATHS.moduleResolveDirectory,
          "node_modules",
        ],
      },
      externals: {
        // enzyme needs these for backwards compatibility with 0.13.
        // see https://github.com/airbnb/enzyme/blob/master/docs/guides/webpack.md#using-enzyme-with-webpack
        "react/addons": true,
        "react/lib/ReactContext": true,
        "react/lib/ExecutionEnvironment": true,
      },
      module: {
        rules: [
          // This rule rewrites importing/exporting in .jsm files to be compatible with esmodules
          {
            test: /\.jsm$/,
            exclude: [/node_modules/],
            use: [{
              loader: "babel-loader", // require("babel-core")
              options: {
                plugins: [
                  // Converts .jsm files into common-js modules
                  ["jsm-to-commonjs", {basePath: PATHS.resourcePathRegEx, removeOtherImports: true, replace: true}], // require("babel-plugin-jsm-to-commonjs")
                ],
              },
            }],
          },
          {
            test: /\.js$/,
            exclude: [/node_modules\/(?!(fluent|fluent-react)\/).*/, /test/],
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
            test: /\.js[mx]?$/,
            loader: "istanbul-instrumenter-loader",
            options: {esModules: true},
            include: [
              path.resolve("content-src"),
              path.resolve("lib"),
              path.resolve("common"),
            ],
            exclude: [
              path.resolve("test"),
              path.resolve("vendor"),
              path.resolve("lib/ASRouterTargeting.jsm"),
              path.resolve("lib/ASRouterTriggerListeners.jsm"),
              path.resolve("lib/OnboardingMessageProvider.jsm"),
              path.resolve("lib/CFRMessageProvider.jsm"),
              path.resolve("lib/CFRPageActions.jsm"),
            ],
          },
        ],
      },
    },
    // Silences some overly-verbose logging of individual module builds
    webpackMiddleware: {noInfo: true},
  });
};
