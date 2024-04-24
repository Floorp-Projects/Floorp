/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for console.createInstance usage in workers.
 *
 * Also tests that the use of `maxLogLevelPref` logs an error, and log levels
 * fallback to the `maxLogLevel` option.
 */

"use strict";

let { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

add_task(async function () {
  let endConsoleListening = TestUtils.listenForConsoleMessages();
  let workerCompleteDeferred = Promise.withResolvers();

  const worker = new ChromeWorker("resource://test/worker.mjs");
  worker.onmessage = workerCompleteDeferred.resolve;
  worker.postMessage({});

  await workerCompleteDeferred.promise;

  let messages = await endConsoleListening();

  // Should log that we can't use `maxLogLevelPref`, and the warning message.
  // The info message should not be logged, as that's lower than the specified
  // `maxLogLevel` in the worker.
  Assert.equal(messages.length, 2, "Should have received two messages");

  // First message should be the error that `maxLogLevelPref` cannot be used.
  Assert.equal(messages[0].level, "error", "Should be an error message");
  Assert.equal(
    messages[0].prefix,
    "testPrefix",
    "Should have the correct prefix"
  );
  Assert.equal(
    messages[0].arguments[0],
    "Console.maxLogLevelPref is not supported within workers!",
    "Should have the correct message text"
  );

  // Second message should be the warning.
  Assert.equal(messages[1].level, "warn", "Should be a warning message");
  Assert.equal(
    messages[1].prefix,
    "testPrefix",
    "Should have the correct prefix"
  );
  Assert.equal(
    messages[1].arguments[0],
    "Test Warn",
    "Should have the correct message text"
  );
});
