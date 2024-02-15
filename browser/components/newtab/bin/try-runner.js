/* eslint-disable no-console */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * A small test runner/reporter for node-based tests,
 * which are run via taskcluster node(debugger).
 *
 * Forked from
 * https://searchfox.org/mozilla-central/rev/c3453c7a0427eb27d467e1582f821f402aed9850/devtools/client/debugger/bin/try-runner.js
 */

const { execFileSync } = require("child_process");
const { readFileSync, writeFileSync } = require("fs");
const path = require("path");
const { pathToFileURL } = require("url");
const chalk = require("chalk");

function logErrors(tool, errors) {
  for (const error of errors) {
    console.log(`TEST-UNEXPECTED-FAIL | ${tool} | ${error}`);
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
  console.log(`TEST-START | ${name}`);
}

function logSkip(name) {
  console.log(`TEST-SKIP | ${name}`);
}

const npmCommand = process.platform === "win32" ? "npm.cmd" : "npm";

const tests = {
  bundles() {
    logStart("bundles");

    const items = {
      "Activity Stream bundle": {
        path: path.join("data", "content", "activity-stream.bundle.js"),
      },
      "activity-stream.html": {
        path: path.join("prerendered", "activity-stream.html"),
      },
      "activity-stream-debug.html": {
        path: path.join("prerendered", "activity-stream-debug.html"),
      },
      "activity-stream-noscripts.html": {
        path: path.join("prerendered", "activity-stream-noscripts.html"),
      },
      "activity-stream-linux.css": {
        path: path.join("css", "activity-stream-linux.css"),
      },
      "activity-stream-mac.css": {
        path: path.join("css", "activity-stream-mac.css"),
      },
      "activity-stream-windows.css": {
        path: path.join("css", "activity-stream-windows.css"),
      },
      // These should get split out to their own try-runner eventually (bug 1866170).
      "about:welcome bundle": {
        path: path.join(
          "../",
          "aboutwelcome",
          "content",
          "aboutwelcome.bundle.js"
        ),
      },
      "aboutwelcome.css": {
        path: path.join("../", "aboutwelcome", "content", "aboutwelcome.css"),
      },
      // These should get split out to their own try-runner eventually (bug 1866170).
      "about:asrouter bundle": {
        path: path.join(
          "../",
          "asrouter",
          "content",
          "asrouter-admin.bundle.js"
        ),
      },
      "ASRouterAdmin.css": {
        path: path.join(
          "../",
          "asrouter",
          "content",
          "components",
          "ASRouterAdmin",
          "ASRouterAdmin.css"
        ),
      },
    };
    const errors = [];

    for (const name of Object.keys(items)) {
      const item = items[name];
      item.before = readFileSync(item.path, item.encoding || "utf8");
    }

    let newtabBundleExitCode = execOut(npmCommand, ["run", "bundle"]).exitCode;

    // Until we split out the try runner for about:welcome out into its own
    // script, we manually run its bundle script.
    let cwd = process.cwd();
    process.chdir("../aboutwelcome");
    let welcomeBundleExitCode = execOut(npmCommand, ["run", "bundle"]).exitCode;
    process.chdir(cwd);

    // Same thing for about:asrouter
    process.chdir("../asrouter");
    let asrouterBundleExitCode = execOut(npmCommand, [
      "run",
      "bundle",
    ]).exitCode;
    process.chdir(cwd);

    for (const name of Object.keys(items)) {
      const item = items[name];
      const after = readFileSync(item.path, item.encoding || "utf8");

      if (item.before !== after) {
        errors.push(`${name} out of date`);
      }
    }

    if (newtabBundleExitCode !== 0) {
      errors.push("newtab npm:bundle did not run successfully");
    }

    if (welcomeBundleExitCode !== 0) {
      errors.push("about:welcome npm:bundle did not run successfully");
    }

    if (asrouterBundleExitCode !== 0) {
      errors.push("about:asrouter npm:bundle did not run successfully");
    }

    logErrors("bundles", errors);
    return errors.length === 0;
  },

  karma() {
    logStart(`karma ${process.cwd()}`);

    const errors = [];
    const { exitCode, out } = execOut(npmCommand, [
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

    logErrors(`karma ${process.cwd()}`, errors);

    console.log("-----karma stdout below this line---");
    console.log(out);
    console.log("-----karma stdout above this line---");

    // Pass if there's no detected errors and nothing unexpected.
    return errors.length === 0 && !exitCode;
  },

  welcomekarma() {
    let cwd = process.cwd();
    process.chdir("../aboutwelcome");
    const result = this.karma();
    process.chdir(cwd);
    return result;
  },

  asrouterkarma() {
    let cwd = process.cwd();
    process.chdir("../asrouter");
    const result = this.karma();
    process.chdir(cwd);
    return result;
  },

  zipCodeCoverage() {
    logStart("zipCodeCoverage");

    const newtabCoveragePath = "logs/coverage/lcov.info";
    const welcomeCoveragePath = "../aboutwelcome/logs/coverage/lcov.info";
    const asrouterCoveragePath = "../asrouter/logs/coverage/lcov.info";

    let newtabCoverage = readFileSync(newtabCoveragePath, "utf8");
    const welcomeCoverage = readFileSync(welcomeCoveragePath, "utf8");
    const asrouterCoverage = readFileSync(asrouterCoveragePath, "utf8");

    newtabCoverage = `${newtabCoverage}${welcomeCoverage}${asrouterCoverage}`;
    writeFileSync(newtabCoveragePath, newtabCoverage, "utf8");

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
  },
};

async function main() {
  const { default: meow } = await import("meow");
  const fileUrl = pathToFileURL(__filename);
  const cli = meow(
    `
  Usage
    $ node bin/try-runner.js <tests> [options]

  Options
    -t NAME, --test NAME   Run only the specified test. If not specified,
                           all tests will be run. Argument can be passed 
                           multiple times to run multiple tests.
    --help                 Show this help message.

  Examples
    $ node bin/try-runner.js bundles karma
    $ node bin/try-runner.js -t karma -t zip
`,
    {
      description: false,
      // `pkg` is a tiny optimization. It prevents meow from looking for a package
      // that doesn't technically exist. meow searches for a package and changes
      // the process name to the package name. It resolves to the newtab
      // package.json, which would give a confusing name and be wasteful.
      pkg: {
        name: "try-runner",
        version: "1.0.0",
      },
      // `importMeta` is required by meow 10+. It was added to support ESM, but
      // meow now requires it, and no longer supports CJS style imports. But it
      // only uses import.meta.url, which can be polyfilled like this:
      importMeta: { url: fileUrl },
      flags: {
        test: {
          type: "string",
          isMultiple: true,
          alias: "t",
        },
      },
    }
  );
  const aliases = {
    bundle: "bundles",
    build: "bundles",
    coverage: "karma",
    cov: "karma",
    zip: "zipCodeCoverage",
    welcomecoverage: "welcomekarma",
    welcomecov: "welcomekarma",
    asroutercoverage: "asrouterkarma",
    asroutercov: "asrouterkarma",
  };

  const inputs = [...cli.input, ...cli.flags.test].map(input =>
    (aliases[input] || input).toLowerCase()
  );

  function shouldRunTest(name) {
    if (inputs.length) {
      return inputs.includes(name.toLowerCase());
    }
    return true;
  }

  const results = [];
  for (const name of Object.keys(tests)) {
    if (shouldRunTest(name)) {
      results.push([name, tests[name]()]);
    } else {
      logSkip(name);
    }
  }

  for (const [name, result] of results) {
    // colorize output based on result
    console.log(result ? chalk.green(`✓ ${name}`) : chalk.red(`✗ ${name}`));
  }

  const success = results.every(([, result]) => result);
  process.exitCode = success ? 0 : 1;
  console.log("CODE", process.exitCode);
}

main();
