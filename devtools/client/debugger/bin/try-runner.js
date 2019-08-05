/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * A small test runner/reporter for node-based tests,
 * which are run via taskcluster node(debugger).
 */

const { execFileSync } = require("child_process");
const { chdir } = require("process");
const path = require("path");
const flow = require("flow-bin");

const dbgPath = path.join(__dirname, "..");

function execOut(...args) {
  let out;
  let err;
  try {
    out = execFileSync(...args, { silent: true });
  } catch (e) {
    out = e.stdout;
    err = e.stderr;
  }
  return { out: out.toString(), err: err && err.toString() };
}

function logErrors(tool, errors) {
  for (const error of errors) {
    console.log(`TEST-UNEXPECTED-FAIL ${tool} | ${error}`);
  }
  return errors;
}

function logStart(name) {
  console.log(`TEST START | ${name}`);
}

function runFlowJson() {
  const { out } = execOut(flow, ["check", "--json"]);
  const results = JSON.parse(out);

  if (!results.passed) {
    for (const error of results.errors) {
      for (const message of error.message) {
        console.log(`TEST-UNEXPECTED-FAIL flow | ${message.descr}`);
      }
    }
  }

  return results.passed;
}

function runFlow() {
  logStart("Flow");
  const { out } = execOut(flow, ["check"]);
  console.log(out);
  return runFlowJson();
}

function eslint() {
  logStart("Eslint");
  const { out } = execOut("yarn", ["lint:js"]);
  console.log(out);
  const errors = logErrors("eslint", out.match(/ {2}error {2}(.*)/g) || []);

  return errors.length == 0;
}

function jest() {
  logStart("Jest");
  const { out } = execOut("yarn", ["test-ci"]);
  // Remove the non-JSON logs mixed with the JSON output by yarn.
  const jsonOut = out.substring(out.indexOf("{"), out.lastIndexOf("}") + 1);
  const results = JSON.parse(jsonOut);

  const failed = results.numFailedTests == 0;

  // The individual failing tests are in jammed into the same message string :/
  const errors = [].concat(
    ...results.testResults.map(r =>
      r.message.split("\n").filter(l => l.includes("●"))
    )
  );

  logErrors("jest", errors);
  return failed;
}

function stylelint() {
  logStart("Stylelint");
  const { out } = execOut("yarn", ["lint:css"]);
  console.log(out);
  const errors = logErrors("stylelint", out.match(/ {2}✖(.*)/g) || []);

  return errors.length == 0;
}

function jsxAccessibility() {
  logStart("Eslint (JSX Accessibility)");

  const { out } = execOut("yarn", ["lint:jsx-a11y"]);
  console.log(out);
  const errors = logErrors(
    "eslint (jsx accessibility)",
    out.match(/ {2}error {2}(.*)/g) || []
  );
  return errors.length == 0;
}

function lintMd() {
  logStart("Remark");

  const { err } = execOut("yarn", ["lint:md"]);
  const errors = logErrors("remark", (err || "").match(/warning(.+)/g) || []);
  return errors.length == 0;
}

chdir(dbgPath);
const flowPassed = runFlow();
const eslintPassed = eslint();
const jestPassed = jest();
const styleLintPassed = stylelint();
const jsxAccessibilityPassed = jsxAccessibility();
const remarkPassed = lintMd();

const success =
  flowPassed &&
  eslintPassed &&
  jestPassed &&
  styleLintPassed &&
  jsxAccessibilityPassed &&
  remarkPassed;

console.log({
  flowPassed,
  eslintPassed,
  jestPassed,
  styleLintPassed,
  jsxAccessibilityPassed,
  remarkPassed,
});

if (!success) {
  console.log(
    "[debugger-node-test-runner] You can find documentation about the " +
      "debugger node tests at https://docs.firefox-dev.tools/tests/node-tests.html"
  );
}

process.exitCode = success ? 0 : 1;
console.log("CODE", process.exitCode);
