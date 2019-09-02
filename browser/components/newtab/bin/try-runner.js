/* eslint-disable no-console */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * A small test runner/reporter for node-based tests,
 * which are run via taskcluster node(debugger).
 *
 * Forked from
 * https://searchfox.org/mozilla-central/source/devtools/client/debugger/bin/try-runner.js
 */

const { execFileSync } = require("child_process");
const { readFileSync } = require("fs");
const path = require("path");

function logErrors(tool, errors) {
  for (const error of errors) {
    console.log(`TEST-UNEXPECTED-FAIL ${tool} | ${error}`);
  }
  return errors;
}

function execOut(...args) {
  let out;
  let err;

  try {
    out = execFileSync(...args, {
      silent: false,
    });
  } catch (e) {
    // For debugging on (eg) try server...
    //
    // if (e) {
    //   logErrors("execOut", ["execFileSync returned exception: ", e]);
    // }

    out = e && e.stdout;
    err = e && e.stderr;
  }
  return { out: out && out.toString(), err: err && err.toString() };
}

function logStart(name) {
  console.log(`TEST START | ${name}`);
}

function karma() {
  logStart("karma");

  const { out } = execOut("npm", [
    "run",
    "testmc:unit",
    // , "--", "--log-level", "--verbose",
    // to debug the karma integration, uncomment the above line
  ]);

  // karma spits everything to stdout, not stderr, so if nothing came back on
  // stdout, give up now.
  if (!out) {
    return false;
  }

  let jsonContent;
  try {
    // Note that this will be overwritten at each run, but that shouldn't
    // matter.
    jsonContent = readFileSync(path.join("logs", "karma-run-results.json"));
  } catch (ex) {
    console.error("exception reading karma-run-results.json: ", ex);
    return false;
  }

  const results = JSON.parse(jsonContent);

  const failed = results.summary.failed === 0;

  let errors = [];
  // eslint-disable-next-line guard-for-in
  for (let testArray in results.result) {
    let failedTests = Array.from(results.result[testArray]).filter(
      test => !test.success && !test.skipped
    );

    let errs = failedTests.map(test => {
      return `${test.suite.join(":")} ${test.description}: ${test.log[0]}`;
    });

    errors = errors.concat(errs);
  }

  logErrors("karma", errors);
  return failed;
}

function sasslint() {
  logStart("sasslint");
  const { out } = execOut("npm", [
    "run",
    "--silent",
    "lint:sasslint",
    "--",
    "--format",
    "json",
  ]);

  if (!out.length) {
    return true;
  }

  let fileObjects = JSON.parse(out);
  let filesWithIssues = fileObjects.filter(
    file => file.warningCount || file.errorCount
  );

  let errs = [];
  let errorString;
  filesWithIssues.forEach(file => {
    file.messages.forEach(messageObj => {
      errorString = `${file.filePath}(${messageObj.line}, ${
        messageObj.column
      }): ${messageObj.message} (${messageObj.ruleId})`;
      errs.push(errorString);
    });
  });

  const errors = logErrors("sasslint", errs);
  return errors.length === 0;
}

const karmaPassed = karma();
const sasslintPassed = sasslint();

const success = karmaPassed && sasslintPassed;

console.log({
  karmaPassed,
  sasslintPassed,
});

process.exitCode = success ? 0 : 1;
console.log("CODE", process.exitCode);
