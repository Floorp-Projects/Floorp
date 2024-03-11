const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);

var pref = "browser.fixup.typo.scheme";

var data = [
  {
    // ttp -> http.
    wrong: "ttp://www.example.com/",
    fixed: "http://www.example.com/",
  },
  {
    // htp -> http.
    wrong: "htp://www.example.com/",
    fixed: "http://www.example.com/",
  },
  {
    // ttps -> https.
    wrong: "ttps://www.example.com/",
    fixed: "https://www.example.com/",
  },
  {
    // tps -> https.
    wrong: "tps://www.example.com/",
    fixed: "https://www.example.com/",
  },
  {
    // ps -> https.
    wrong: "ps://www.example.com/",
    fixed: "https://www.example.com/",
  },
  {
    // htps -> https.
    wrong: "htps://www.example.com/",
    fixed: "https://www.example.com/",
  },
  {
    // ile -> file.
    wrong: "ile:///this/is/a/test.html",
    fixed: "file:///this/is/a/test.html",
  },
  {
    // le -> file.
    wrong: "le:///this/is/a/test.html",
    fixed: "file:///this/is/a/test.html",
  },
  {
    // Replace ';' with ':'.
    wrong: "http;//www.example.com/",
    fixed: "http://www.example.com/",
    noPrefValue: "http://http;//www.example.com/",
  },
  {
    // Missing ':'.
    wrong: "https//www.example.com/",
    fixed: "https://www.example.com/",
    noPrefValue: "http://https//www.example.com/",
  },
  {
    // Missing ':' for file scheme.
    wrong: "file///this/is/a/test.html",
    fixed: "file:///this/is/a/test.html",
    noPrefValue: "http://file///this/is/a/test.html",
  },
  {
    // Valid should not be changed.
    wrong: "https://example.com/this/is/a/test.html",
    fixed: "https://example.com/this/is/a/test.html",
  },
  {
    // Unmatched should not be changed.
    wrong: "whatever://this/is/a/test.html",
    fixed: "whatever://this/is/a/test.html",
  },
  {
    // Spaces before @ are valid if it appears after the domain.
    wrong: "example.com/ @test.com",
    fixed: "http://example.com/%20@test.com",
    noPrefValue: "http://example.com/%20@test.com",
  },
];

var dontFixURIs = [
  {
    input: " leadingSpaceUsername@example.com/",
    testInfo: "dont fix usernames with leading space",
  },
  {
    input: "trailingSpacerUsername @example.com/",
    testInfo: "dont fix usernames with trailing space",
  },
  {
    input: "multiple words username@example.com/",
    testInfo: "dont fix usernames with multiple spaces",
  },
  {
    input: "one spaceTwo  SpacesThree   Spaces@example.com/",
    testInfo: "dont match multiple consecutive spaces",
  },
  {
    input: " dontMatchCredentialsWithSpaces: secret password @example.com/",
    testInfo: "dont fix credentials with spaces",
  },
];

var len = data.length;

add_task(async function setup() {
  // Force search service to fail, so we do not have any engines that can
  // interfere with this test.
  // Search engine integration is tested in test_URIFixup_search.js.
  Services.search.wrappedJSObject.errorToThrowInTest = "Settings";

  // When search service fails, we want the promise rejection to be uncaught
  // so we can continue running the test.
  PromiseTestUtils.expectUncaughtRejection(
    /Fake Settings error during search service initialization./
  );

  try {
    await setupSearchService();
  } catch {}
});

// Make sure we fix what needs fixing when there is no pref set.
add_task(function test_unset_pref_fixes_typos() {
  Services.prefs.clearUserPref(pref);
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let { preferredURI } = Services.uriFixup.getFixupURIInfo(
      item.wrong,
      Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
    );
    Assert.equal(preferredURI.spec, item.fixed);
  }
});

// Make sure we don't do anything when the pref is explicitly
// set to false.
add_task(function test_false_pref_keeps_typos() {
  Services.prefs.setBoolPref(pref, false);
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let { preferredURI } = Services.uriFixup.getFixupURIInfo(
      item.wrong,
      Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
    );
    Assert.equal(preferredURI.spec, item.noPrefValue || item.wrong);
  }
});

// Finally, make sure we still fix what needs fixing if the pref is
// explicitly set to true.
add_task(function test_true_pref_fixes_typos() {
  Services.prefs.setBoolPref(pref, true);
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let { preferredURI } = Services.uriFixup.getFixupURIInfo(
      item.wrong,
      Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
    );
    Assert.equal(preferredURI.spec, item.fixed);
  }
});

add_task(function test_dont_fix_uris() {
  let dontFixLength = dontFixURIs.length;
  for (let i = 0; i < dontFixLength; i++) {
    let testCase = dontFixURIs[i];
    Assert.throws(
      () => {
        Services.uriFixup.getFixupURIInfo(
          testCase.input,
          Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
        );
      },
      /NS_ERROR_MALFORMED_URI/,
      testCase.testInfo
    );
  }
});
