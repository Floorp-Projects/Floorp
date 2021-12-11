const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const kForceDNSLookup = "browser.fixup.dns_first_for_single_words";

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
    affectedByDNSForSingleWordHosts: false, // Whether the input host could be a host, but is normally assumed to be a keyword query
  }
*/
var testcases = [
  {
    input: "about:home",
    fixedURI: "about:home",
  },
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
    alternateURI: "https://www.mozilla.com/",
  },
  {
    input: "http://test./",
    fixedURI: "http://test./",
    alternateURI: "https://www.test./",
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
    protocolChange: true,
  },
  {
    input: "[::1]/",
    fixedURI: "http://[::1]/",
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
  },
  {
    input: "[fe80:cd00:0:cde:1257:0:211e:729c]",
    fixedURI: "http://[fe80:cd00:0:cde:1257:0:211e:729c]/",
    protocolChange: true,
  },
  {
    input: "[64:ff9b::8.8.8.8]",
    fixedURI: "http://[64:ff9b::808:808]/",
    protocolChange: true,
  },
  {
    input: "[64:ff9b::8.8.8.8]/~moz",
    fixedURI: "http://[64:ff9b::808:808]/~moz",
    protocolChange: true,
  },
  {
    input: "[::1][::1]",
    keywordLookup: true,
  },
  {
    input: "[::1][100",
    keywordLookup: true,
  },
  {
    input: "[::1]]",
    keywordLookup: true,
  },
  {
    input: "1234",
    fixedURI: "http://0.0.4.210/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "whitelisted/foo.txt",
    fixedURI: "http://whitelisted/foo.txt",
    alternateURI: "https://www.whitelisted.com/foo.txt",
    protocolChange: true,
  },
  {
    input: "mozilla",
    fixedURI: "http://mozilla/",
    alternateURI: "https://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "test.",
    fixedURI: "http://test./",
    alternateURI: "https://www.test./",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: ".test",
    fixedURI: "http://.test/",
    alternateURI: "https://www.test/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "mozilla is amazing",
    keywordLookup: true,
  },
  {
    input: "search ?mozilla",
    keywordLookup: true,
  },
  {
    input: "mozilla .com",
    keywordLookup: true,
  },
  {
    input: "what if firefox?",
    keywordLookup: true,
  },
  {
    input: "london's map",
    keywordLookup: true,
  },
  {
    input: "mozilla ",
    fixedURI: "http://mozilla/",
    alternateURI: "https://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "   mozilla  ",
    fixedURI: "http://mozilla/",
    alternateURI: "https://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "mozilla \\",
    keywordLookup: true,
  },
  {
    input: "mozilla \\ foo.txt",
    keywordLookup: true,
  },
  {
    input: "mozilla \\\r foo.txt",
    keywordLookup: true,
  },
  {
    input: "mozilla\n",
    fixedURI: "http://mozilla/",
    alternateURI: "https://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "mozilla \r\n",
    fixedURI: "http://mozilla/",
    alternateURI: "https://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "moz\r\nfirefox\nos\r",
    fixedURI: "http://mozfirefoxos/",
    alternateURI: "https://www.mozfirefoxos.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "moz\r\n firefox\n",
    keywordLookup: true,
  },
  {
    input: "",
    keywordLookup: true,
  },
  {
    input: "[]",
    keywordLookup: true,
  },
  {
    input: "http://whitelisted/",
    fixedURI: "http://whitelisted/",
    alternateURI: "https://www.whitelisted.com/",
    inWhitelist: true,
  },
  {
    input: "whitelisted",
    fixedURI: "http://whitelisted/",
    alternateURI: "https://www.whitelisted.com/",
    protocolChange: true,
    inWhitelist: true,
  },
  {
    input: "whitelisted.",
    fixedURI: "http://whitelisted./",
    alternateURI: "https://www.whitelisted./",
    protocolChange: true,
    inWhitelist: true,
  },
  {
    input: "mochi.test",
    fixedURI: "http://mochi.test/",
    alternateURI: "https://www.mochi.test/",
    protocolChange: true,
    inWhitelist: true,
  },
  // local.domain is a whitelisted suffix...
  {
    input: "some.local.domain",
    fixedURI: "http://some.local.domain/",
    protocolChange: true,
    inWhitelist: true,
  },
  // ...but .domain is not.
  {
    input: "some.domain",
    fixedURI: "http://some.domain/",
    alternateURI: "https://www.some.domain/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "café.com",
    fixedURI: "http://xn--caf-dma.com/",
    alternateURI: "https://www.xn--caf-dma.com/",
    protocolChange: true,
  },
  {
    input: "mozilla.nonexistent",
    fixedURI: "http://mozilla.nonexistent/",
    alternateURI: "https://www.mozilla.nonexistent/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "mochi.ocm",
    fixedURI: "http://mochi.com/",
    alternateURI: "https://www.mochi.com/",
    protocolChange: true,
  },
  {
    input: "47.6182,-122.830",
    fixedURI: "http://47.6182,-122.830/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "-47.6182,-23.51",
    fixedURI: "http://-47.6182,-23.51/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "-22.14,23.51-",
    fixedURI: "http://-22.14,23.51-/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "32.7",
    fixedURI: "http://32.0.0.7/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "5+2",
    fixedURI: "http://5+2/",
    alternateURI: "https://www.5+2.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "5/2",
    fixedURI: "http://0.0.0.5/2",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "moz ?.::%27",
    keywordLookup: true,
  },
  {
    input: "mozilla.com/?q=search",
    fixedURI: "http://mozilla.com/?q=search",
    alternateURI: "https://www.mozilla.com/?q=search",
    protocolChange: true,
  },
  {
    input: "mozilla.com?q=search",
    fixedURI: "http://mozilla.com/?q=search",
    alternateURI: "https://www.mozilla.com/?q=search",
    protocolChange: true,
  },
  {
    input: "mozilla.com ?q=search",
    keywordLookup: true,
  },
  {
    input: "mozilla.com.?q=search",
    fixedURI: "http://mozilla.com./?q=search",
    protocolChange: true,
  },
  {
    input: "mozilla.com'?q=search",
    fixedURI: "http://mozilla.com/?q=search",
    alternateURI: "https://www.mozilla.com/?q=search",
    protocolChange: true,
  },
  {
    input: "mozilla.com':search",
    keywordLookup: true,
  },
  {
    input: "[mozilla]",
    keywordLookup: true,
  },
  {
    input: "':?",
    fixedURI: "http://'/?",
    alternateURI: "https://www.'.com/?",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "whitelisted?.com",
    fixedURI: "http://whitelisted/?.com",
    alternateURI: "https://www.whitelisted.com/?.com",
    protocolChange: true,
  },
  {
    input: "?'.com",
    keywordLookup: true,
  },
  {
    input: "' ?.com",
    keywordLookup: true,
  },
  {
    input: "?mozilla",
    keywordLookup: true,
  },
  {
    input: "??mozilla",
    keywordLookup: true,
  },
  {
    input: "mozilla/",
    fixedURI: "http://mozilla/",
    alternateURI: "https://www.mozilla.com/",
    protocolChange: true,
  },
  {
    input: "mozilla",
    fixedURI: "http://mozilla/",
    alternateURI: "https://www.mozilla.com/",
    protocolChange: true,
    keywordLookup: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "mozilla5/2",
    fixedURI: "http://mozilla5/2",
    alternateURI: "https://www.mozilla5.com/2",
    protocolChange: true,
    keywordLookup: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "mozilla/foo",
    fixedURI: "http://mozilla/foo",
    alternateURI: "https://www.mozilla.com/foo",
    protocolChange: true,
    keywordLookup: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "mozilla\\",
    fixedURI: "http://mozilla/",
    alternateURI: "https://www.mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "localhost",
    fixedURI: "http://localhost/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "http",
    fixedURI: "http://http/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "https",
    fixedURI: "http://https/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
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
    input: "plonk:8080?test",
    fixedURI: "http://plonk:8080/?test",
    protocolChange: true,
  },
  {
    input: "plonk:8080#test",
    fixedURI: "http://plonk:8080/#test",
    protocolChange: true,
  },
  {
    input: "plonk/ #",
    fixedURI: "http://plonk/%20#",
    alternateURI: "https://www.plonk.com/%20#",
    protocolChange: true,
    keywordLookup: false,
  },
  {
    input: "blah.com.",
    fixedURI: "http://blah.com./",
    protocolChange: true,
  },
  {
    input:
      "\u10E0\u10D4\u10D2\u10D8\u10E1\u10E2\u10E0\u10D0\u10EA\u10D8\u10D0.\u10D2\u10D4",
    fixedURI: "http://xn--lodaehvb5cdik4g.xn--node/",
    alternateURI: "https://www.xn--lodaehvb5cdik4g.xn--node/",
    protocolChange: true,
  },
  {
    input: " \t mozilla.org/\t \t ",
    fixedURI: "http://mozilla.org/",
    alternateURI: "https://www.mozilla.org/",
    protocolChange: true,
  },
  {
    input: " moz\ti\tlla.org ",
    keywordLookup: true,
  },
  {
    input: "mozilla/",
    fixedURI: "http://mozilla/",
    alternateURI: "https://www.mozilla.com/",
    protocolChange: true,
  },
  {
    input: "mozilla/ test /",
    fixedURI: "http://mozilla/%20test%20/",
    alternateURI: "https://www.mozilla.com/%20test%20/",
    protocolChange: true,
  },
  {
    input: "mozilla /test/",
    keywordLookup: true,
  },
  {
    input: "pserver:8080",
    fixedURI: "http://pserver:8080/",
    protocolChange: true,
  },
  {
    input: "http;mozilla",
    fixedURI: "http://http;mozilla/",
    alternateURI: "https://www.http;mozilla.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
  },
  {
    input: "http//mozilla.org",
    fixedURI: "http://mozilla.org/",
    shouldRunTest: flags =>
      flags & Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS,
  },
  {
    input: "http//mozilla.org",
    fixedURI: "http://http//mozilla.org",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
    shouldRunTest: flags =>
      !(flags & Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS),
  },
  {
    input: "www.mozilla",
    fixedURI: "http://www.mozilla/",
    protocolChange: true,
  },
  {
    input: "https://sub.www..mozilla...org/",
    fixedURI: "https://sub.www.mozilla.org/",
  },
  {
    input: "sub.www..mozilla...org/",
    fixedURI: "http://sub.www.mozilla.org/",
    protocolChange: true,
  },
  {
    input: "sub.www..mozilla...org",
    fixedURI: "http://sub.www.mozilla.org/",
    protocolChange: true,
  },
  {
    input: "www...mozilla",
    fixedURI: "http://www.mozilla/",
    keywordLookup: true,
    protocolChange: true,
    shouldRunTest: flags =>
      !gSingleWordDNSLookup &&
      flags & Services.uriFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP,
  },
  {
    input: "www...mozilla",
    fixedURI: "http://www.mozilla/",
    protocolChange: true,
    shouldRunTest: flags =>
      gSingleWordDNSLookup ||
      !(flags & Services.uriFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP),
  },
  {
    input: "mozilla...org",
    fixedURI: "http://mozilla.org/",
    protocolChange: true,
  },
  {
    input: "モジラ...org",
    fixedURI: "http://xn--yck6dwa.org/",
    protocolChange: true,
  },
  {
    input: "user@localhost",
    fixedURI: "http://user@localhost/",
    protocolChange: true,
    shouldRunTest: () => gSingleWordDNSLookup,
  },
  {
    input: "user@localhost",
    fixedURI: "http://user@localhost/",
    keywordLookup: true,
    protocolChange: true,
    shouldRunTest: () => !gSingleWordDNSLookup,
  },
  {
    input: "user@192.168.0.1",
    fixedURI: "http://user@192.168.0.1/",
    protocolChange: true,
  },
  {
    input: "user@dummy-host",
    fixedURI: "http://user@dummy-host/",
    protocolChange: true,
    shouldRunTest: () => gSingleWordDNSLookup,
  },
  {
    input: "user@dummy-host",
    fixedURI: "http://user@dummy-host/",
    keywordLookup: true,
    protocolChange: true,
    shouldRunTest: () => !gSingleWordDNSLookup,
  },
  {
    input: "user:pass@dummy-host",
    fixedURI: "http://user:pass@dummy-host/",
    protocolChange: true,
  },
  {
    input: ":pass@dummy-host",
    fixedURI: "http://:pass@dummy-host/",
    protocolChange: true,
  },
  {
    input: "user@dummy-host/path",
    fixedURI: "http://user@dummy-host/path",
    protocolChange: true,
    shouldRunTest: () => gSingleWordDNSLookup,
  },
  {
    input: "jar:file:///omni.ja!/",
    fixedURI: "jar:file:///omni.ja!/",
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
    alternateURI: "https://www.mozilla.com/",
    protocolChange: true,
  });
  testcases.push({
    input: "/a",
    fixedURI: "http://a/",
    alternateURI: "https://www.a.com/",
    keywordLookup: true,
    protocolChange: true,
    affectedByDNSForSingleWordHosts: true,
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
    input: "/a",
    fixedURI: "file:///a",
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
    "browser.fixup.domainsuffixwhitelist.test",
    "browser.fixup.domainsuffixwhitelist.local.domain",
    "browser.search.separatePrivateDefault",
    "browser.search.separatePrivateDefault.ui.enabled",
  ];
  for (let pref of prefList) {
    Services.prefs.setBoolPref(pref, true);
  }

  await setupSearchService();
  await addTestEngines();

  await Services.search.setDefault(
    Services.search.getEngineByName(kSearchEngineID)
  );
  await Services.search.setDefaultPrivate(
    Services.search.getEngineByName(kPrivateSearchEngineID)
  );
});

var gSingleWordDNSLookup = false;
add_task(async function run_test() {
  // Only keywordlookup things should be affected by requiring a DNS lookup for single-word hosts:
  info(
    "Check only keyword lookup testcases should be affected by requiring DNS for single hosts"
  );
  let affectedTests = testcases.filter(
    t => !t.keywordLookup && t.affectedByDNSForSingleWordHosts
  );
  if (affectedTests.length) {
    for (let testcase of affectedTests) {
      info("Affected: " + testcase.input);
    }
  }
  Assert.equal(affectedTests.length, 0);
  await do_single_test_run();
  gSingleWordDNSLookup = true;
  await do_single_test_run();
  gSingleWordDNSLookup = false;
  await Services.search.setDefault(
    Services.search.getEngineByName(kPostSearchEngineID)
  );
  await do_single_test_run();
});

async function do_single_test_run() {
  Services.prefs.setBoolPref(kForceDNSLookup, gSingleWordDNSLookup);

  let relevantTests = gSingleWordDNSLookup
    ? testcases.filter(t => t.keywordLookup)
    : testcases;

  let engine = await Services.search.getDefault();
  let engineUrl =
    engine.name == kPostSearchEngineID
      ? kPostSearchEngineURL
      : kSearchEngineURL;
  let privateEngine = await Services.search.getDefaultPrivate();
  let privateEngineUrl = kPrivateSearchEngineURL;

  for (let {
    input: testInput,
    fixedURI: expectedFixedURI,
    alternateURI: alternativeURI,
    keywordLookup: expectKeywordLookup,
    protocolChange: expectProtocolChange,
    inWhitelist: inWhitelist,
    affectedByDNSForSingleWordHosts: affectedByDNSForSingleWordHosts,
    shouldRunTest,
  } of relevantTests) {
    // Explicitly force these into a boolean
    expectKeywordLookup = !!expectKeywordLookup;
    expectProtocolChange = !!expectProtocolChange;
    inWhitelist = !!inWhitelist;
    affectedByDNSForSingleWordHosts = !!affectedByDNSForSingleWordHosts;

    expectKeywordLookup =
      expectKeywordLookup &&
      (!affectedByDNSForSingleWordHosts || !gSingleWordDNSLookup);

    for (let flags of flagInputs) {
      info(
        'Checking "' +
          testInput +
          '" with flags ' +
          flags +
          " (DNS lookup for single words: " +
          (gSingleWordDNSLookup ? "yes" : "no") +
          ")"
      );

      if (shouldRunTest && !shouldRunTest(flags)) {
        continue;
      }

      let URIInfo;
      try {
        URIInfo = Services.uriFixup.getFixupURIInfo(testInput, flags);
      } catch (ex) {
        // Both APIs should return an error in the same cases.
        info("Caught exception: " + ex);
        Assert.equal(expectedFixedURI, null);
        continue;
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
            let searchEngineUrl = isPrivate ? privateEngineUrl : engineUrl;
            let searchURL = searchEngineUrl.replace(
              "{searchTerms}",
              urlparamInput
            );
            let spec = URIInfo.preferredURI.spec.replace(/%27/g, "'");
            Assert.equal(spec, searchURL, "should get correct search URI");
            let providerName = isPrivate ? privateEngine.name : engine.name;
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
            if (providerName == kPostSearchEngineID) {
              Assert.ok(kwInfo.postData);
              let submission = engine.getSubmission(urlparamInput);
              let enginePostData = NetUtil.readInputStreamToString(
                submission.postData,
                submission.postData.available()
              );
              let postData = NetUtil.readInputStreamToString(
                kwInfo.postData,
                kwInfo.postData.available()
              );
              Assert.equal(postData, enginePostData);
            }
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
