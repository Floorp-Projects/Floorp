let urifixup = Cc["@mozilla.org/docshell/urifixup;1"].
               getService(Ci.nsIURIFixup);

Components.utils.import("resource://gre/modules/Services.jsm");

let prefList = ["browser.fixup.typo.scheme", "keyword.enabled",
                "browser.fixup.domainwhitelist.whitelisted"];
for (let pref of prefList) {
  Services.prefs.setBoolPref(pref, true);
}

const kSearchEngineID = "test_urifixup_search_engine";
const kSearchEngineURL = "http://www.example.org/?search={searchTerms}";
Services.search.addEngineWithDetails(kSearchEngineID, "", "", "", "get",
                                     kSearchEngineURL);

let oldDefaultEngine = Services.search.defaultEngine;
Services.search.defaultEngine = Services.search.getEngineByName(kSearchEngineID);

let selectedName = Services.search.defaultEngine.name;
do_check_eq(selectedName, kSearchEngineID);

do_register_cleanup(function() {
  if (oldDefaultEngine) {
    Services.search.defaultEngine = oldDefaultEngine;
  }
  let engine = Services.search.getEngineByName(kSearchEngineID);
  if (engine) {
    Services.search.removeEngine(engine);
  }
  Services.prefs.clearUserPref("keyword.enabled");
  Services.prefs.clearUserPref("browser.fixup.typo.scheme");
});

let flagInputs = [
  urifixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP,
  urifixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI,
  urifixup.FIXUP_FLAG_FIX_SCHEME_TYPOS,
  urifixup.FIXUP_FLAG_REQUIRE_WHITELISTED_HOST,
];

flagInputs.concat([
  flagInputs[0] | flagInputs[1],
  flagInputs[1] | flagInputs[2],
  flagInputs[0] | flagInputs[2],
  flagInputs[0] | flagInputs[1] | flagInputs[2]
]);

/*
  The following properties are supported for these test cases:
  {
    input: "", // Input string, required
    fixedURI: "", // Expected fixedURI
    alternateURI: "", // Expected alternateURI
    keywordLookup: false, // Whether a keyword lookup is expected
    protocolChange: false, // Whether a protocol change is expected
    affectedByWhitelist: false, // Whether the input host is affected by the whitelist
    inWhitelist: false, // Whether the input host is in the whitelist
  }
*/
let testcases = [ {
    input: "http://www.mozilla.org",
    fixedURI: "http://www.mozilla.org/",
  }, {
    input: "http://127.0.0.1/",
    fixedURI: "http://127.0.0.1/",
  }, {
    input: "file:///foo/bar",
    fixedURI: "file:///foo/bar",
  }, {
    input: "://www.mozilla.org",
    fixedURI: "http://www.mozilla.org/",
    protocolChange: true,
  }, {
    input: "www.mozilla.org",
    fixedURI: "http://www.mozilla.org/",
    protocolChange: true,
  }, {
    input: "http://mozilla/",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
  }, {
    input: "http://test./",
    fixedURI: "http://test./",
    alternateURI: "http://www.test./",
  }, {
    input: "127.0.0.1",
    fixedURI: "http://127.0.0.1/",
    protocolChange: true,
  }, {
    input: "1234",
    fixedURI: "http://1234/",
    alternateURI: "http://www.1234.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "host/foo.txt",
    fixedURI: "http://host/foo.txt",
    alternateURI: "http://www.host.com/foo.txt",
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "mozilla",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "test.",
    fixedURI: "http://test./",
    alternateURI: "http://www.test./",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: ".test",
    fixedURI: "http://.test/",
    alternateURI: "http://www..test/",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "mozilla is amazing",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "mozilla ",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "   mozilla  ",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "mozilla \\",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "mozilla \\ foo.txt",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "mozilla \\\r foo.txt",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "mozilla\n",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "mozilla \r\n",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "moz\r\nfirefox\nos\r",
    fixedURI: "http://mozfirefoxos/",
    alternateURI: "http://www.mozfirefoxos.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "moz\r\n firefox\n",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "[]",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "http://whitelisted/",
    fixedURI: "http://whitelisted/",
    alternateURI: "http://www.whitelisted.com/",
    affectedByWhitelist: true,
    inWhitelist: true,
  }];

if (Services.appinfo.OS.toLowerCase().startsWith("win")) {
  testcases.push({
    input: "C:\\some\\file.txt",
    fixedURI: "file:///C:/some/file.txt",
    protocolChange: true,
  });
  testcases.push({
    input: "//mozilla",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    protocolChange: true,
    affectedByWhitelist: true,
  });
  testcases.push({
    input: "mozilla\\",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  });
} else {
  testcases.push({
    input: "/some/file.txt",
    fixedURI: "file:///some/file.txt",
    protocolChange: true,
  });
  testcases.push({
    input: "//mozilla",
    fixedURI: "file:////mozilla",
    protocolChange: true,
  });
  testcases.push({
    input: "mozilla\\",
    fixedURI: "http://mozilla\\/",
    alternateURI: "http://www.mozilla/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
  });
}

function sanitize(input) {
  return input.replace(/\r|\n/g, "").trim();
}

function run_test() {
  for (let { input: testInput,
             fixedURI: expectedFixedURI,
             alternateURI: alternativeURI,
             keywordLookup: expectKeywordLookup,
             protocolChange: expectProtocolChange,
             affectedByWhitelist: affectedByWhitelist,
             inWhitelist: inWhitelist } of testcases) {

    // Explicitly force these into a boolean
    expectKeywordLookup = !!expectKeywordLookup;
    expectProtocolChange = !!expectProtocolChange;
    affectedByWhitelist = !!affectedByWhitelist;
    inWhitelist = !!inWhitelist;

    for (let flags of flagInputs) {
      let info;
      let fixupURIOnly = null;
      try {
        fixupURIOnly = urifixup.createFixupURI(testInput, flags);
      } catch (ex) {
        do_print("Caught exception: " + ex);
        do_check_eq(expectedFixedURI, null);
      }

      try {
        info = urifixup.getFixupURIInfo(testInput, flags);
      } catch (ex) {
        // Both APIs should return an error in the same cases.
        do_print("Caught exception: " + ex);
        do_check_eq(expectedFixedURI, null);
        do_check_eq(fixupURIOnly, null);
        continue;
      }

      do_print("Checking \"" + testInput + "\" with flags " + flags);

      // Both APIs should then also be using the same spec.
      do_check_eq(!!fixupURIOnly, !!info.preferredURI);
      if (fixupURIOnly)
        do_check_eq(fixupURIOnly.spec, info.preferredURI.spec);

      let isFileURL = expectedFixedURI && expectedFixedURI.startsWith("file");

      // Check the fixedURI:
      let makeAlternativeURI = flags & urifixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI;
      if (makeAlternativeURI && alternativeURI != null) {
        do_check_eq(info.fixedURI.spec, alternativeURI);
      } else {
        do_check_eq(info.fixedURI && info.fixedURI.spec, expectedFixedURI);
      }

      // Check booleans on input:
      let couldDoKeywordLookup = flags & urifixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
      do_check_eq(info.fixupUsedKeyword, couldDoKeywordLookup && expectKeywordLookup);
      do_check_eq(info.fixupChangedProtocol, expectProtocolChange);
      do_check_eq(info.fixupCreatedAlternateURI, makeAlternativeURI && alternativeURI != null);

      // Check the preferred URI
      let requiresWhitelistedDomain = flags & urifixup.FIXUP_FLAG_REQUIRE_WHITELISTED_HOST
      if (couldDoKeywordLookup) {
        if (expectKeywordLookup) {
          if (!affectedByWhitelist || (affectedByWhitelist && !inWhitelist)) {
        let urlparamInput = encodeURIComponent(sanitize(testInput)).replace("%20", "+", "g");
        let searchURL = kSearchEngineURL.replace("{searchTerms}", urlparamInput);
        do_check_eq(info.preferredURI.spec, searchURL);
      } else {
            do_check_eq(info.preferredURI, null);
          }
        } else {
          do_check_eq(info.preferredURI.spec, info.fixedURI.spec);
        }
      } else if (requiresWhitelistedDomain) {
        // Not a keyword search, but we want to enforce the host whitelist
        if (!affectedByWhitelist || (affectedByWhitelist && inWhitelist))
          do_check_eq(info.preferredURI.spec, info.fixedURI.spec);
        else
          do_check_eq(info.preferredURI, null);
      } else {
        // In these cases, we should never be doing a keyword lookup and
        // the fixed URI should be preferred:
        do_check_eq(info.preferredURI.spec, info.fixedURI.spec);
      }
      do_check_eq(sanitize(testInput), info.originalInput);
    }
  }
}
