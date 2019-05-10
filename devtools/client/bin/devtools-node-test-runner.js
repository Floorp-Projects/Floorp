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
  "framework": {
    path: "../framework/test/jest",
    type: TEST_TYPES.JEST,
  },
  "netmonitor": {
    path: "../netmonitor/test",
    type: TEST_TYPES.JEST,
  },
  "webconsole": {
    path: "../webconsole/test",
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
      // jest errors are logged in the `err` buffer.
      return parseErrorsFromLogs(err, / {4}✕\s*(.*)/);
    case TEST_TYPES.MOCHA:
      // mocha errors are logged in the `out` buffer, and will either log Error or
      // TypeError depending on the error detected.
      return parseErrorsFromLogs(out, / {4}((?:Type)?Error\:.*)/);
    default:
      throw new Error("Unsupported suite type: " + SUITES[suite].type);
  }
}

function parseErrorsFromLogs(text, regExp) {
  text = text || "";
  const globalRegexp = new RegExp(regExp, "g");
  const matches = text.match(globalRegexp) || [];
  return matches.map(m => m.match(regExp)[1]);
}

function runTests() {
  console.log("[devtools-node-test-runner] Extract suite argument");
  const suiteArg = process.argv.find(arg => arg.includes("suite="));
  const suite = suiteArg.split("=")[1];
  if (!SUITES[suite]) {
    throw new Error("Invalid suite argument to devtools-node-test-runner: " + suite);
  }

  console.log("[devtools-node-test-runner] Found test suite: " + suite);
  const testPath = path.join(__dirname, SUITES[suite].path);
  chdir(testPath);

  console.log("[devtools-node-test-runner] Run `yarn` in test folder");
  execOut("yarn");

  console.log(`TEST START | ${SUITES[suite].type} | ${suite}`);

  console.log("[devtools-node-test-runner] Run `yarn test` in test folder");
  const { out, err } = execOut("yarn", ["test"]);

  if (err) {
    console.log("[devtools-node-test-runner] Error log");
    console.log(err);
  }

  console.log("[devtools-node-test-runner] Parse errors from the test logs");
  const errors = getErrors(suite, out, err) || [];
  for (const error of errors) {
    console.log(`TEST-UNEXPECTED-FAIL | ${SUITES[suite].type} | ${suite} | ${error}`);
  }
  return errors.length === 0;
}

const success = runTests();

process.exitCode = success ? 0 : 1;
