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
  ["http://www.mozilla.org", "http://www.mozilla.org/"],
  ["://www.mozilla.org", "http://www.mozilla.org/"],
  ["www.mozilla.org", "http://www.mozilla.org/"],
  ["http://mozilla/", "http://mozilla/"],
  ["127.0.0.1", "http://127.0.0.1/"],
  ["1234", "http://1234/"],
  ["host/foo.txt", "http://host/foo.txt"],
  ["mozilla", "http://mozilla/"],
  ["mozilla is amazing", null],
  ["", null],
];

if (Services.appinfo.OS.toLowerCase().startsWith("win")) {
  testcases.push(["C:\\some\\file.txt", "file:///C:/some/file.txt"]);
} else {
  testcases.push(["/some/file.txt", "file:///some/file.txt"]);
}

function run_test() {
  for (let [testInput, expectedFixedURI] of testcases) {
    for (let flags of flagInputs) {
      let info;
      let fixupURIOnly = null;
      try {
        fixupURIOnly = urifixup.createFixupURI(testInput, flags);
      } catch (ex) {
        do_check_eq(expectedFixedURI, null);
      }

      try {
        info = urifixup.getFixupURIInfo(testInput, flags);
      } catch (ex) {
        // Both APIs should return an error in the same cases.
        do_check_eq(expectedFixedURI, null);
        do_check_eq(fixupURIOnly, null);
        continue;
      }

      // Both APIs should then also be using the same spec.
      do_check_eq(fixupURIOnly.spec, info.preferredURI.spec);

      let isFileURL = expectedFixedURI && expectedFixedURI.startsWith("file");

      // Check the fixedURI:
      let alternateURI = flags & urifixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI;
      if (!isFileURL && alternateURI && !info.inputHostHasDot && info.fixedURI) {
        let originalURI = Services.io.newURI(expectedFixedURI, null, null);
        do_check_eq(info.fixedURI.host, "www." + originalURI.host + ".com");
      } else {
        do_check_eq(info.fixedURI && info.fixedURI.spec, expectedFixedURI);
      }

      // Check booleans on input:
      if (isFileURL) {
        do_check_eq(info.inputHasProtocol, testInput.startsWith("file:"));
        do_check_eq(info.inputHostHasDot, false);
      } else {
        // The duff protocol doesn't count, so > 0 rather than -1:
        do_check_eq(info.inputHasProtocol, testInput.indexOf(":") > 0);
        let dotIndex = testInput.indexOf(".");
        let slashIndex = testInput.replace("://", "").indexOf("/");
        slashIndex = slashIndex == -1 ? testInput.length : slashIndex;
        do_check_eq(info.inputHostHasDot, dotIndex != -1 && slashIndex > dotIndex);
      }

      let couldDoKeywordLookup = flags & urifixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
      // Check the preferred URI
      if (info.inputHostHasDot || info.inputHasProtocol) {
        // In these cases, we should never be doing a keyword lookup and
        // the fixed URI should be preferred:
        do_check_eq(info.preferredURI.spec, info.fixedURI.spec);
      } else if (!isFileURL && couldDoKeywordLookup && testInput.indexOf(".") == -1) {
        // Otherwise, and assuming we're allowed, there will be a search URI:
        let urlparamInput = testInput.replace(/ /g, '+');
        let searchURL = kSearchEngineURL.replace("{searchTerms}", urlparamInput);
        do_check_eq(info.preferredURI.spec, searchURL);
      } else if (info.fixedURI) {
        // This is for lack of keyword lookup, combined with hostnames with no
        // protocol:
        do_check_eq(info.fixedURI, info.preferredURI);
        if (isFileURL) {
          do_check_eq(info.fixedURI.host, "");
        } else {
          let hostMatch = testInput.match(/(?:[^:\/]*:\/\/)?([^\/]+)(\/|$)/);
          let host = hostMatch ? hostMatch[1] : "";
          if (alternateURI) {
            do_check_eq(info.fixedURI.host, "www." + host + ".com");
          } else {
            do_check_eq(info.fixedURI.host, host);
          }
        }
      } else {
        do_check_true(false, "There should be no cases where we got here, " +
                             "there's no keyword lookup, and no fixed URI." +
                             "Offending input: " + testInput);
      }
      do_check_eq(testInput, info.originalInput);
    }
  }
}
