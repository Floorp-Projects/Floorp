const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const kSearchEngineID = "test_urifixup_search_engine";
const kSearchEngineURL = "http://www.example.org/?search={searchTerms}";
const kPrivateSearchEngineID = "test_urifixup_search_engine_private";
const kPrivateSearchEngineURL = "http://www.example.org/?private={searchTerms}";

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

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  Services.prefs.setBoolPref("keyword.enabled", true);
  Services.prefs.setBoolPref("browser.search.separatePrivateDefault", true);
  Services.prefs.setBoolPref(
    "browser.search.separatePrivateDefault.ui.enabled",
    true
  );

  Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler)
    .setSubstitution(
      "search-extensions",
      Services.io.newURI("chrome://mozapps/locale/searchextensions/")
    );

  var oldCurrentEngine = await Services.search.getDefault();
  var oldPrivateEngine = await Services.search.getDefaultPrivate();

  let newCurrentEngine = await Services.search.addEngineWithDetails(
    kSearchEngineID,
    {
      method: "get",
      template: kSearchEngineURL,
    }
  );
  await Services.search.setDefault(newCurrentEngine);

  let newPrivateEngine = await Services.search.addEngineWithDetails(
    kPrivateSearchEngineID,
    {
      method: "get",
      template: kPrivateSearchEngineURL,
    }
  );
  await Services.search.setDefaultPrivate(newPrivateEngine);

  registerCleanupFunction(async function() {
    if (oldCurrentEngine) {
      await Services.search.setDefault(oldCurrentEngine);
    }
    if (oldPrivateEngine) {
      await Services.search.setDefault(oldPrivateEngine);
    }
    await Services.search.removeEngine(newCurrentEngine);
    await Services.search.removeEngine(newPrivateEngine);
    Services.prefs.clearUserPref("keyword.enabled");
    Services.prefs.clearUserPref("browser.search.separatePrivateDefault");
    Services.prefs.clearUserPref(
      "browser.search.separatePrivateDefault.ui.enabled"
    );
  });
});

// Make sure we fix what needs fixing
add_task(function test_fix_unknown_schemes() {
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let flags = Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS;
    if (item.inPrivateBrowsing) {
      flags |= Services.uriFixup.FIXUP_FLAG_PRIVATE_CONTEXT;
    }
    let result = Services.uriFixup.createFixupURI(item.wrong, flags).spec;
    Assert.equal(result, item.fixed);
  }
});
