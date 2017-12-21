var urifixup = Cc["@mozilla.org/docshell/urifixup;1"].
               getService(Ci.nsIURIFixup);
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AppConstants.jsm");

Services.prefs.setBoolPref("keyword.enabled", true);

const kSearchEngineID = "test_urifixup_search_engine";
const kSearchEngineURL = "http://www.example.org/?search={searchTerms}";
Services.search.addEngineWithDetails(kSearchEngineID, "", "", "", "get",
                                     kSearchEngineURL);

var oldDefaultEngine = Services.search.defaultEngine;
Services.search.defaultEngine = Services.search.getEngineByName(kSearchEngineID);

var selectedName = Services.search.defaultEngine.name;
Assert.equal(selectedName, kSearchEngineID);

do_register_cleanup(function() {
  if (oldDefaultEngine) {
    Services.search.defaultEngine = oldDefaultEngine;
  }
  let engine = Services.search.getEngineByName(kSearchEngineID);
  if (engine) {
    Services.search.removeEngine(engine);
  }
  Services.prefs.clearUserPref("keyword.enabled");
});

var isWin = AppConstants.platform == "win";

var data = [
  {
    // Valid should not be changed.
    wrong: 'https://example.com/this/is/a/test.html',
    fixed: 'https://example.com/this/is/a/test.html',
  },
  {
    // Unrecognized protocols should be changed.
    wrong: 'whatever://this/is/a/test.html',
    fixed: kSearchEngineURL.replace("{searchTerms}", encodeURIComponent('whatever://this/is/a/test.html')),
  },

  // The following tests check that when a user:password is present in the URL
  // `user:` isn't treated as an unknown protocol thus leaking the user and
  // password to the search engine.
  {
    wrong: 'user:pass@example.com/this/is/a/test.html',
    fixed: 'http://user:pass@example.com/this/is/a/test.html',
  },
  {
    wrong: 'user@example.com:8080/this/is/a/test.html',
    fixed: 'http://user@example.com:8080/this/is/a/test.html',
  },
  {
    wrong: 'https:pass@example.com/this/is/a/test.html',
    fixed: 'https://pass@example.com/this/is/a/test.html',
  },
  {
    wrong: 'user:pass@example.com:8080/this/is/a/test.html',
    fixed: 'http://user:pass@example.com:8080/this/is/a/test.html',
  },
  {
    wrong: 'http:user:pass@example.com:8080/this/is/a/test.html',
    fixed: 'http://user:pass@example.com:8080/this/is/a/test.html',
  },
  {
    wrong: 'ttp:user:pass@example.com:8080/this/is/a/test.html',
    fixed: 'http://user:pass@example.com:8080/this/is/a/test.html',
  },
  {
    wrong: 'gobbledygook:user:pass@example.com:8080/this/is/a/test.html',
    fixed: 'http://gobbledygook:user%3Apass@example.com:8080/this/is/a/test.html',
  },
  {
    wrong: 'user:@example.com:8080/this/is/a/test.html',
    fixed: 'http://user@example.com:8080/this/is/a/test.html',
  },
  {
    wrong: '//user:pass@example.com:8080/this/is/a/test.html',
    fixed: (isWin ? "http:" : "file://") + '//user:pass@example.com:8080/this/is/a/test.html',
  },
  {
    wrong: '://user:pass@example.com:8080/this/is/a/test.html',
    fixed: 'http://user:pass@example.com:8080/this/is/a/test.html',
  },
  {
    wrong: 'whatever://this/is/a@b/test.html',
    fixed: kSearchEngineURL.replace("{searchTerms}", encodeURIComponent('whatever://this/is/a@b/test.html')),
  },
];

var extProtocolSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"]
                     .getService(Ci.nsIExternalProtocolService);

if (extProtocolSvc && extProtocolSvc.externalProtocolHandlerExists("mailto")) {
  data.push({
    wrong: "mailto:foo@bar.com",
    fixed: "mailto:foo@bar.com"
  });
}

function run_test() {
  run_next_test();
}

var len = data.length;
// Make sure we fix what needs fixing
add_task(function test_fix_unknown_schemes() {
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let result =
      urifixup.createFixupURI(item.wrong,
                              urifixup.FIXUP_FLAG_FIX_SCHEME_TYPOS).spec;
    Assert.equal(result, item.fixed);
  }
});
