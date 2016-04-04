/*
 * Description of the tests:
 *   These tests check for conformance to the CSP spec as they relate to Web Manifests.
 *
 *   In particular, the tests check that default-src and manifest-src directives are
 *   are respected by the ManifestObtainer.
 */
/*globals Cu, is, ok*/
'use strict';
requestLongerTimeout(10); // e10s tests take time.
const {
  ManifestObtainer
} = Cu.import('resource://gre/modules/ManifestObtainer.jsm', {});
const path = '/tests/dom/security/test/csp/';
const testFile = `file=${path}file_web_manifest.html`;
const remoteFile = `file=${path}file_web_manifest_remote.html`;
const httpsManifest = `file=${path}file_web_manifest_https.html`;
const server = 'file_testserver.sjs';
const defaultURL = `http://example.org${path}${server}`;
const secureURL = `https://example.com:443${path}${server}`;
const tests = [
  // CSP block everything, so trying to load a manifest
  // will result in a policy violation.
  {
    expected: `default-src 'none' blocks fetching manifest.`,
    get tabURL() {
      let queryParts = [
        `csp=default-src 'none'`,
        testFile
      ];
      return `${defaultURL}?${queryParts.join('&')}`;
    },
    run(topic) {
      is(topic, 'csp-on-violate-policy', this.expected);
    }
  },
  // CSP allows fetching only from mochi.test:8888,
  // so trying to load a manifest from same origin
  // triggers a CSP violation.
  {
    expected: `default-src mochi.test:8888 blocks manifest fetching.`,
    get tabURL() {
      let queryParts = [
        `csp=default-src mochi.test:8888`,
        testFile
      ];
      return `${defaultURL}?${queryParts.join('&')}`;
    },
    run(topic) {
      is(topic, 'csp-on-violate-policy', this.expected);
    }
  },
  // CSP restricts fetching to 'self', so allowing the manifest
  // to load. The name of the manifest is then checked.
  {
    expected: `CSP default-src 'self' allows fetch of manifest.`,
    get tabURL() {
      let queryParts = [
        `csp=default-src 'self'`,
        testFile
      ];
      return `${defaultURL}?${queryParts.join('&')}`;
    },
    run(manifest) {
      is(manifest.name, 'loaded', this.expected);
    }
  },
  // CSP only allows fetching from mochi.test:8888 and remoteFile
  // requests a manifest from that origin, so manifest should load.
  {
    expected: 'CSP default-src mochi.test:8888 allows fetching manifest.',
    get tabURL() {
      let queryParts = [
        `csp=default-src http://mochi.test:8888`,
        remoteFile
      ];
      return `${defaultURL}?${queryParts.join('&')}`;
    },
    run(manifest) {
      is(manifest.name, 'loaded', this.expected);
    }
  },
  // default-src blocks everything, so any attempt to
  // fetch a manifest from another origin will trigger a
  // policy violation.
  {
    expected: `default-src 'none' blocks mochi.test:8888`,
    get tabURL() {
      let queryParts = [
        `csp=default-src 'none'`,
        remoteFile
      ];
      return `${defaultURL}?${queryParts.join('&')}`;
    },
    run(topic) {
      is(topic, 'csp-on-violate-policy', this.expected);
    }
  },
  // CSP allows fetching from self, so manifest should load.
  {
    expected: `CSP manifest-src allows self`,
    get tabURL() {
      let queryParts = [
        `csp=manifest-src 'self'`,
        testFile
      ];
      return `${defaultURL}?${queryParts.join('&')}`;
    },
    run(manifest) {
      is(manifest.name, 'loaded', this.expected);
    }
  },
  // CSP allows fetching from example.org, so manifest should load.
  {
    expected: `CSP manifest-src allows http://example.org`,
    get tabURL() {
      let queryParts = [
        `csp=manifest-src http://example.org`,
        testFile
      ];
      return `${defaultURL}?${queryParts.join('&')}`;
    },
    run(manifest) {
      is(manifest.name, 'loaded', this.expected);
    }
  },
  {
    expected: `CSP manifest-src allows mochi.test:8888`,
    get tabURL() {
      let queryParts = [
        `cors=*`,
        `csp=default-src *; manifest-src http://mochi.test:8888`,
        remoteFile
      ];
      return `${defaultURL}?${queryParts.join('&')}`;
    },
    run(manifest) {
      is(manifest.name, 'loaded', this.expected);
    }
  },
  // CSP restricts fetching to mochi.test:8888, but the test
  // file is at example.org. Hence, a policy violation is
  // triggered.
  {
    expected: `CSP blocks manifest fetching from example.org.`,
    get tabURL() {
      let queryParts = [
        `csp=manifest-src mochi.test:8888`,
        testFile
      ];
      return `${defaultURL}?${queryParts.join('&')}`;
    },
    run(topic) {
      is(topic, 'csp-on-violate-policy', this.expected);
    }
  },
  // CSP is set to only allow manifest to be loaded from same origin,
  // but the remote file attempts to load from a different origin. Thus
  // this causes a CSP violation.
  {
    expected: `CSP manifest-src 'self' blocks cross-origin fetch.`,
    get tabURL() {
      let queryParts = [
        `csp=manifest-src 'self'`,
        remoteFile
      ];
      return `${defaultURL}?${queryParts.join('&')}`;
    },
    run(topic) {
      is(topic, 'csp-on-violate-policy', this.expected);
    }
  },
  // CSP allows fetching over TLS from example.org, so manifest should load.
  {
    expected: `CSP manifest-src allows example.com over TLS`,
    get tabURL() {
      let queryParts = [
        'cors=*',
        'csp=manifest-src https://example.com:443',
        httpsManifest
      ];
      // secureURL loads https://example.com:443
      // and gets manifest from https://example.org:443
      return `${secureURL}?${queryParts.join('&')}`;
    },
    run(manifest) {
      is(manifest.name, 'loaded', this.expected);
    }
  },
];
//jscs:disable
add_task(function* () {
  //jscs:enable
  var testPromises = tests.map(
      (test) => ([test, {gBrowser, url: test.tabURL, skipAnimation: true}])
    ).map(
      ([test, tabOptions]) => BrowserTestUtils.withNewTab(tabOptions, (browser) => testObtainingManifest(browser, test))
    );
  yield Promise.all(testPromises);
});

function* testObtainingManifest(aBrowser, aTest) {
  const expectsBlocked = aTest.expected.includes('block');
  const observer = (expectsBlocked) ? createNetObserver(aTest) : null;
  // Expect an exception (from promise rejection) if there a content policy
  // that is violated.
  try {
    const manifest = yield ManifestObtainer.browserObtainManifest(aBrowser);
    aTest.run(manifest);
  } catch (e) {
    const wasBlocked = e.message.includes('blocked the loading of a resource');
    ok(wasBlocked,`Expected promise rejection obtaining ${aTest.tabURL}: ${e.message}`);
    if (observer) {
      yield observer.untilFinished;
      return;
    }
    throw e;
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
      test.run('This test timed out.');
      finishedTest();
    }
  }, 1000);
  var observer = {
    get untilFinished(){
      return finished;
    },
    observe(subject, topic) {
      SpecialPowers.removeObserver(observer, 'csp-on-violate-policy');
      test.run(topic);
      finishedTest();
      clearTimeout(timeoutId);
      success = true;
    },
  };
  SpecialPowers.addObserver(observer, 'csp-on-violate-policy', false);
  return observer;
}
