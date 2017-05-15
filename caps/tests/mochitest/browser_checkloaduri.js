"use strict";

let ssm = Services.scriptSecurityManager;
// This will show a directory listing, but we never actually load these so that's OK.
const kDummyPage = getRootDirectory(gTestPath);

const kAboutPagesRegistered = Promise.all([
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "test-chrome-privs", kDummyPage,
    Ci.nsIAboutModule.ALLOW_SCRIPT),
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "test-chrome-privs2", kDummyPage,
    Ci.nsIAboutModule.ALLOW_SCRIPT),
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "test-unknown-linkable", kDummyPage,
    Ci.nsIAboutModule.MAKE_LINKABLE | Ci.nsIAboutModule.ALLOW_SCRIPT),
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "test-unknown-linkable2", kDummyPage,
    Ci.nsIAboutModule.MAKE_LINKABLE | Ci.nsIAboutModule.ALLOW_SCRIPT),
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "test-unknown-unlinkable", kDummyPage,
    Ci.nsIAboutModule.ALLOW_SCRIPT),
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "test-unknown-unlinkable2", kDummyPage,
    Ci.nsIAboutModule.ALLOW_SCRIPT),
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "test-content-unlinkable", kDummyPage,
    Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT | Ci.nsIAboutModule.ALLOW_SCRIPT),
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "test-content-unlinkable2", kDummyPage,
    Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT | Ci.nsIAboutModule.ALLOW_SCRIPT),
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "test-content-linkable", kDummyPage,
    Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT | Ci.nsIAboutModule.MAKE_LINKABLE |
    Ci.nsIAboutModule.ALLOW_SCRIPT),
  BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "test-content-linkable2", kDummyPage,
    Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT | Ci.nsIAboutModule.MAKE_LINKABLE |
    Ci.nsIAboutModule.ALLOW_SCRIPT),
]);

const URLs = new Map([
  ["http://www.example.com", [
  // For each of these entries, the booleans represent whether the parent URI can:
  // - load them
  // - load them without principal inheritance
  // - whether the URI can be created at all (some protocol handlers will
  //   refuse to create certain variants)
    ["http://www.example2.com", true, true, true],
    ["feed:http://www.example2.com", false, false, true],
    ["https://www.example2.com", true, true, true],
    ["chrome://foo/content/bar.xul", false, false, true],
    ["feed:chrome://foo/content/bar.xul", false, false, false],
    ["view-source:http://www.example2.com", false, false, true],
    ["view-source:https://www.example2.com", false, false, true],
    ["view-source:feed:http://www.example2.com", false, false, true],
    ["feed:view-source:http://www.example2.com", false, false, false],
    ["data:text/html,Hi", true, false, true],
    ["view-source:data:text/html,Hi", false, false, true],
    ["javascript:alert('hi')", true, false, true],
    ["moz://a", false, false, true],
    ["about:test-chrome-privs", false, false, true],
    ["about:test-unknown-unlinkable", false, false, true],
    ["about:test-content-unlinkable", false, false, true],
    ["about:test-content-linkable", true, true, true],
    // Because this page doesn't have SAFE_FOR_UNTRUSTED, the web can't link to it:
    ["about:test-unknown-linkable", false, false, true],
  ]],
  ["feed:http://www.example.com", [
    ["http://www.example2.com", true, true, true],
    ["feed:http://www.example2.com", true, true, true],
    ["https://www.example2.com", true, true, true],
    ["feed:https://www.example2.com", true, true, true],
    ["chrome://foo/content/bar.xul", false, false, true],
    ["feed:chrome://foo/content/bar.xul", false, false, false],
    ["view-source:http://www.example2.com", false, false, true],
    ["view-source:https://www.example2.com", false, false, true],
    ["view-source:feed:http://www.example2.com", false, false, true],
    ["feed:view-source:http://www.example2.com", false, false, false],
    ["data:text/html,Hi", true, false, true],
    ["view-source:data:text/html,Hi", false, false, true],
    ["javascript:alert('hi')", true, false, true],
    ["moz://a", false, false, true],
    ["about:test-chrome-privs", false, false, true],
    ["about:test-unknown-unlinkable", false, false, true],
    ["about:test-content-unlinkable", false, false, true],
    ["about:test-content-linkable", true, true, true],
    // Because this page doesn't have SAFE_FOR_UNTRUSTED, the web can't link to it:
    ["about:test-unknown-linkable", false, false, true],
  ]],
  ["view-source:http://www.example.com", [
    ["http://www.example2.com", true, true, true],
    ["feed:http://www.example2.com", false, false, true],
    ["https://www.example2.com", true, true, true],
    ["feed:https://www.example2.com", false, false, true],
    ["chrome://foo/content/bar.xul", false, false, true],
    ["feed:chrome://foo/content/bar.xul", false, false, false],
    ["view-source:http://www.example2.com", true, true, true],
    ["view-source:https://www.example2.com", true, true, true],
    ["view-source:feed:http://www.example2.com", false, false, true],
    ["feed:view-source:http://www.example2.com", false, false, false],
    ["data:text/html,Hi", true, false, true],
    ["view-source:data:text/html,Hi", true, false, true],
    ["javascript:alert('hi')", true, false, true],
    ["moz://a", false, false, true],
    ["about:test-chrome-privs", false, false, true],
    ["about:test-unknown-unlinkable", false, false, true],
    ["about:test-content-unlinkable", false, false, true],
    ["about:test-content-linkable", true, true, true],
    // Because this page doesn't have SAFE_FOR_UNTRUSTED, the web can't link to it:
    ["about:test-unknown-linkable", false, false, true],
  ]],
  // about: related tests.
  ["about:test-chrome-privs", [
    ["about:test-chrome-privs", true, true, true],
    ["about:test-chrome-privs2", true, true, true],
    ["about:test-chrome-privs2?foo#bar", true, true, true],
    ["about:test-chrome-privs2?foo", true, true, true],
    ["about:test-chrome-privs2#bar", true, true, true],

    ["about:test-unknown-unlinkable", true, true, true],

    ["about:test-content-unlinkable", true, true, true],
    ["about:test-content-unlinkable?foo", true, true, true],
    ["about:test-content-unlinkable?foo#bar", true, true, true],
    ["about:test-content-unlinkable#bar", true, true, true],

    ["about:test-content-linkable", true, true, true],

    ["about:test-unknown-linkable", true, true, true],
  ]],
  ["about:test-unknown-unlinkable", [
    ["about:test-chrome-privs", false, false, true],

    // Can link to ourselves:
    ["about:test-unknown-unlinkable", true, true, true],
    // Can't link to unlinkable content if we're not sure it's privileged:
    ["about:test-unknown-unlinkable2", false, false, true],

    ["about:test-content-unlinkable", true, true, true],
    ["about:test-content-unlinkable2", true, true, true],
    ["about:test-content-unlinkable2?foo", true, true, true],
    ["about:test-content-unlinkable2?foo#bar", true, true, true],
    ["about:test-content-unlinkable2#bar", true, true, true],

    ["about:test-content-linkable", true, true, true],

    // Because this page doesn't have SAFE_FOR_UNTRUSTED, the web can't link to it:
    ["about:test-unknown-linkable", false, false, true],
  ]],
  ["about:test-content-unlinkable", [
    ["about:test-chrome-privs", false, false, true],

    // Can't link to unlinkable content if we're not sure it's privileged:
    ["about:test-unknown-unlinkable", false, false, true],

    ["about:test-content-unlinkable", true, true, true],
    ["about:test-content-unlinkable2", true, true, true],
    ["about:test-content-unlinkable2?foo", true, true, true],
    ["about:test-content-unlinkable2?foo#bar", true, true, true],
    ["about:test-content-unlinkable2#bar", true, true, true],

    ["about:test-content-linkable", true, true, true],
    ["about:test-unknown-linkable", false, false, true],
  ]],
  ["about:test-unknown-linkable", [
    ["about:test-chrome-privs", false, false, true],

    // Linkable content can't link to unlinkable content.
    ["about:test-unknown-unlinkable", false, false, true],

    ["about:test-content-unlinkable", false, false, true],
    ["about:test-content-unlinkable2", false, false, true],
    ["about:test-content-unlinkable2?foo", false, false, true],
    ["about:test-content-unlinkable2?foo#bar", false, false, true],
    ["about:test-content-unlinkable2#bar", false, false, true],

    // ... but it can link to other linkable content.
    ["about:test-content-linkable", true, true, true],

    // Can link to ourselves:
    ["about:test-unknown-linkable", true, true, true],

    // Because this page doesn't have SAFE_FOR_UNTRUSTED, the web can't link to it:
    ["about:test-unknown-linkable2", false, false, true],
  ]],
  ["about:test-content-linkable", [
    ["about:test-chrome-privs", false, false, true],

    // Linkable content can't link to unlinkable content.
    ["about:test-unknown-unlinkable", false, false, true],

    ["about:test-content-unlinkable", false, false, true],

    // ... but it can link to itself and other linkable content.
    ["about:test-content-linkable", true, true, true],
    ["about:test-content-linkable2", true, true, true],

    // Because this page doesn't have SAFE_FOR_UNTRUSTED, the web can't link to it:
    ["about:test-unknown-linkable", false, false, true],
  ]],
]);

function testURL(source, target, canLoad, canLoadWithoutInherit, canCreate, flags) {
  function getPrincipalDesc(principal) {
    if (principal.URI) {
      return principal.URI.spec;
    }
    if (principal.isSystemPrincipal) {
      return "system principal";
    }
    if (principal.isNullPrincipal) {
      return "null principal";
    }
    return "unknown principal";
  }
  let threw = false;
  let targetURI;
  try {
    targetURI = makeURI(target);
  } catch (ex) {
    ok(!canCreate, "Shouldn't be passing URIs that we can't create. Failed to create: " + target);
    return;
  }
  ok(canCreate, "Created a URI for " + target + " which should " +
     (canCreate ? "" : "not ") + "be possible.");
  try {
    ssm.checkLoadURIWithPrincipal(source, targetURI, flags);
  } catch (ex) {
    info(ex.message);
    threw = true;
  }
  let inheritDisallowed = flags & ssm.DISALLOW_INHERIT_PRINCIPAL;
  let shouldThrow = inheritDisallowed ? !canLoadWithoutInherit : !canLoad;
  ok(threw == shouldThrow,
     "Should " + (shouldThrow ? "" : "not ") + "throw an error when loading " +
     target + " from " + getPrincipalDesc(source) +
     (inheritDisallowed ? " without" : " with") + " principal inheritance.");
}

add_task(function* () {
  yield kAboutPagesRegistered;
  let baseFlags = ssm.STANDARD | ssm.DONT_REPORT_ERRORS;
  for (let [sourceString, targetsAndExpectations] of URLs) {
    let source;
    if (sourceString.startsWith("about:test-chrome-privs")) {
      source = ssm.getSystemPrincipal();
    } else {
      source = ssm.createCodebasePrincipal(makeURI(sourceString), {});
    }
    for (let [target, canLoad, canLoadWithoutInherit, canCreate] of targetsAndExpectations) {
      testURL(source, target, canLoad, canLoadWithoutInherit, canCreate, baseFlags);
      testURL(source, target, canLoad, canLoadWithoutInherit, canCreate,
              baseFlags | ssm.DISALLOW_INHERIT_PRINCIPAL);
    }
  }

  // Now test blob URIs, which we need to do in-content.
  yield BrowserTestUtils.withNewTab("http://www.example.com/", function* (browser) {
    yield ContentTask.spawn(
      browser,
      testURL.toString(),
      function* (testURLFn) {
        // eslint-disable-next-line no-shadow , no-eval
        let testURL = eval("(" + testURLFn + ")");
        // eslint-disable-next-line no-shadow
        let ssm = Services.scriptSecurityManager;
        // eslint-disable-next-line no-shadow
        let baseFlags = ssm.STANDARD | ssm.DONT_REPORT_ERRORS;
        // eslint-disable-next-line no-unused-vars
        let makeURI = Cu.import("resource://gre/modules/BrowserUtils.jsm", {}).BrowserUtils.makeURI;
        let b = new content.Blob(["I am a blob"]);
        let contentBlobURI = content.URL.createObjectURL(b);
        let contentPrincipal = content.document.nodePrincipal;
        // Loading this blob URI from the content page should work:
        testURL(contentPrincipal, contentBlobURI, true, true, true, baseFlags);
        testURL(contentPrincipal, contentBlobURI, true, true, true,
                baseFlags | ssm.DISALLOW_INHERIT_PRINCIPAL);

        testURL(contentPrincipal, "view-source:" + contentBlobURI, false, false, true,
                baseFlags);
        testURL(contentPrincipal, "view-source:" + contentBlobURI, false, false, true,
                baseFlags | ssm.DISALLOW_INHERIT_PRINCIPAL);

        // Feed URIs for blobs can't be created, so need to pass false as the fourth param.
        for (let prefix of ["feed:", "view-source:feed:", "feed:view-source:"]) {
          testURL(contentPrincipal, prefix + contentBlobURI, false, false, false,
                  baseFlags);
          testURL(contentPrincipal, prefix + contentBlobURI, false, false, false,
                  baseFlags | ssm.DISALLOW_INHERIT_PRINCIPAL);
        }
      }
    );

  });
});
