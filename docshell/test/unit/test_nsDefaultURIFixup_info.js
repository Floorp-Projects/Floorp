const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const kSearchEngineID = "test_urifixup_search_engine";
const kSearchEngineURL = "http://www.example.org/?search={searchTerms}";
const kPrivateSearchEngineID = "test_urifixup_search_engine_private";
const kPrivateSearchEngineURL = "http://www.example.org/?private={searchTerms}";
const kForceHostLookup = "browser.fixup.dns_first_for_single_words";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

// TODO(bug 1522134), this test should also use
// combinations of the following flags.
var flagInputs = [
  Services.uriFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP,
  Services.uriFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP |
    Services.uriFixup.FIXUP_FLAG_PRIVATE_CONTEXT,
  Services.uriFixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI,
  Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS,
  // This should not really generate a search, but it does, see Bug 1588118.
  Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
    Services.uriFixup.FIXUP_FLAG_PRIVATE_CONTEXT,
];

/*
  The following properties are supported for these test cases:
  {
    input: "", // Input string, required
    fixedURI: "", // Expected fixedURI
    alternateURI: "", // Expected alternateURI
    keywordLookup: false, // Whether a keyword lookup is expected
    protocolChange: false, // Whether a protocol change is expected
    inWhitelist: false, // Whether the input host is in the whitelist
    affectedByDNSForSingleHosts: false, // Whether the input host could be a host, but is normally assumed to be a keyword query
  }
*/
var testcases = [
  {
    input: "http://www.mozilla.org",
    fixedURI: "http://www.mozilla.org/",
  },
  {
    input: "http://127.0.0.1/",
    fixedURI: "http://127.0.0.1/",
  },
  {
    input: "file:///foo/bar",
    fixedURI: "file:///foo/bar",
  },
  {
    input: "://www.mozilla.org",
    fixedURI: "http://www.mozilla.org/",
    protocolChange: true,
  },
  {
    input: "www.mozilla.org",
    fixedURI: "http://www.mozilla.org/",
    protocolChange: true,
  },
  {
    input: "http://mozilla/",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
  },
  {
    input: "http://test./",
    fixedURI: "http://test./",
    alternateURI: "http://www.test./",
  },
  {
    input: "127.0.0.1",
    fixedURI: "http://127.0.0.1/",
    protocolChange: true,
  },
  {
    input: "1.2.3.4/",
    fixedURI: "http://1.2.3.4/",
    protocolChange: true,
  },
  {
    input: "1.2.3.4/foo",
    fixedURI: "http://1.2.3.4/foo",
    protocolChange: true,
  },
  {
    input: "1.2.3.4:8000",
    fixedURI: "http://1.2.3.4:8000/",
    protocolChange: true,
  },
  {
    input: "1.2.3.4:8000/",
    fixedURI: "http://1.2.3.4:8000/",
    protocolChange: true,
  },
  {
    input: "1.2.3.4:8000/foo",
    fixedURI: "http://1.2.3.4:8000/foo",
    protocolChange: true,
  },
  {
    input: "192.168.10.110",
    fixedURI: "http://192.168.10.110/",
    protocolChange: true,
  },
  {
    input: "192.168.10.110/123",
    fixedURI: "http://192.168.10.110/123",
    protocolChange: true,
  },
  {
    input: "192.168.10.110/123foo",
    fixedURI: "http://192.168.10.110/123foo",
    protocolChange: true,
  },
  {
    input: "192.168.10.110:1234/123",
    fixedURI: "http://192.168.10.110:1234/123",
    protocolChange: true,
  },
  {
    input: "192.168.10.110:1234/123foo",
    fixedURI: "http://192.168.10.110:1234/123foo",
    protocolChange: true,
  },
  {
    input: "1.2.3",
    fixedURI: "http://1.2.0.3/",
    protocolChange: true,
  },
  {
    input: "1.2.3/",
    fixedURI: "http://1.2.0.3/",
    protocolChange: true,
  },
  {
    input: "1.2.3/foo",
    fixedURI: "http://1.2.0.3/foo",
    protocolChange: true,
  },
  {
    input: "1.2.3/123",
    fixedURI: "http://1.2.0.3/123",
    protocolChange: true,
  },
  {
    input: "1.2.3:8000",
    fixedURI: "http://1.2.0.3:8000/",
    protocolChange: true,
  },
  {
    input: "1.2.3:8000/",
    fixedURI: "http://1.2.0.3:8000/",
    protocolChange: true,
  },
  {
    input: "1.2.3:8000/foo",
    fixedURI: "http://1.2.0.3:8000/foo",
    protocolChange: true,
  },
  {
    input: "1.2.3:8000/123",
    fixedURI: "http://1.2.0.3:8000/123",
    protocolChange: true,
  },
  {
    input: "http://1.2.3",
    fixedURI: "http://1.2.0.3/",
  },
  {
    input: "http://1.2.3/",
    fixedURI: "http://1.2.0.3/",
  },
  {
    input: "http://1.2.3/foo",
    fixedURI: "http://1.2.0.3/foo",
  },
  {
    input: "[::1]",
    fixedURI: "http://[::1]/",
    alternateURI: "http://[::1]/",
    protocolChange: true,
  },
  {
    input: "[::1]/",
    fixedURI: "http://[::1]/",
    alternateURI: "http://[::1]/",
    protocolChange: true,
  },
  {
    input: "[::1]:8000",
    fixedURI: "http://[::1]:8000/",
    protocolChange: true,
  },
  {
    input: "[::1]:8000/",
    fixedURI: "http://[::1]:8000/",
    protocolChange: true,
  },
  {
    input: "[[::1]]/",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "[fe80:cd00:0:cde:1257:0:211e:729c]",
    fixedURI: "http://[fe80:cd00:0:cde:1257:0:211e:729c]/",
    alternateURI: "http://[fe80:cd00:0:cde:1257:0:211e:729c]/",
    protocolChange: true,
  },
  {
    input: "[64:ff9b::8.8.8.8]",
    fixedURI: "http://[64:ff9b::808:808]/",
    alternateURI: "http://[64:ff9b::808:808]/",
    protocolChange: true,
  },
  {
    input: "[64:ff9b::8.8.8.8]/~moz",
    fixedURI: "http://[64:ff9b::808:808]/~moz",
    alternateURI: "http://[64:ff9b::808:808]/~moz",
    protocolChange: true,
  },
  {
    input: "[::1][::1]",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "[::1][100",
    fixedURI: null,
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "[::1]]",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "1234",
    fixedURI: "http://0.0.4.210/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "host/foo.txt",
    fixedURI: "http://host/foo.txt",
    alternateURI: "http://www.host.com/foo.txt",
    protocolChange: true,
  },
  {
    input: "mozilla",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "test.",
    fixedURI: "http://test./",
    alternateURI: "http://www.test./",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: ".test",
    fixedURI: "http://.test/",
    alternateURI: "http://www..test/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "mozilla is amazing",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "search ?mozilla",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "mozilla .com",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "what if firefox?",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "london's map",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "mozilla ",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "   mozilla  ",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "mozilla \\",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "mozilla \\ foo.txt",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "mozilla \\\r foo.txt",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "mozilla\n",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "mozilla \r\n",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "moz\r\nfirefox\nos\r",
    fixedURI: "http://mozfirefoxos/",
    alternateURI: "http://www.mozfirefoxos.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "moz\r\n firefox\n",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "[]",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "http://whitelisted/",
    fixedURI: "http://whitelisted/",
    alternateURI: "http://www.whitelisted.com/",
    inWhitelist: true,
  },
  {
    input: "café.local",
    fixedURI: "http://xn--caf-dma.local/",
    alternateURI: "http://www.xn--caf-dma.local/",
    protocolChange: true,
  },
  {
    input: "47.6182,-122.830",
    fixedURI: "http://47.6182,-122.830/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "-47.6182,-23.51",
    fixedURI: "http://-47.6182,-23.51/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "-22.14,23.51-",
    fixedURI: "http://-22.14,23.51-/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "32.7",
    fixedURI: "http://32.0.0.7/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "5+2",
    fixedURI: "http://5+2/",
    alternateURI: "http://www.5+2.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "5/2",
    fixedURI: "http://0.0.0.5/2",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "moz ?.::%27",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "mozilla.com/?q=search",
    fixedURI: "http://mozilla.com/?q=search",
    alternateURI: "http://www.mozilla.com/?q=search",
    protocolChange: true,
  },
  {
    input: "mozilla.com?q=search",
    fixedURI: "http://mozilla.com/?q=search",
    alternateURI: "http://www.mozilla.com/?q=search",
    protocolChange: true,
  },
  {
    input: "mozilla.com ?q=search",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "mozilla.com.?q=search",
    fixedURI: "http://mozilla.com./?q=search",
    protocolChange: true,
  },
  {
    input: "mozilla.com'?q=search",
    fixedURI: "http://mozilla.com'/?q=search",
    alternateURI: "http://www.mozilla.com'/?q=search",
    protocolChange: true,
  },
  {
    input: "mozilla.com':search",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "[mozilla]",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "':?",
    fixedURI: "http://'/?",
    alternateURI: "http://www.'.com/?",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "a?.com",
    fixedURI: "http://a/?.com",
    alternateURI: "http://www.a.com/?.com",
    protocolChange: true,
  },
  {
    input: "?'.com",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "' ?.com",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "?mozilla",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "??mozilla",
    keywordLookup: true,
    protocolChange: true,
  },
  {
    input: "mozilla/",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    protocolChange: true,
  },
  {
    input: "mozilla",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    protocolChange: true,
    keywordLookup: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "mozilla5/2",
    fixedURI: "http://mozilla5/2",
    alternateURI: "http://www.mozilla5.com/2",
    protocolChange: true,
  },
  {
    input: "mozilla/foo",
    fixedURI: "http://mozilla/foo",
    alternateURI: "http://www.mozilla.com/foo",
    protocolChange: true,
  },
  {
    input: "mozilla\\",
    fixedURI: "http://mozilla/",
    alternateURI: "http://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "localhost",
    fixedURI: "http://localhost/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleHosts: true,
  },
  {
    input: "localhost:8080",
    fixedURI: "http://localhost:8080/",
    protocolChange: true,
  },
  {
    input: "plonk:8080",
    fixedURI: "http://plonk:8080/",
    protocolChange: true,
  },
  {
    input:
      "\u10E0\u10D4\u10D2\u10D8\u10E1\u10E2\u10E0\u10D0\u10EA\u10D8\u10D0.\u10D2\u10D4",
    fixedURI: "http://xn--lodaehvb5cdik4g.xn--node/",
    alternateURI: "http://www.xn--lodaehvb5cdik4g.xn--node/",
    protocolChange: true,
  },
  {
    input: " \t mozilla.org/\t \t ",
    fixedURI: "http://mozilla.org/",
    alternateURI: "http://www.mozilla.org/",
    protocolChange: true,
  },
  {
    input: " moz\ti\tlla.org ",
    keywordLookup: true,
  },
];

if (AppConstants.platform == "win") {
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
}

function sanitize(input) {
  return input.replace(/\r|\n/g, "").trim();
}

add_task(async function setup() {
  var prefList = [
    "browser.fixup.typo.scheme",
    "keyword.enabled",
    "browser.fixup.domainwhitelist.whitelisted",
    "browser.search.separatePrivateDefault",
    "browser.search.separatePrivateDefault.ui.enabled",
  ];
  for (let pref of prefList) {
    Services.prefs.setBoolPref(pref, true);
  }

  await AddonTestUtils.promiseStartupManager();

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

  var selectedName = (await Services.search.getDefault()).name;
  Assert.equal(selectedName, kSearchEngineID);

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
    Services.prefs.clearUserPref("browser.fixup.typo.scheme");
    Services.prefs.clearUserPref(kForceHostLookup);
    Services.prefs.clearUserPref("browser.search.separatePrivateDefault");
    Services.prefs.clearUserPref(
      "browser.search.separatePrivateDefault.ui.enabled"
    );
  });
});

var gSingleWordHostLookup = false;
add_task(async function run_test() {
  // Only keywordlookup things should be affected by requiring a DNS lookup for single-word hosts:
  info(
    "Check only keyword lookup testcases should be affected by requiring DNS for single hosts"
  );
  let affectedTests = testcases.filter(
    t => !t.keywordLookup && t.affectedByDNSForSingleHosts
  );
  if (affectedTests.length) {
    for (let testcase of affectedTests) {
      info("Affected: " + testcase.input);
    }
  }
  Assert.equal(affectedTests.length, 0);
  do_single_test_run();
  gSingleWordHostLookup = true;
  do_single_test_run();
});

function do_single_test_run() {
  Services.prefs.setBoolPref(kForceHostLookup, gSingleWordHostLookup);

  let relevantTests = gSingleWordHostLookup
    ? testcases.filter(t => t.keywordLookup)
    : testcases;

  for (let {
    input: testInput,
    fixedURI: expectedFixedURI,
    alternateURI: alternativeURI,
    keywordLookup: expectKeywordLookup,
    protocolChange: expectProtocolChange,
    inWhitelist: inWhitelist,
    affectedByDNSForSingleHosts: affectedByDNSForSingleHosts,
  } of relevantTests) {
    // Explicitly force these into a boolean
    expectKeywordLookup = !!expectKeywordLookup;
    expectProtocolChange = !!expectProtocolChange;
    inWhitelist = !!inWhitelist;
    affectedByDNSForSingleHosts = !!affectedByDNSForSingleHosts;

    expectKeywordLookup =
      expectKeywordLookup &&
      (!affectedByDNSForSingleHosts || !gSingleWordHostLookup);

    for (let flags of flagInputs) {
      let URIInfo;
      let fixupURIOnly = null;
      try {
        fixupURIOnly = Services.uriFixup.createFixupURI(testInput, flags);
      } catch (ex) {
        info("Caught exception: " + ex);
        Assert.equal(expectedFixedURI, null);
      }

      try {
        URIInfo = Services.uriFixup.getFixupURIInfo(testInput, flags);
      } catch (ex) {
        // Both APIs should return an error in the same cases.
        info("Caught exception: " + ex);
        Assert.equal(expectedFixedURI, null);
        Assert.equal(fixupURIOnly, null);
        continue;
      }

      info(
        'Checking "' +
          testInput +
          '" with flags ' +
          flags +
          " (host lookup for single words: " +
          (gSingleWordHostLookup ? "yes" : "no") +
          ")"
      );

      // Both APIs should then also be using the same spec.
      Assert.equal(!!fixupURIOnly, !!URIInfo.preferredURI);
      if (fixupURIOnly) {
        Assert.equal(
          fixupURIOnly.spec,
          URIInfo.preferredURI.spec,
          "Fixed and preferred URI should match"
        );
      }

      // Check the fixedURI:
      let makeAlternativeURI =
        flags & Services.uriFixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI;
      if (makeAlternativeURI && alternativeURI != null) {
        Assert.equal(
          URIInfo.fixedURI.spec,
          alternativeURI,
          "should have gotten alternate URI"
        );
      } else {
        Assert.equal(
          URIInfo.fixedURI && URIInfo.fixedURI.spec,
          expectedFixedURI,
          "should get correct fixed URI"
        );
      }

      // Check booleans on input:
      let couldDoKeywordLookup =
        flags & Services.uriFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
      Assert.equal(
        !!URIInfo.keywordProviderName,
        couldDoKeywordLookup && expectKeywordLookup,
        "keyword lookup as expected"
      );
      Assert.equal(
        URIInfo.fixupChangedProtocol,
        expectProtocolChange,
        "protocol change as expected"
      );
      Assert.equal(
        URIInfo.fixupCreatedAlternateURI,
        makeAlternativeURI && alternativeURI != null,
        "alternative URI as expected"
      );

      // Check the preferred URI
      if (couldDoKeywordLookup) {
        if (expectKeywordLookup) {
          if (!inWhitelist) {
            let urlparamInput = encodeURIComponent(sanitize(testInput)).replace(
              /%20/g,
              "+"
            );
            // If the input starts with `?`, then URIInfo.preferredURI.spec will omit it
            // In order to test this behaviour, remove `?` only if it is the first character
            if (urlparamInput.startsWith("%3F")) {
              urlparamInput = urlparamInput.replace("%3F", "");
            }
            let isPrivate =
              flags & Services.uriFixup.FIXUP_FLAG_PRIVATE_CONTEXT;
            let searchEngineUrl = isPrivate
              ? kPrivateSearchEngineURL
              : kSearchEngineURL;
            let searchURL = searchEngineUrl.replace(
              "{searchTerms}",
              urlparamInput
            );
            let spec = URIInfo.preferredURI.spec.replace(/%27/g, "'");
            Assert.equal(spec, searchURL, "should get correct search URI");
            let providerName = isPrivate
              ? kPrivateSearchEngineID
              : kSearchEngineID;
            Assert.equal(
              URIInfo.keywordProviderName,
              providerName,
              "should get correct provider name"
            );
            // Also check keywordToURI() uses the right engine.
            let kwInfo = Services.uriFixup.keywordToURI(
              urlparamInput,
              isPrivate
            );
            Assert.equal(kwInfo.providerName, URIInfo.providerName);
          } else {
            Assert.equal(
              URIInfo.preferredURI,
              null,
              "not expecting a preferred URI"
            );
          }
        } else {
          Assert.equal(
            URIInfo.preferredURI.spec,
            URIInfo.fixedURI.spec,
            "fixed URI should match"
          );
        }
      } else {
        // In these cases, we should never be doing a keyword lookup and
        // the fixed URI should be preferred:
        let prefURI = URIInfo.preferredURI && URIInfo.preferredURI.spec;
        let fixedURI = URIInfo.fixedURI && URIInfo.fixedURI.spec;
        Assert.equal(prefURI, fixedURI, "fixed URI should be same as expected");
      }
      Assert.equal(
        sanitize(testInput),
        URIInfo.originalInput,
        "should mirror original input"
      );
    }
  }
}
