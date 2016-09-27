"use strict";

let ssm = Services.scriptSecurityManager;

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
  ]],
]);

function testURL(source, target, canLoad, canLoadWithoutInherit, canCreate, flags) {
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
     target + " from " + source.URI.spec +
     (inheritDisallowed ? " without" : " with") + " principal inheritance.");
}

add_task(function* () {
  let baseFlags = ssm.STANDARD | ssm.DONT_REPORT_ERRORS;
  for (let [sourceString, targetsAndExpectations] of URLs) {
    let source = ssm.createCodebasePrincipal(makeURI(sourceString), {});
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
        let testURL = eval("(" + testURLFn + ")");
        let ssm = Services.scriptSecurityManager;
        let baseFlags = ssm.STANDARD | ssm.DONT_REPORT_ERRORS;
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
