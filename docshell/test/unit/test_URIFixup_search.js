const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

var isWin = AppConstants.platform == "win";

var data = [
  {
    // Valid should not be changed.
    wrong: "https://example.com/this/is/a/test.html",
    fixed: "https://example.com/this/is/a/test.html",
  },
  {
    // Unrecognized protocols should be changed.
    wrong: "whatever://this/is/a/test.html",
    fixed: kSearchEngineURL.replace(
      "{searchTerms}",
      encodeURIComponent("whatever://this/is/a/test.html")
    ),
  },

  {
    // Unrecognized protocols should be changed.
    wrong: "whatever://this/is/a/test.html",
    fixed: kPrivateSearchEngineURL.replace(
      "{searchTerms}",
      encodeURIComponent("whatever://this/is/a/test.html")
    ),
    inPrivateBrowsing: true,
  },

  // The following tests check that when a user:password is present in the URL
  // `user:` isn't treated as an unknown protocol thus leaking the user and
  // password to the search engine.
  {
    wrong: "user:pass@example.com/this/is/a/test.html",
    fixed: "http://user:pass@example.com/this/is/a/test.html",
  },
  {
    wrong: "user@example.com:8080/this/is/a/test.html",
    fixed: "http://user@example.com:8080/this/is/a/test.html",
  },
  {
    wrong: "https:pass@example.com/this/is/a/test.html",
    fixed: "https://pass@example.com/this/is/a/test.html",
  },
  {
    wrong: "user:pass@example.com:8080/this/is/a/test.html",
    fixed: "http://user:pass@example.com:8080/this/is/a/test.html",
  },
  {
    wrong: "http:user:pass@example.com:8080/this/is/a/test.html",
    fixed: "http://user:pass@example.com:8080/this/is/a/test.html",
  },
  {
    wrong: "ttp:user:pass@example.com:8080/this/is/a/test.html",
    fixed: "http://user:pass@example.com:8080/this/is/a/test.html",
  },
  {
    wrong: "gobbledygook:user:pass@example.com:8080/this/is/a/test.html",
    fixed:
      "http://gobbledygook:user%3Apass@example.com:8080/this/is/a/test.html",
  },
  {
    wrong: "user:@example.com:8080/this/is/a/test.html",
    fixed: "http://user@example.com:8080/this/is/a/test.html",
  },
  {
    wrong: "//user:pass@example.com:8080/this/is/a/test.html",
    fixed:
      (isWin ? "http:" : "file://") +
      "//user:pass@example.com:8080/this/is/a/test.html",
  },
  {
    wrong: "://user:pass@example.com:8080/this/is/a/test.html",
    fixed: "http://user:pass@example.com:8080/this/is/a/test.html",
  },
  {
    wrong: "localhost:8080/?param=1",
    fixed: "http://localhost:8080/?param=1",
  },
  {
    wrong: "localhost:8080?param=1",
    fixed: "http://localhost:8080/?param=1",
  },
  {
    wrong: "localhost:8080#somewhere",
    fixed: "http://localhost:8080/#somewhere",
  },
  {
    wrong: "whatever://this/is/a@b/test.html",
    fixed: kSearchEngineURL.replace(
      "{searchTerms}",
      encodeURIComponent("whatever://this/is/a@b/test.html")
    ),
  },
];

var extProtocolSvc = Cc[
  "@mozilla.org/uriloader/external-protocol-service;1"
].getService(Ci.nsIExternalProtocolService);

if (extProtocolSvc && extProtocolSvc.externalProtocolHandlerExists("mailto")) {
  data.push({
    wrong: "mailto:foo@bar.com",
    fixed: "mailto:foo@bar.com",
  });
}

var len = data.length;

add_task(async function setup() {
  await setupSearchService();
  await addTestEngines();

  Services.prefs.setBoolPref("keyword.enabled", true);
  Services.prefs.setBoolPref("browser.search.separatePrivateDefault", true);
  Services.prefs.setBoolPref(
    "browser.search.separatePrivateDefault.ui.enabled",
    true
  );

  await Services.search.setDefault(
    Services.search.getEngineByName(kSearchEngineID),
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.setDefaultPrivate(
    Services.search.getEngineByName(kPrivateSearchEngineID),
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
});

// Make sure we fix what needs fixing
add_task(function test_fix_unknown_schemes() {
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let flags = Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS;
    if (item.inPrivateBrowsing) {
      flags |= Services.uriFixup.FIXUP_FLAG_PRIVATE_CONTEXT;
    }
    let { preferredURI } = Services.uriFixup.getFixupURIInfo(item.wrong, flags);
    Assert.equal(preferredURI.spec, item.fixed);
  }
});
