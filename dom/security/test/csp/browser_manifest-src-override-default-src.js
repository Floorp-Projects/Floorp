/*
 * Description of the tests:
 * Tests check that default-src can be overridden by manifest-src.
 */
/*globals Cu, is, ok*/
"use strict";
const {
  ManifestObtainer
} = Cu.import("resource://gre/modules/ManifestObtainer.jsm", {});
const path = "/tests/dom/security/test/csp/";
const testFile = `${path}file_web_manifest.html`;
const mixedContentFile = `${path}file_web_manifest_mixed_content.html`;
const server = `${path}file_testserver.sjs`;
const defaultURL = new URL(`http://example.org${server}`);
const mixedURL = new URL(`http://mochi.test:8888${server}`);
const tests = [
  // Check interaction with default-src and another origin,
  // CSP allows fetching from example.org, so manifest should load.
  {
    expected: `CSP manifest-src overrides default-src of elsewhere.com`,
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", testFile);
      url.searchParams.append("cors", "*");
      url.searchParams.append("csp", "default-src http://elsewhere.com; manifest-src http://example.org");
      return url.href;
    },
    run(manifest) {
      is(manifest.name, "loaded", this.expected);
    }
  },
  // Check interaction with default-src none,
  // CSP allows fetching manifest from example.org, so manifest should load.
  {
    expected: `CSP manifest-src overrides default-src`,
    get tabURL() {
      const url = new URL(mixedURL);
      url.searchParams.append("file", mixedContentFile);
      url.searchParams.append("cors", "http://test:80");
      url.searchParams.append("csp", "default-src 'self'; manifest-src http://test:80");
      return url.href;
    },
    run(manifest) {
      is(manifest.name, "loaded", this.expected);
    }
  },
];

//jscs:disable
add_task(function* () {
  //jscs:enable
  const testPromises = tests.map((test) => {
    const tabOptions = {
      gBrowser,
      url: test.tabURL,
      skipAnimation: true,
    };
    return BrowserTestUtils.withNewTab(tabOptions, (browser) => testObtainingManifest(browser, test));
  });
  yield Promise.all(testPromises);
});

function* testObtainingManifest(aBrowser, aTest) {
  const expectsBlocked = aTest.expected.includes("block");
  const observer = (expectsBlocked) ? createNetObserver(aTest) : null;
  // Expect an exception (from promise rejection) if there a content policy
  // that is violated.
  try {
    const manifest = yield ManifestObtainer.browserObtainManifest(aBrowser);
    aTest.run(manifest);
  } catch (e) {
    const wasBlocked = e.message.includes("NetworkError when attempting to fetch resource");
    ok(wasBlocked,`Expected promise rejection obtaining ${aTest.tabURL}: ${e.message}`);
    if (observer) {
      yield observer.untilFinished;
    }
  }
}

// Helper object used to observe policy violations. It waits 1 seconds
// for a response, and then times out causing its associated test to fail.
function createNetObserver(test) {
  let finishedTest;
  let success = false;
  const finished = new Promise((resolver) => {
    finishedTest = resolver;
  });
  const timeoutId = setTimeout(() => {
    if (!success) {
      test.run("This test timed out.");
      finishedTest();
    }
  }, 1000);
  var observer = {
    get untilFinished(){
      return finished;
    },
    observe(subject, topic) {
      SpecialPowers.removeObserver(observer, "csp-on-violate-policy");
      test.run(topic);
      finishedTest();
      clearTimeout(timeoutId);
      success = true;
    },
  };
  SpecialPowers.addObserver(observer, "csp-on-violate-policy");
  return observer;
}
