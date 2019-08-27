/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// For comprehensive test cases of MozURL::Init, refer to
// netwerk/test/gtest/TestMozURL.cpp, this is just testing whether the
// validation result is correctly exposed to JS.
//
// This test doesn't have to be an async function because there are no async
// operations inside the test. However, it's currently needed due to our
// infrastructure for xpcshell tests.
async function testSteps() {
  // TODO add more appropriate test cases?
  const validURLs = ["about:blank", "https://foo.github.io"];
  const invalidURLs = ["", "xxx"];

  info("Testing valid URLs");

  for (let validURL of validURLs) {
    ok(isValidMozURL(validURL), "Should be considered valid: " + validURL);
  }

  for (let invalidURL of invalidURLs) {
    ok(
      !isValidMozURL(invalidURL),
      "Should be considered invalid: " + invalidURL
    );
  }
}
