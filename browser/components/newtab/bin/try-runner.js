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
  let exitCode = 0;
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
    exitCode = e && e.status;
  }
  return { exitCode, out: out && out.toString(), err: err && err.toString() };
}

function logStart(name) {
  console.log(`TEST START | ${name}`);
}

function checkBundles() {
  logStart("checkBundles");

  const ASbundle = path.join("data", "content", "activity-stream.bundle.js");
  const AWbundle = path.join(
    "aboutwelcome",
    "content",
    "aboutwelcome.bundle.js"
  );
  let errors = [];

  let ASbefore = readFileSync(ASbundle, "utf8");
  let AWbefore = readFileSync(AWbundle, "utf8");

  execOut("npm", ["run", "bundle"]);

  let ASafter = readFileSync(ASbundle, "utf8");
  let AWafter = readFileSync(AWbundle, "utf8");

  if (ASbefore !== ASafter) {
    errors.push("Activity Stream bundle out of date");
  }

  if (AWbefore !== AWafter) {
    errors.push("About:welcome bundle out of date");
  }

  logErrors("checkBundles", errors);
  return errors.length === 0;
}

function karma() {
  logStart("karma");

  const errors = [];
  const { exitCode, out } = execOut("npm", [
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

  // Detect mocha failures
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
  // eslint-disable-next-line guard-for-in
  for (let testArray in results.result) {
    let failedTests = Array.from(results.result[testArray]).filter(
      test => !test.success && !test.skipped
    );

    errors.push(
      ...failedTests.map(
        test => `${test.suite.join(":")} ${test.description}: ${test.log[0]}`
      )
    );
  }

  // Detect istanbul failures (coverage thresholds set in karma config)
  const coverage = out.match(/ERROR.+coverage-istanbul.+/g);
  if (coverage) {
    errors.push(...coverage.map(line => line.match(/Coverage.+/)[0]));
  }

  logErrors("karma", errors);

  console.log("-----karma stdout below this line---");
  console.log(out);
  console.log("-----karma stdout above this line---");

  // Pass if there's no detected errors and nothing unexpected.
  return errors.length === 0 && !exitCode;
}

function sasslint() {
  logStart("sasslint");
  const { exitCode, out } = execOut("npm", [
    "run",
    "--silent",
    "lint:sasslint",
    "--",
    "--format",
    "json",
  ]);

  // Successful exit and no output means sasslint passed.
  if (!exitCode && !out.length) {
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
      errorString = `${file.filePath}(${messageObj.line}, ${messageObj.column}): ${messageObj.message} (${messageObj.ruleId})`;
      errs.push(errorString);
    });
  });

  const errors = logErrors("sasslint", errs);

  // Pass if there's no detected errors and nothing unexpected.
  return errors.length === 0 && !exitCode;
}

function zipCodeCoverage() {
  logStart("zipCodeCoverage");
  const { exitCode, out } = execOut("zip", [
    "-j",
    "logs/coverage/code-coverage-grcov",
    "logs/coverage/lcov.info",
  ]);

  console.log("zipCodeCoverage log output: ", out);

  if (!exitCode) {
    return true;
  }

  return false;
}

const tests = {};
const success = [checkBundles, karma, zipCodeCoverage, sasslint].every(
  t => (tests[t.name] = t())
);
console.log(tests);

process.exitCode = success ? 0 : 1;
console.log("CODE", process.exitCode);
