/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Use initOrigin to test the operation of the origin parser on a list of URLs
// we should support. If the origin doesn't parse, then initOrigin will throw an
// exception (and potentially MOZ_ASSERT under debug builds). Handling of
// obsolete or invalid origins is handled in other test files.
async function testSteps() {
  const testingURLs = [
    // General
    "https://example.com",
    // About
    "about:home",
    "about:reader",
    // IPv6
    "https://[::]",
    "https://[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]",
    "http://[2010:836B:4179::836B:4179]:80",
    "https://[::FFFF:129.144.52.38]",
  ];

  for (let testingURL of testingURLs) {
    info("Testing " + testingURL);
    try {
      let request = initOrigin(getPrincipal(testingURL), "default");
      await requestFinished(request);

      ok(true, "Should not have thrown");
    } catch (ex) {
      ok(false, "Should not have thrown");
    }
  }
}
