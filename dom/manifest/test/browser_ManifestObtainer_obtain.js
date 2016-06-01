//Used by JSHint:
/*global is, Cu, BrowserTestUtils, add_task, gBrowser, makeTestURL*/
'use strict';
const { ManifestObtainer } = Cu.import('resource://gre/modules/ManifestObtainer.jsm', {});
const remoteURL = 'http://mochi.test:8888/browser/dom/manifest/test/resource.sjs';
const defaultURL = new URL('http://example.org/browser/dom/manifest/test/resource.sjs');
defaultURL.searchParams.set('Content-Type', 'text/html; charset=utf-8');

const tests = [
  // Fetch tests.
  {
    body: `
      <link rel="manifesto" href='resource.sjs?body={"name":"fail"}'>
      <link rel="foo bar manifest bar test" href='resource.sjs?body={"name":"pass-1"}'>
      <link rel="manifest" href='resource.sjs?body={"name":"fail"}'>`,
    run(manifest) {
      is(manifest.name, 'pass-1', 'Manifest is first `link` where @rel contains token manifest.');
    }
  }, {
    body: `
      <link rel="foo bar manifest bar test" href='resource.sjs?body={"name":"pass-2"}'>
      <link rel="manifest" href='resource.sjs?body={"name":"fail"}'>
      <link rel="manifest foo bar test" href='resource.sjs?body={"name":"fail"}'>`,
    run(manifest) {
      is(manifest.name, 'pass-2', 'Manifest is first `link` where @rel contains token manifest.');
    },
  }, {
    body: `<link rel="manifest" href='${remoteURL}?body={"name":"pass-3"}'>`,
    run(err) {
      is(err.name, 'TypeError', 'By default, manifest cannot load cross-origin.');
    },
  },
  // CORS Tests.
  {
    get body() {
      const body = 'body={"name": "pass-4"}';
      const CORS =
        `Access-Control-Allow-Origin=${defaultURL.origin}`;
      const link =
        `<link
        crossorigin=anonymous
        rel="manifest"
        href='${remoteURL}?${body}&${CORS}'>`;
      return link;
    },
    run(manifest) {
      is(manifest.name, 'pass-4', 'CORS enabled, manifest must be fetched.');
    },
  }, {
    get body() {
      const body = 'body={"name": "fail"}';
      const CORS = 'Access-Control-Allow-Origin=http://not-here';
      const link =
        `<link
        crossorigin
        rel="manifest"
        href='${remoteURL}?${body}&${CORS}'>`;
      return link;
    },
    run(err) {
      is(err.name, 'TypeError', 'Fetch blocked by CORS - origin does not match.');
    },
  }, {
    body: `<link rel="manifest" href='about:whatever'>`,
    run(err) {
      is(err.name, 'TypeError', 'Trying to load from about:whatever is TypeError.');
    },
  }, {
    body: `<link rel="manifest" href='file://manifest'>`,
    run(err) {
      is(err.name, 'TypeError', 'Trying to load from file://whatever is a TypeError.');
    },
  },
  //URL parsing tests
  {
    body: `<link rel="manifest" href='http://[12.1212.21.21.12.21.12]'>`,
    run(err) {
      is(err.name, 'TypeError', 'Trying to load invalid URL is a TypeError.');
    },
  },
];

function makeTestURL({ body }) {
  const url = new URL(defaultURL);
  url.searchParams.set("body", encodeURIComponent(body));
  return url.href;
}

add_task(function*() {
  const promises = tests
    .map(test => ({
      gBrowser,
      testRunner: testObtainingManifest(test),
      url: makeTestURL(test)
    }))
    .reduce((collector, tabOpts) => {
      const promise = BrowserTestUtils.withNewTab(tabOpts, tabOpts.testRunner);
      collector.push(promise);
      return collector;
    }, []);

  const results = yield Promise.all(promises);

  function testObtainingManifest(aTest) {
    return function*(aBrowser) {
      try {
        const manifest = yield ManifestObtainer.browserObtainManifest(aBrowser);
        aTest.run(manifest);
      } catch (e) {
        aTest.run(e);
      }
    };
  }
});

/*
 * e10s race condition tests
 * Open a bunch of tabs and load manifests
 * in each tab. They should all return pass.
 */
add_task(function*() {
  const defaultPath = '/browser/dom/manifest/test/manifestLoader.html';
  const tabURLs = [
    `http://example.com:80${defaultPath}`,
    `http://example.org:80${defaultPath}`,
    `http://example.org:8000${defaultPath}`,
    `http://mochi.test:8888${defaultPath}`,
    `http://sub1.test1.example.com:80${defaultPath}`,
    `http://sub1.test1.example.org:80${defaultPath}`,
    `http://sub1.test1.example.org:8000${defaultPath}`,
    `http://sub1.test1.mochi.test:8888${defaultPath}`,
    `http://sub1.test2.example.com:80${defaultPath}`,
    `http://sub1.test2.example.org:80${defaultPath}`,
    `http://sub1.test2.example.org:8000${defaultPath}`,
    `http://sub2.test1.example.com:80${defaultPath}`,
    `http://sub2.test1.example.org:80${defaultPath}`,
    `http://sub2.test1.example.org:8000${defaultPath}`,
    `http://sub2.test2.example.com:80${defaultPath}`,
    `http://sub2.test2.example.org:80${defaultPath}`,
    `http://sub2.test2.example.org:8000${defaultPath}`,
    `http://sub2.xn--lt-uia.mochi.test:8888${defaultPath}`,
    `http://test1.example.com:80${defaultPath}`,
    `http://test1.example.org:80${defaultPath}`,
    `http://test1.example.org:8000${defaultPath}`,
    `http://test1.mochi.test:8888${defaultPath}`,
    `http://test2.example.com:80${defaultPath}`,
    `http://test2.example.org:80${defaultPath}`,
    `http://test2.example.org:8000${defaultPath}`,
    `http://test2.mochi.test:8888${defaultPath}`,
    `http://test:80${defaultPath}`,
    `http://www.example.com:80${defaultPath}`,
  ];
  // Open tabs an collect corresponding browsers
  let browsers = [
    for (url of tabURLs) gBrowser.addTab(url).linkedBrowser
  ];
  // Once all the pages have loaded, run a bunch of tests in "parallel".
  yield Promise.all((
    for (browser of browsers) BrowserTestUtils.browserLoaded(browser)
  ));
  // Flood random browsers with requests. Once promises settle, check that
  // responses all pass.
  const results = yield Promise.all((
    for (browser of randBrowsers(browsers, 100)) ManifestObtainer.browserObtainManifest(browser)
  ));
  const pass = results.every(manifest => manifest.name === 'pass');
  ok(pass, 'Expect every manifest to have name equal to `pass`.');
  //cleanup
  browsers
    .map(browser => gBrowser.getTabForBrowser(browser))
    .forEach(tab => gBrowser.removeTab(tab));

  //Helper generator, spits out random browsers
  function* randBrowsers(aBrowsers, aMax) {
    for (let i = 0; i < aMax; i++) {
      const randNum = Math.round(Math.random() * (aBrowsers.length - 1));
      yield aBrowsers[randNum];
    }
  }
});
