//Used by JSHint:
/*global Cu, BrowserTestUtils, ok, add_task, gBrowser */
"use strict";
const { ManifestFinder } = Cu.import("resource://gre/modules/ManifestFinder.jsm", {});
const defaultURL = new URL("http://example.org/browser/dom/manifest/test/resource.sjs");
defaultURL.searchParams.set("Content-Type", "text/html; charset=utf-8");

const tests = [{
  body: `
    <link rel="manifesto" href='${defaultURL}?body={"name":"fail"}'>
    <link rel="foo bar manifest bar test" href='${defaultURL}?body={"name":"value"}'>
    <link rel="manifest" href='${defaultURL}?body={"name":"fail"}'>
  `,
  run(result) {
    ok(result, "Document has a web manifest.");
  },
}, {
  body: `
    <link rel="amanifista" href='${defaultURL}?body={"name":"fail"}'>
    <link rel="foo bar manifesto bar test" href='${defaultURL}?body={"name":"pass-1"}'>
    <link rel="manifesto" href='${defaultURL}?body={"name":"fail"}'>`,
  run(result) {
    ok(!result, "Document does not have a web manifest.");
  },
}, {
  body: `
    <link rel="manifest" href="">
    <link rel="manifest" href='${defaultURL}?body={"name":"fail"}'>`,
  run(result) {
    ok(!result, "Manifest link is has empty href.");
  },
}, {
  body: `
    <link rel="manifest">
    <link rel="manifest" href='${defaultURL}?body={"name":"fail"}'>`,
  run(result) {
    ok(!result, "Manifest link is missing.");
  },
}];

function makeTestURL({ body }) {
  const url = new URL(defaultURL);
  url.searchParams.set("body", encodeURIComponent(body));
  return url.href;
}

/**
 * Test basic API error conditions
 */
add_task(async function() {
  const expected = "Invalid types should throw a TypeError.";
  for (let invalidValue of [undefined, null, 1, {}, "test"]) {
    try {
      await ManifestFinder.contentManifestLink(invalidValue);
      ok(false, expected);
    } catch (e) {
      is(e.name, "TypeError", expected);
    }
    try {
      await ManifestFinder.browserManifestLink(invalidValue);
      ok(false, expected);
    } catch (e) {
      is(e.name, "TypeError", expected);
    }
  }
});

add_task(async function() {
  const runningTests = tests
    .map(
      test => ({
        gBrowser,
        test,
        url: makeTestURL(test),
      })
    )
    .map(
      tabOptions => BrowserTestUtils.withNewTab(tabOptions, async function(browser) {
        const result = await ManifestFinder.browserHasManifestLink(browser);
        tabOptions.test.run(result);
      })
    );
  await Promise.all(runningTests);
});
