/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global __dirname, process */

"use strict";

/**
 * This is a test runner dedicated to run DevTools node tests continuous integration
 * platforms. It will parse the logs to output errors compliant with treeherder tooling.
 *
 * See taskcluster/kinds/source-test/node.yml for the definition of the task running those
 * tests on try.
 */

const { execFileSync } = require("child_process");
const { writeFileSync } = require("fs");
const { chdir } = require("process");
const path = require("path");
const os = require("os");

const REPOSITORY_ROOT = __dirname.replace("devtools/client/bin", "");

// All Windows platforms report "win32", even for 64bit editions.
const isWin = os.platform() === "win32";

// On Windows, the ".cmd" suffix is mandatory to invoke yarn ; or executables in
// general.
const YARN_PROCESS = isWin ? "yarn.cmd" : "yarn";

// Supported node test suites for DevTools
const TEST_TYPES = {
  JEST: "jest",
  TYPESCRIPT: "typescript",
};

const SUITES = {
  aboutdebugging: {
    path: "../aboutdebugging/test/node",
    type: TEST_TYPES.JEST,
  },
  accessibility: {
    path: "../accessibility/test/node",
    type: TEST_TYPES.JEST,
  },
  application: {
    path: "../application/test/node",
    type: TEST_TYPES.JEST,
  },
  compatibility: {
    path: "../inspector/compatibility/test/node",
    type: TEST_TYPES.JEST,
  },
  debugger: {
    path: "../debugger",
    type: TEST_TYPES.JEST,
  },
  framework: {
    path: "../framework/test/node",
    type: TEST_TYPES.JEST,
  },
  netmonitor: {
    path: "../netmonitor/test/node",
    type: TEST_TYPES.JEST,
  },
  performance: {
    path: "../performance-new",
    type: TEST_TYPES.TYPESCRIPT,
  },
  shared_components: {
    path: "../shared/components/test/node",
    type: TEST_TYPES.JEST,
  },
  webconsole: {
    path: "../webconsole/test/node",
    type: TEST_TYPES.JEST,
    dependencies: ["../debugger"],
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

function getErrors(suite, out, err, testPath) {
  switch (SUITES[suite].type) {
    case TEST_TYPES.JEST:
      return getJestErrors(out, err);
    case TEST_TYPES.TYPESCRIPT:
      return getTypescriptErrors(out, err, testPath);
    default:
      throw new Error("Unsupported suite type: " + SUITES[suite].type);
  }
}

const JEST_ERROR_SUMMARY_REGEX = /\s●\s/;

function getJestErrors(out) {
  // The string out has extra content before the JSON object starts.
  const jestJsonOut = out.substring(out.indexOf("{"), out.lastIndexOf("}") + 1);
  const results = JSON.parse(jestJsonOut);

  /**
   * We don't have individual information, but a multiple line string in testResult.message,
   * which looks like
   *
   *  ● Simple function
   *
   *        expect(received).toEqual(expected) // deep equality
   *
   *        Expected: false
   *        Received: true
   *
   *          391 |       url: "test.js",
   *          392 |     });
   *        > 393 |     expect(true).toEqual(false);
   *              |                  ^
   *          394 |     expect(actual.code).toMatchSnapshot();
   *          395 |
   *          396 |     const smc = await new SourceMapConsumer(actual.map.toJSON());
   *
   *          at Object.<anonymous> (src/workers/pretty-print/tests/prettyFast.spec.js:393:18)
   *          at asyncGeneratorStep (src/workers/pretty-print/tests/prettyFast.spec.js:7:103)
   *          at _next (src/workers/pretty-print/tests/prettyFast.spec.js:9:194)
   *          at src/workers/pretty-print/tests/prettyFast.spec.js:9:364
   *          at Object.<anonymous> (src/workers/pretty-print/tests/prettyFast.spec.js:9:97)
   *
   */

  const errors = [];
  for (const testResult of results.testResults) {
    if (testResult.status != "failed") {
      continue;
    }
    let currentError;
    let errorLine;

    const lines = testResult.message.split("\n");
    lines.forEach((line, i) => {
      if (line.match(JEST_ERROR_SUMMARY_REGEX) || i == lines.length - 1) {
        // This is the name of the test, if we were gathering information from a previous
        // error, we add it to the errors
        if (currentError) {
          errors.push({
            // The file should be relative from the repository
            file: testResult.name.replace(REPOSITORY_ROOT, ""),
            line: errorLine,
            // we don't have information for the column
            column: 0,
            message: currentError.trim(),
          });
        }

        // Handle the new error
        currentError = line;
      } else {
        // We put any line that is not a test name in the error message as it may be
        // valuable for the user.
        currentError += "\n" + line;

        // The actual line of the error is marked with " > XXX |"
        const res = line.match(/> (?<line>\d+) \|/);
        if (res) {
          errorLine = parseInt(res.groups.line, 10);
        }
      }
    });
  }

  return errors;
}

function getTypescriptErrors(out, err, testPath) {
  console.log(out);
  // Typescript error lines look like:
  //   popup/panel.jsm.js(103,7): error TS2531: Object is possibly 'null'.
  // Which means:
  //   {file_path}({line},{col}): error TS{error_code}: {message}
  const tsErrorRegex =
    /(?<file>(\w|\/|\.)+)\((?<line>\d+),(?<column>\d+)\): (?<message>error TS\d+\:.*)/;
  const errors = [];
  for (const line of out.split("\n")) {
    const res = line.match(tsErrorRegex);
    if (!res) {
      continue;
    }
    // TypeScript gives us the path from the directory the command is executed in, so we
    // need to prepend the directory path.
    const fileAbsPath = testPath + res.groups.file;
    errors.push({
      // The file should be relative from the repository.
      file: fileAbsPath.replace(REPOSITORY_ROOT, ""),
      line: parseInt(res.groups.line, 10),
      column: parseInt(res.groups.column, 10),
      message: res.groups.message.trim(),
    });
  }
  return errors;
}

function runTests() {
  console.log("[devtools-node-test-runner] Extract suite argument");
  const suiteArg = process.argv.find(arg => arg.includes("suite="));
  const suite = suiteArg.split("=")[1];
  if (suite !== "all" && !SUITES[suite]) {
    throw new Error(
      "Invalid suite argument to devtools-node-test-runner: " + suite
    );
  }

  console.log("[devtools-node-test-runner] Check `yarn` is available");
  try {
    // This will throw if yarn is unavailable
    execFileSync(YARN_PROCESS, ["--version"]);
  } catch (e) {
    console.log(
      "[devtools-node-test-runner] ERROR: `yarn` is not installed. " +
        "See https://yarnpkg.com/docs/install/ "
    );
    return false;
  }

  const artifactArg = process.argv.find(arg => arg.includes("artifact="));
  const artifactFilePath = artifactArg && artifactArg.split("=")[1];
  const artifactErrors = {};

  const failedSuites = [];
  const suites = suite == "all" ? SUITES : { [suite]: SUITES[suite] };
  for (const [suiteName, suiteData] of Object.entries(suites)) {
    console.log("[devtools-node-test-runner] Running suite: " + suiteName);

    if (suiteData.dependencies) {
      console.log(
        "[devtools-node-test-runner] Running `yarn` for dependencies"
      );
      for (const dep of suiteData.dependencies) {
        const depPath = path.join(__dirname, dep);
        chdir(depPath);

        console.log("[devtools-node-test-runner] Run `yarn` in " + depPath);
        execOut(YARN_PROCESS);
      }
    }

    const testPath = path.join(__dirname, suiteData.path);
    chdir(testPath);

    console.log("[devtools-node-test-runner] Run `yarn` in test folder");
    execOut(YARN_PROCESS);

    console.log(`TEST START | ${suiteData.type} | ${suiteName}`);

    console.log("[devtools-node-test-runner] Run `yarn test` in test folder");
    const { out, err } = execOut(YARN_PROCESS, ["test-ci"]);

    if (err) {
      console.log("[devtools-node-test-runner] Error log");
      console.log(err);
    }

    console.log("[devtools-node-test-runner] Parse errors from the test logs");
    const errors = getErrors(suiteName, out, err, testPath) || [];
    if (errors.length) {
      failedSuites.push(suiteName);
    }
    for (const error of errors) {
      if (!artifactErrors[error.file]) {
        artifactErrors[error.file] = [];
      }
      artifactErrors[error.file].push({
        path: error.file,
        line: error.line,
        column: error.column,
        level: "error",
        message: error.message,
        analyzer: suiteName,
      });

      console.log(
        `TEST-UNEXPECTED-FAIL | ${suiteData.type} | ${suiteName} | ${error.file}:${error.line}: ${error.message}`
      );
    }
  }

  if (artifactFilePath) {
    console.log(
      `[devtools-node-test-runner] Writing artifact to ${artifactFilePath}`
    );
    writeFileSync(artifactFilePath, JSON.stringify(artifactErrors, null, 2));
  }

  const success = failedSuites.length === 0;
  if (success) {
    console.log(
      `[devtools-node-test-runner] Test suites [${Object.keys(suites).join(
        ", "
      )}] succeeded`
    );
  } else {
    console.log(
      `[devtools-node-test-runner] Test suites [${failedSuites.join(
        ", "
      )}] failed`
    );
    console.log(
      "[devtools-node-test-runner] You can find documentation about the " +
        "devtools node tests at https://firefox-source-docs.mozilla.org/devtools/tests/node-tests.html"
    );
  }
  return success;
}

process.exitCode = runTests() ? 0 : 1;
