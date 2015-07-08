//Used by JSHint:
/*global Cu, BrowserTestUtils, is, ok, add_task, gBrowser, ManifestFinder */
'use strict';
Cu.import('resource://gre/modules/ManifestFinder.jsm', this);  // jshint ignore:line

const finder = new ManifestFinder();
const defaultURL =
  'http://example.org/tests/dom/manifest/test/resource.sjs';
const tests = [{
  expected: 'Document has a web manifest.',
  get tabURL() {
    let query = [
      `body=<h1>${this.expected}</h1>`,
      'Content-Type=text/html; charset=utf-8',
    ];
    const URL = `${defaultURL}?${query.join('&')}`;
    return URL;
  },
  run(result) {
    is(result, true, this.expected);
  },
  testData: `
      <link rel="manifesto" href='${defaultURL}?body={"name":"fail"}'>
      <link rel="foo bar manifest bar test" href='${defaultURL}?body={"name":"value"}'>
      <link rel="manifest" href='${defaultURL}?body={"name":"fail"}'>`
}, {
  expected: 'Document does not have a web manifest.',
  get tabURL() {
    let query = [
      `body=<h1>${this.expected}</h1>`,
      'Content-Type=text/html; charset=utf-8',
    ];
    const URL = `${defaultURL}?${query.join('&')}`;
    return URL;
  },
  run(result) {
    is(result, false, this.expected);
  },
  testData: `
      <link rel="amanifista" href='${defaultURL}?body={"name":"fail"}'>
      <link rel="foo bar manifesto bar test" href='${defaultURL}?body={"name":"pass-1"}'>
      <link rel="manifesto" href='${defaultURL}?body={"name":"fail"}'>`
}, {
  expected: 'Manifest link is has empty href.',
  get tabURL() {
    let query = [
      `body=<h1>${this.expected}</h1>`,
      'Content-Type=text/html; charset=utf-8',
    ];
    const URL = `${defaultURL}?${query.join('&')}`;
    return URL;
  },
  run(result) {
    is(result, false, this.expected);
  },
  testData: `
  <link rel="manifest" href="">
  <link rel="manifest" href='${defaultURL}?body={"name":"fail"}'>`
}, {
  expected: 'Manifest link is missing.',
  get tabURL() {
    let query = [
      `body=<h1>${this.expected}</h1>`,
      'Content-Type=text/html; charset=utf-8',
    ];
    const URL = `${defaultURL}?${query.join('&')}`;
    return URL;
  },
  run(result) {
    is(result, false, this.expected);
  },
  testData: `
    <link rel="manifest">
    <link rel="manifest" href='${defaultURL}?body={"name":"fail"}'>`
}];

/**
 * Test basic API error conditions
 */
add_task(function* () {
  let expected = 'Invalid types should throw a TypeError.';
  for (let invalidValue of [undefined, null, 1, {}, 'test']) {
    try {
      yield finder.hasManifestLink(invalidValue);
      ok(false, expected);
    } catch (e) {
      is(e.name, 'TypeError', expected);
    }
  }
});

add_task(function* () {
  for (let test of tests) {
    let tabOptions = {
      gBrowser: gBrowser,
      url: test.tabURL,
    };
    yield BrowserTestUtils.withNewTab(
      tabOptions,
      browser => testHasManifest(browser, test)
    );
  }

  function* testHasManifest(aBrowser, aTest) {
    aBrowser.contentWindowAsCPOW.document.head.innerHTML = aTest.testData;
    const result = yield finder.hasManifestLink(aBrowser);
    aTest.run(result);
  }
});
