/*
 * Description of the tests:
 *   These tests check for conformance to the CSP spec as they relate to Web Manifests.
 *
 *   In particular, the tests check that default-src and manifest-src directives are
 *   are respected by the ManifestObtainer.
 */
/*globals Cu, is, ok*/
"use strict";
const {
  ManifestObtainer
} = Cu.import("resource://gre/modules/ManifestObtainer.jsm", {});
const path = "/tests/dom/security/test/csp/";
const testFile = `${path}file_web_manifest.html`;
const remoteFile = `${path}file_web_manifest_remote.html`;
const httpsManifest = `${path}file_web_manifest_https.html`;
const server = `${path}file_testserver.sjs`;
const defaultURL = new URL(`http://example.org${server}`);
const secureURL = new URL(`https://example.com:443${server}`);
const tests = [
  // CSP block everything, so trying to load a manifest
  // will result in a policy violation.
  {
    expected: "default-src 'none' blocks fetching manifest.",
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", testFile);
      url.searchParams.append("csp", "default-src 'none'");
      return url.href;
    },
    run(topic) {
      is(topic, "csp-on-violate-policy", this.expected);
    }
  },
  // CSP allows fetching only from mochi.test:8888,
  // so trying to load a manifest from same origin
  // triggers a CSP violation.
  {
    expected: "default-src mochi.test:8888 blocks manifest fetching.",
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", testFile);
      url.searchParams.append("csp", "default-src mochi.test:8888");
      return url.href;
    },
    run(topic) {
      is(topic, "csp-on-violate-policy", this.expected);
    }
  },
  // CSP restricts fetching to 'self', so allowing the manifest
  // to load. The name of the manifest is then checked.
  {
    expected: "CSP default-src 'self' allows fetch of manifest.",
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", testFile);
      url.searchParams.append("csp", "default-src 'self'");
      return url.href;
    },
    run(manifest) {
      is(manifest.name, "loaded", this.expected);
    }
  },
  // CSP only allows fetching from mochi.test:8888 and remoteFile
  // requests a manifest from that origin, so manifest should load.
  {
    expected: "CSP default-src mochi.test:8888 allows fetching manifest.",
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", remoteFile);
      url.searchParams.append("csp", "default-src http://mochi.test:8888");
      return url.href;
    },
    run(manifest) {
      is(manifest.name, "loaded", this.expected);
    }
  },
  // default-src blocks everything, so any attempt to
  // fetch a manifest from another origin will trigger a
  // policy violation.
  {
    expected: "default-src 'none' blocks mochi.test:8888",
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", remoteFile);
      url.searchParams.append("csp", "default-src 'none'");
      return url.href;
    },
    run(topic) {
      is(topic, "csp-on-violate-policy", this.expected);
    }
  },
  // CSP allows fetching from self, so manifest should load.
  {
    expected: "CSP manifest-src allows self",
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", testFile);
      url.searchParams.append("csp", "manifest-src 'self'");
      return url.href;
    },
    run(manifest) {
      is(manifest.name, "loaded", this.expected);
    }
  },
  // CSP allows fetching from example.org, so manifest should load.
  {
    expected: "CSP manifest-src allows http://example.org",
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", testFile);
      url.searchParams.append("csp", "manifest-src http://example.org");
      return url.href;
    },
    run(manifest) {
      is(manifest.name, "loaded", this.expected);
    }
  }, {
    expected: "CSP manifest-src allows mochi.test:8888",
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", remoteFile);
      url.searchParams.append("cors", "*");
      url.searchParams.append("csp", "default-src *; manifest-src http://mochi.test:8888");
      return url.href;
    },
    run(manifest) {
      is(manifest.name, "loaded", this.expected);
    }
  },
  // CSP restricts fetching to mochi.test:8888, but the test
  // file is at example.org. Hence, a policy violation is
  // triggered.
  {
    expected: "CSP blocks manifest fetching from example.org.",
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", testFile);
      url.searchParams.append("csp", "manifest-src mochi.test:8888");
      return url.href;
    },
    run(topic) {
      is(topic, "csp-on-violate-policy", this.expected);
    }
  },
  // CSP is set to only allow manifest to be loaded from same origin,
  // but the remote file attempts to load from a different origin. Thus
  // this causes a CSP violation.
  {
    expected: "CSP manifest-src 'self' blocks cross-origin fetch.",
    get tabURL() {
      const url = new URL(defaultURL);
      url.searchParams.append("file", remoteFile);
      url.searchParams.append("csp", "manifest-src 'self'");
      return url.href;
    },
    run(topic) {
      is(topic, "csp-on-violate-policy", this.expected);
    }
  },
  // CSP allows fetching over TLS from example.org, so manifest should load.
  {
    expected: "CSP manifest-src allows example.com over TLS",
    get tabURL() {
      // secureURL loads https://example.com:443
      // and gets manifest from https://example.org:443
      const url = new URL(secureURL);
      url.searchParams.append("file", httpsManifest);
      url.searchParams.append("cors", "*");
      url.searchParams.append("csp", "manifest-src https://example.com:443");
      return url.href;
    },
    run(manifest) {
      is(manifest.name, "loaded", this.expected);
    }
  },
];

//jscs:disable
add_task(async function() {
  //jscs:enable
  const testPromises = tests.map((test) => {
    const tabOptions = {
      gBrowser,
      url: test.tabURL,
      skipAnimation: true,
    };
    return BrowserTestUtils.withNewTab(tabOptions, (browser) => testObtainingManifest(browser, test));
  });
  await Promise.all(testPromises);
});

async function testObtainingManifest(aBrowser, aTest) {
  const waitForObserver = waitForNetObserver(aBrowser, aTest);
  // Expect an exception (from promise rejection) if there a content policy
  // that is violated.
  try {
    const manifest = await ManifestObtainer.browserObtainManifest(aBrowser);
    aTest.run(manifest);
  } catch (e) {
    const wasBlocked = e.message.includes("NetworkError when attempting to fetch resource");
    ok(wasBlocked, `Expected promise rejection obtaining ${aTest.tabURL}: ${e.message}`);
  } finally {
    await waitForObserver;
  }
}

// Helper object used to observe policy violations when blocking is expected.
function waitForNetObserver(aBrowser, aTest) {
  // We don't need to wait for violation, so just resolve
  if (!aTest.expected.includes("block")){
    return Promise.resolve();
  }

  return ContentTask.spawn(aBrowser, null, () => {
    return new Promise(resolve => {
      function observe(subject, topic) {
        Services.obs.removeObserver(observe, "csp-on-violate-policy");
        resolve();
      };
      Services.obs.addObserver(observe, "csp-on-violate-policy");
    });
  }).then(() => aTest.run("csp-on-violate-policy"));
}
