let urifixup = Cc["@mozilla.org/docshell/urifixup;1"].
               getService(Ci.nsIURIFixup);

Components.utils.import("resource://gre/modules/Services.jsm");

let prefList = ["browser.fixup.typo.scheme", "keyword.enabled"];
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
  urifixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
];

flagInputs.concat([
  flagInputs[0] | flagInputs[1],
  flagInputs[1] | flagInputs[2],
  flagInputs[0] | flagInputs[2],
  flagInputs[0] | flagInputs[1] | flagInputs[2]
]);

let testcases = [
  ["http://www.mozilla.org", "http://www.mozilla.org/", null, false, false],
  ["http://127.0.0.1/", "http://127.0.0.1/", null, false, false],
  ["file:///foo/bar", "file:///foo/bar", null, false, false],
  ["://www.mozilla.org", "http://www.mozilla.org/", null, false, true],
  ["www.mozilla.org", "http://www.mozilla.org/", null, false, true],
  ["http://mozilla/", "http://mozilla/", "http://www.mozilla.com/", false, false],
  ["http://test./", "http://test./", "http://www.test./", false, false],
  ["127.0.0.1", "http://127.0.0.1/", null, false, true],
  ["1234", "http://1234/", "http://www.1234.com/", true, true],
  ["host/foo.txt", "http://host/foo.txt", "http://www.host.com/foo.txt", false, true],
  ["mozilla", "http://mozilla/", "http://www.mozilla.com/", true, true],
  ["test.", "http://test./", "http://www.test./", true, true],
  [".test", "http://.test/", "http://www..test/", true, true],
  ["mozilla is amazing", null, null, true, true],
  ["", null, null, true, true],
  ["[]", null, null, true, true]
];

if (Services.appinfo.OS.toLowerCase().startsWith("win")) {
  testcases.push(["C:\\some\\file.txt", "file:///C:/some/file.txt", null, false, true]);
  testcases.push(["//mozilla", "http://mozilla/", "http://www.mozilla.com/", false, true]);
} else {
  testcases.push(["/some/file.txt", "file:///some/file.txt", null, false, true]);
  testcases.push(["//mozilla", "file:////mozilla", null, false, true]);
}

function run_test() {
  for (let [testInput, expectedFixedURI, alternativeURI,
            expectKeywordLookup, expectProtocolChange] of testcases) {
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

      do_print("Checking " + testInput + " with flags " + flags);

      // Both APIs should then also be using the same spec.
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
      if (couldDoKeywordLookup && expectKeywordLookup) {
        let urlparamInput = encodeURIComponent(testInput).replace("%20", "+", "g");
        let searchURL = kSearchEngineURL.replace("{searchTerms}", urlparamInput);
        do_check_eq(info.preferredURI.spec, searchURL);
      } else {
        // In these cases, we should never be doing a keyword lookup and
        // the fixed URI should be preferred:
        do_check_eq(info.preferredURI.spec, info.fixedURI.spec);
      }
      do_check_eq(testInput, info.originalInput);
    }
  }
}
