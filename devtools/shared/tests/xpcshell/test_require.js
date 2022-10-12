/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test require

// Ensure that DevtoolsLoader.require doesn't spawn multiple
// loader/modules when early cached
function testBug1091706() {
  const loader = new DevToolsLoader();
  const require = loader.require;

  const indent1 = require("resource://devtools/shared/indentation.js");
  const indent2 = require("resource://devtools/shared/indentation.js");

  Assert.ok(indent1 === indent2);
}

function testInvalidModule() {
  const loader = new DevToolsLoader();
  const require = loader.require;

  try {
    // This will result in an invalid URL with no scheme and mae loadSubScript
    // throws "Error creating URI" error
    require("foo");
    Assert.ok(false, "require should throw");
  } catch (error) {
    Assert.equal(error.message, "Module `foo` is not found at foo.js");
    Assert.ok(
      error.stack.includes("testInvalidModule"),
      "Exception's stack includes the test function"
    );
  }

  try {
    // But when using devtools prefix, the URL is going to be correct but the file
    // doesn't exists, leading to "Error opening input stream (invalid filename?)" error
    require("resource://devtools/foo.js");
    Assert.ok(false, "require should throw");
  } catch (error) {
    Assert.equal(
      error.message,
      "Module `resource://devtools/foo.js` is not found at resource://devtools/foo.js"
    );
    Assert.ok(
      error.stack.includes("testInvalidModule"),
      "Exception's stack includes the test function"
    );
  }
}

function testThrowingModule() {
  const loader = new DevToolsLoader();
  const require = loader.require;

  try {
    // Require a test module that is throwing an Error object
    require("xpcshell-test/throwing-module-1.js");
    Assert.ok(false, "require should throw");
  } catch (error) {
    Assert.equal(error.message, "my-exception");
    Assert.ok(
      error.stack.includes("testThrowingModule"),
      "Exception's stack includes the test function"
    );
    Assert.ok(
      error.stack.includes("throwingMethod"),
      "Exception's stack also includes the module function that throws"
    );
  }
  try {
    // Require a test module that is throwing a string
    require("xpcshell-test/throwing-module-2.js");
    Assert.ok(false, "require should throw");
  } catch (error) {
    Assert.equal(
      error.message,
      "Error while loading module `xpcshell-test/throwing-module-2.js` at " +
        "resource://test/throwing-module-2.js:\nmy-exception"
    );
    Assert.ok(
      error.stack.includes("testThrowingModule"),
      "Exception's stack includes the test function"
    );
    Assert.ok(
      !error.stack.includes("throwingMethod"),
      "Exception's stack also includes the module function that throws"
    );
  }
}

function run_test() {
  testBug1091706();

  testInvalidModule();

  testThrowingModule();
}
