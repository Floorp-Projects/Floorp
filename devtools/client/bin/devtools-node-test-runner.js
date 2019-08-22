/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global __dirname, process */

"use strict";

/**
 * This is a test runner dedicated to run DevTools node tests continuous integration
 * platforms. It will parse the logs to output errors compliant with treeherder tooling.
 *
 * See taskcluster/ci/source-test/node.yml for the definition of the task running those
 * tests on try.
 */

const { execFileSync } = require("child_process");
const { chdir } = require("process");
const path = require("path");

// Supported node test suites for DevTools
const TEST_TYPES = {
  JEST: "jest",
  MOCHA: "mocha",
};

const SUITES = {
  "aboutdebugging-new": {
    path: "../aboutdebugging-new/test/jest",
    type: TEST_TYPES.JEST,
  },
  accessibility: {
    path: "../accessibility/test/jest",
    type: TEST_TYPES.JEST,
  },
  application: {
    path: "../application/test/components",
    type: TEST_TYPES.JEST,
  },
  framework: {
    path: "../framework/test/jest",
    type: TEST_TYPES.JEST,
  },
  netmonitor: {
    path: "../netmonitor/test/node",
    type: TEST_TYPES.JEST,
  },
  webconsole: {
    path: "../webconsole/test/node",
    type: TEST_TYPES.MOCHA,
  },
};

function execOut(...args) {
  let out;
  let err;
  try {
    out = execFileSync(...args);
  } catch (e) {
    out = e.stdout;
    err = e.stderr;
  }
  return { out: out.toString(), err: err && err.toString() };
}

function getErrors(suite, out, err) {
  switch (SUITES[suite].type) {
    case TEST_TYPES.JEST:
      return getJestErrors(out, err);
    case TEST_TYPES.MOCHA:
      return getMochaErrors(out, err);
    default:
      throw new Error("Unsupported suite type: " + SUITES[suite].type);
  }
}

function getJestErrors(out, err) {
  // The string out has extra content before the JSON object starts.
  const jestJsonOut = out.substring(out.indexOf("{"), out.lastIndexOf("}") + 1);
  const results = JSON.parse(jestJsonOut);

  // The individual failing tests are jammed into the same message string :/
  return results.testResults.reduce((p, testResult) => {
    const failures = testResult.message
      .split("\n")
      .filter(l => l.includes("●"));
    return p.concat(failures);
  }, []);
}

function getMochaErrors(out, err) {
  // With mocha tests, the command itself contains curly braces already so we need
  // to find the first brace after the first line.
  const firstRelevantBracket = out.indexOf("{", out.indexOf("--reporter json"));
  const mochaJsonOut = out.substring(
    firstRelevantBracket,
    out.lastIndexOf("}") + 1
  );
  const results = JSON.parse(mochaJsonOut);
  if (!results.failures) {
    // No failures, return an empty array.
    return [];
  }
  return results.failures.map(
    failure => failure.fullTitle + " | " + failure.err.message
  );
}

function runTests() {
  console.log("[devtools-node-test-runner] Extract suite argument");
  const suiteArg = process.argv.find(arg => arg.includes("suite="));
  const suite = suiteArg.split("=")[1];
  if (!SUITES[suite]) {
    throw new Error(
      "Invalid suite argument to devtools-node-test-runner: " + suite
    );
  }

  console.log("[devtools-node-test-runner] Found test suite: " + suite);
  const testPath = path.join(__dirname, SUITES[suite].path);
  chdir(testPath);

  console.log("[devtools-node-test-runner] Check `yarn` is available");
  try {
    // This will throw if yarn is unavailable
    execFileSync("yarn", ["--version"]);
  } catch (e) {
    console.log(
      "[devtools-node-test-runner] ERROR: `yarn` is not installed. " +
        "See https://yarnpkg.com/docs/install/ "
    );
    return false;
  }

  console.log("[devtools-node-test-runner] Run `yarn` in test folder");
  execOut("yarn");

  console.log(`TEST START | ${SUITES[suite].type} | ${suite}`);

  console.log("[devtools-node-test-runner] Run `yarn test` in test folder");
  const { out, err } = execOut("yarn", ["test-ci"]);

  if (err) {
    console.log("[devtools-node-test-runner] Error log");
    console.log(err);
  }

  console.log("[devtools-node-test-runner] Parse errors from the test logs");
  const errors = getErrors(suite, out, err) || [];
  for (const error of errors) {
    console.log(
      `TEST-UNEXPECTED-FAIL | ${SUITES[suite].type} | ${suite} | ${error}`
    );
  }

  const success = errors.length === 0;
  if (success) {
    console.log(`[devtools-node-test-runner] Test suite [${suite}] succeeded`);
  } else {
    console.log(`[devtools-node-test-runner] Test suite [${suite}] failed`);
    console.log(
      "[devtools-node-test-runner] You can find documentation about the " +
        "devtools node tests at https://docs.firefox-dev.tools/tests/node-tests.html"
    );
  }
  return success;
}

process.exitCode = runTests() ? 0 : 1;
