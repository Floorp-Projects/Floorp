let urifixup = Cc["@mozilla.org/docshell/urifixup;1"].
               getService(Ci.nsIURIFixup);
Components.utils.import("resource://gre/modules/Services.jsm");

Services.prefs.setBoolPref("keyword.enabled", true);

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
});

let data = [
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
];


function run_test() {
  run_next_test();
}

let len = data.length;
// Make sure we fix what needs fixing
add_task(function test_fix_unknown_schemes() {
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let result =
      urifixup.createFixupURI(item.wrong,
                              urifixup.FIXUP_FLAG_FIX_SCHEME_TYPOS).spec;
    do_check_eq(result, item.fixed);
  }
});
