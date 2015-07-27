/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env node */

module.exports = function(config) {
  "use strict";

  return {

    // Base path that will be used to resolve all patterns (eg. files, exclude).
    basePath: "../../",

    // List of files / patterns to load in the browser.
    files: [],

    // List of files to exclude.
    exclude: [
    ],

    // Frameworks to use.
    // Available frameworks: https://npmjs.org/browse/keyword/karma-adapter .
    frameworks: ["mocha"],

    // Test results reporter to use.
    // Possible values: "dots", "progress".
    // Available reporters: https://npmjs.org/browse/keyword/karma-reporter .
    reporters: ["progress", "coverage"],

    coverageReporter: {
      type: "html",
      dir: "test/coverage/"
    },

    // Web server port.
    port: 9876,

    // Enable / disable colors in the output (reporters and logs).
    colors: true,

    // Level of logging.
    // Possible values: config.LOG_DISABLE || config.LOG_ERROR || config.LOG_WARN || config.LOG_INFO || config.LOG_DEBUG.
    logLevel: config.LOG_INFO,

    // Enable / disable watching file and executing tests whenever any file changes.
    autoWatch: false,

    // Start these browsers
    // Available browser launchers: https://npmjs.org/browse/keyword/karma-launcher .
    browsers: ["Firefox"],

    // Continuous Integration mode.
    // If true, Karma captures browsers, runs the tests and exits.
    singleRun: true,

    // Capture console output.
    client: {
      captureConsole: true
    },

    plugins: [
      "karma-coverage",
      "karma-mocha",
      "karma-firefox-launcher"
    ]
  };
};
