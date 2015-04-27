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

const kForceHostLookup = "browser.fixup.dns_first_for_single_words";
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
  Services.prefs.clearUserPref(kForceHostLookup);
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
    affectedByDNSForSingleHosts: false, // Whether the input host could be a host, but is normally assumed to be a keyword query
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
    input: "1.2.3.4/",
    fixedURI: "http://1.2.3.4/",
    protocolChange: true,
  }, {
    input: "1.2.3.4/foo",
    fixedURI: "http://1.2.3.4/foo",
    protocolChange: true,
  }, {
    input: "1.2.3.4:8000",
    fixedURI: "http://1.2.3.4:8000/",
    protocolChange: true,
  }, {
    input: "1.2.3.4:8000/",
    fixedURI: "http://1.2.3.4:8000/",
    protocolChange: true,
  }, {
    input: "1.2.3.4:8000/foo",
    fixedURI: "http://1.2.3.4:8000/foo",
    protocolChange: true,
  }, {
    input: "192.168.10.110",
    fixedURI: "http://192.168.10.110/",
    protocolChange: true,
  }, {
    input: "192.168.10.110/123",
    fixedURI: "http://192.168.10.110/123",
    protocolChange: true,
  }, {
    input: "192.168.10.110/123foo",
    fixedURI: "http://192.168.10.110/123foo",
    protocolChange: true,
  }, {
    input: "192.168.10.110:1234/123",
    fixedURI: "http://192.168.10.110:1234/123",
    protocolChange: true,
  }, {
    input: "192.168.10.110:1234/123foo",
    fixedURI: "http://192.168.10.110:1234/123foo",
    protocolChange: true,
  }, {
    input: "1.2.3",
    fixedURI: "http://1.2.3/",
    protocolChange: true,
  }, {
    input: "1.2.3/",
    fixedURI: "http://1.2.3/",
    protocolChange: true,
  }, {
    input: "1.2.3/foo",
    fixedURI: "http://1.2.3/foo",
    protocolChange: true,
  }, {
    input: "1.2.3/123",
    fixedURI: "http://1.2.3/123",
    protocolChange: true,
  }, {
    input: "1.2.3:8000",
    fixedURI: "http://1.2.3:8000/",
    protocolChange: true,
  }, {
    input: "1.2.3:8000/",
    fixedURI: "http://1.2.3:8000/",
    protocolChange: true,
  }, {
    input: "1.2.3:8000/foo",
    fixedURI: "http://1.2.3:8000/foo",
    protocolChange: true,
  }, {
    input: "1.2.3:8000/123",
    fixedURI: "http://1.2.3:8000/123",
    protocolChange: true,
  }, {
    input: "http://1.2.3",
    fixedURI: "http://1.2.3/",
  }, {
    input: "http://1.2.3/",
    fixedURI: "http://1.2.3/",
  }, {
    input: "http://1.2.3/foo",
    fixedURI: "http://1.2.3/foo",
  }, {
    input: "[::1]",
    fixedURI: "http://[::1]/",
    alternateURI: "http://[::1]/",
    protocolChange: true,
    affectedByWhitelist: true
  }, {
    input: "[::1]/",
    fixedURI: "http://[::1]/",
    alternateURI: "http://[::1]/",
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "[::1]:8000",
    fixedURI: "http://[::1]:8000/",
    alternateURI: "http://[::1]:8000/",
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "[::1]:8000/",
    fixedURI: "http://[::1]:8000/",
    alternateURI: "http://[::1]:8000/",
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "[[::1]]/",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "[fe80:cd00:0:cde:1257:0:211e:729c]",
    fixedURI: "http://[fe80:cd00:0:cde:1257:0:211e:729c]/",
    alternateURI: "http://[fe80:cd00:0:cde:1257:0:211e:729c]/",
    protocolChange: true,
    affectedByWhitelist: true
  }, {
    input: "[64:ff9b::8.8.8.8]",
    fixedURI: "http://[64:ff9b::8.8.8.8]/",
    protocolChange: true
  }, {
    input: "[64:ff9b::8.8.8.8]/~moz",
    fixedURI: "http://[64:ff9b::8.8.8.8]/~moz",
    protocolChange: true
  }, {
    input: "[::1][::1]",
    keywordLookup: true,
    protocolChange: true
  }, {
    input: "[::1][100",
    fixedURI: "http://[::1][100/",
    alternateURI: "http://[::1][100/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "[::1]]",
    keywordLookup: true,
    protocolChange: true
  }, {
    input: "1234",
    fixedURI: "http://1234/",
    alternateURI: "http://www.1234.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
    affectedByDNSForSingleHosts: true,
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
    affectedByDNSForSingleHosts: true,
  }, {
    input: "test.",
    fixedURI: "http://test./",
    alternateURI: "http://www.test./",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: ".test",
    fixedURI: "http://.test/",
    alternateURI: "http://www..test/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "mozilla is amazing",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "search ?mozilla",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "mozilla .com",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "what if firefox?",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "london's map",
    keywordLookup: true,
    protocolChange: true,
  }, {
    input: "mozilla ",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "   mozilla  ",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
    affectedByDNSForSingleHosts: true,
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
    affectedByDNSForSingleHosts: true,
  }, {
    input: "mozilla \r\n",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "moz\r\nfirefox\nos\r",
    fixedURI: "http://mozfirefoxos/",
    alternateURI: "http://www.mozfirefoxos.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
    affectedByDNSForSingleHosts: true,
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
  }, {
    input: "café.local",
    fixedURI: "http://café.local/",
    alternateURI: "http://www.café.local/",
    protocolChange: true
  }, {
    input: "47.6182,-122.830",
    fixedURI: "http://47.6182,-122.830/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "-47.6182,-23.51",
    fixedURI: "http://-47.6182,-23.51/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "-22.14,23.51-",
    fixedURI: "http://-22.14,23.51-/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "32.7",
    fixedURI: "http://32.7/",
    alternateURI: "http://www.32.7/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "5+2",
    fixedURI: "http://5+2/",
    alternateURI: "http://www.5+2.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "5/2",
    fixedURI: "http://5/2",
    alternateURI: "http://www.5.com/2",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "moz ?.::%27",
    keywordLookup: true,
    protocolChange: true
  }, {
    input: "mozilla.com/?q=search",
    fixedURI: "http://mozilla.com/?q=search",
    alternateURI: "http://www.mozilla.com/?q=search",
    protocolChange: true
  }, {
    input: "mozilla.com?q=search",
    fixedURI: "http://mozilla.com/?q=search",
    alternateURI: "http://www.mozilla.com/?q=search",
    protocolChange: true
  }, {
    input: "mozilla.com ?q=search",
    keywordLookup: true,
    protocolChange: true
  }, {
    input: "mozilla.com.?q=search",
    fixedURI: "http://mozilla.com./?q=search",
    protocolChange: true
  }, {
    input: "mozilla.com'?q=search",
    fixedURI: "http://mozilla.com'/?q=search",
    alternateURI: "http://www.mozilla.com'/?q=search",
    protocolChange: true
  }, {
    input: "mozilla.com':search",
    keywordLookup: true,
    protocolChange: true
  }, {
    input: "[mozilla]",
    keywordLookup: true,
    protocolChange: true
  }, {
    input: "':?",
    fixedURI: "http://'/?",
    alternateURI: "http://www.'.com/?",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true
  }, {
    input: "a?.com",
    fixedURI: "http://a/?.com",
    alternateURI: "http://www.a.com/?.com",
    protocolChange: true,
    affectedByWhitelist: true
  }, {
    input: "?'.com",
    fixedURI: "http:///?%27.com",
    alternateURI: "http://www..com/?%27.com",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true
  }, {
    input: "' ?.com",
    keywordLookup: true,
    protocolChange: true
  }, {
    input: "?mozilla",
    fixedURI: "http:///?mozilla",
    alternateURI: "http://www..com/?mozilla",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true
  }, {
    input: "??mozilla",
    fixedURI: "http:///??mozilla",
    alternateURI: "http://www..com/??mozilla",
    keywordLookup: true,
    protocolChange: true,
    affectedByWhitelist: true
  }, {
    input: "mozilla/",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "mozilla",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    protocolChange: true,
    keywordLookup: true,
    affectedByWhitelist: true,
    affectedByDNSForSingleHosts: true,
  }, {
    input: "mozilla5/2",
    fixedURI: "http://mozilla5/2",
    alternateURI: "http://www.mozilla5.com/2",
    protocolChange: true,
    affectedByWhitelist: true,
  }, {
    input: "mozilla/foo",
    fixedURI: "http://mozilla/foo",
    alternateURI: "http://www.mozilla.com/foo",
    protocolChange: true,
    affectedByWhitelist: true,
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
    affectedByDNSForSingleHosts: true,
  });
}

function sanitize(input) {
  return input.replace(/\r|\n/g, "").trim();
}


let gSingleWordHostLookup = false;
function run_test() {
  // Only keywordlookup things should be affected by requiring a DNS lookup for single-word hosts:
  do_print("Check only keyword lookup testcases should be affected by requiring DNS for single hosts");
  let affectedTests = testcases.filter(t => !t.keywordLookup && t.affectedByDNSForSingleHosts);
  if (affectedTests.length) {
    for (let testcase of affectedTests) {
      do_print("Affected: " + testcase.input);
    }
  }
  do_check_eq(affectedTests.length, 0);
  do_single_test_run();
  gSingleWordHostLookup = true;
  do_single_test_run();
}

function do_single_test_run() {
  Services.prefs.setBoolPref(kForceHostLookup, gSingleWordHostLookup);

  let relevantTests = gSingleWordHostLookup ? testcases.filter(t => t.keywordLookup) :
                                              testcases;

  for (let { input: testInput,
             fixedURI: expectedFixedURI,
             alternateURI: alternativeURI,
             keywordLookup: expectKeywordLookup,
             protocolChange: expectProtocolChange,
             affectedByWhitelist: affectedByWhitelist,
             inWhitelist: inWhitelist,
             affectedByDNSForSingleHosts: affectedByDNSForSingleHosts,
           } of relevantTests) {

    // Explicitly force these into a boolean
    expectKeywordLookup = !!expectKeywordLookup;
    expectProtocolChange = !!expectProtocolChange;
    affectedByWhitelist = !!affectedByWhitelist;
    inWhitelist = !!inWhitelist;
    affectedByDNSForSingleHosts = !!affectedByDNSForSingleHosts;

    expectKeywordLookup = expectKeywordLookup && (!affectedByDNSForSingleHosts || !gSingleWordHostLookup);

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

      do_print("Checking \"" + testInput + "\" with flags " + flags +
               " (host lookup for single words: " + (gSingleWordHostLookup ? "yes" : "no") + ")");

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
      do_check_eq(!!info.keywordProviderName, couldDoKeywordLookup && expectKeywordLookup);
      do_check_eq(info.fixupChangedProtocol, expectProtocolChange);
      do_check_eq(info.fixupCreatedAlternateURI, makeAlternativeURI && alternativeURI != null);

      // Check the preferred URI
      let requiresWhitelistedDomain = flags & urifixup.FIXUP_FLAG_REQUIRE_WHITELISTED_HOST;
      if (couldDoKeywordLookup) {
        if (expectKeywordLookup) {
          if (!affectedByWhitelist || (affectedByWhitelist && !inWhitelist)) {
            let urlparamInput = encodeURIComponent(sanitize(testInput)).replace(/%20/g, "+");
            // If the input starts with `?`, then info.preferredURI.spec will omit it
            // In order to test this behaviour, remove `?` only if it is the first character
            if (urlparamInput.startsWith("%3F")) {
              urlparamInput = urlparamInput.replace("%3F", "");
            }
            let searchURL = kSearchEngineURL.replace("{searchTerms}", urlparamInput);
            let spec = info.preferredURI.spec.replace(/%27/g, "'");
            do_check_eq(spec, searchURL);
          } else {
            do_check_eq(info.preferredURI, null);
          }
        } else {
          do_check_eq(info.preferredURI.spec, info.fixedURI.spec);
        }
      } else if (requiresWhitelistedDomain) {
        // Not a keyword search, but we want to enforce the host whitelist.
        // If we always do DNS lookups, we should always havea  preferred URI here - unless the
        // input starts with '?' in which case preferredURI will just be null...
        if (!affectedByWhitelist || (affectedByWhitelist && inWhitelist) || (gSingleWordHostLookup && !testInput.startsWith("?")))
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
