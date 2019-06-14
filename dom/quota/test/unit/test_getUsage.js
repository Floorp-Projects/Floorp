/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const origins = [
    {
      origin: "http://example.com",
      persisted: false,
      usage: 49152
    },

    {
      origin: "http://localhost",
      persisted: false,
      usage: 147456
    },

    {
      origin: "http://www.mozilla.org",
      persisted: true,
      usage: 98304
    }
  ];

  const allOrigins = [
    {
      origin: "chrome",
      persisted: false,
      usage: 147456
    },

    {
      origin: "http://example.com",
      persisted: false,
      usage: 49152
    },

    {
      origin: "http://localhost",
      persisted: false,
      usage: 147456
    },

    {
      origin: "http://www.mozilla.org",
      persisted: true,
      usage: 98304
    }
  ];

  function verifyResult(result, expectedOrigins) {
    ok(result instanceof Array, "Got an array object");
    ok(result.length == expectedOrigins.length, "Correct number of elements");

    info("Sorting elements");

    result.sort(function(a, b) {
      let originA = a.origin
      let originB = b.origin

      if (originA < originB) {
        return -1;
      }
      if (originA > originB) {
        return 1;
      }
      return 0;
    });

    info("Verifying elements");

    for (let i = 0; i < result.length; i++) {
      let a = result[i];
      let b = expectedOrigins[i];
      ok(a.origin == b.origin, "Origin equals");
      ok(a.persisted == b.persisted, "Persisted equals");
      ok(a.usage == b.usage, "Usage equals");
    }
  }

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Getting usage");

  getUsage(grabResultAndContinueHandler, /* getAll */ true);
  let result = yield undefined;

  info("Verifying result");

  verifyResult(result, []);

  info("Installing package");

  // The profile contains IndexedDB databases placed across the repositories.
  // The file create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  // mach xpcshell-test --interactive dom/quota/test/unit/create_db.js
  installPackage("getUsage_profile");

  info("Getting usage");

  getUsage(grabResultAndContinueHandler, /* getAll */ false);
  result = yield undefined;

  info("Verifying result");

  verifyResult(result, origins);

  info("Getting usage");

  getUsage(grabResultAndContinueHandler, /* getAll */ true);
  result = yield undefined;

  info("Verifying result");

  verifyResult(result, allOrigins);

  finishTest();
}
