var urifixup = Cc["@mozilla.org/docshell/urifixup;1"].
               getService(Ci.nsIURIFixup);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);

var pref = "browser.fixup.typo.scheme";

var data = [
  {
    // ttp -> http.
    wrong: 'ttp://www.example.com/',
    fixed: 'http://www.example.com/',
  },
  {
    // ttps -> https.
    wrong: 'ttps://www.example.com/',
    fixed: 'https://www.example.com/',
  },
  {
    // tps -> https.
    wrong: 'tps://www.example.com/',
    fixed: 'https://www.example.com/',
  },
  {
    // ps -> https.
    wrong: 'ps://www.example.com/',
    fixed: 'https://www.example.com/',
  },
  {
    // ile -> file.
    wrong: 'ile:///this/is/a/test.html',
    fixed: 'file:///this/is/a/test.html',
  },
  {
    // le -> file.
    wrong: 'le:///this/is/a/test.html',
    fixed: 'file:///this/is/a/test.html',
  },
  {
    // Valid should not be changed.
    wrong: 'https://example.com/this/is/a/test.html',
    fixed: 'https://example.com/this/is/a/test.html',
  },
  {
    // Unmatched should not be changed.
    wrong: 'whatever://this/is/a/test.html',
    fixed: 'whatever://this/is/a/test.html',
  },
];

var len = data.length;

function run_test() {
  run_next_test();
}

// Make sure we fix what needs fixing when there is no pref set.
add_task(function test_unset_pref_fixes_typos() {
  prefs.clearUserPref(pref);
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let result =
      urifixup.createFixupURI(item.wrong,
                              urifixup.FIXUP_FLAG_FIX_SCHEME_TYPOS).spec;
    do_check_eq(result, item.fixed);
  }
});
  
// Make sure we don't do anything when the pref is explicitly
// set to false.
add_task(function test_false_pref_keeps_typos() {
  prefs.setBoolPref(pref, false);
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let result =
      urifixup.createFixupURI(item.wrong,
                              urifixup.FIXUP_FLAG_FIX_SCHEME_TYPOS).spec;
    do_check_eq(result, item.wrong);
  }
});

// Finally, make sure we still fix what needs fixing if the pref is
// explicitly set to true.
add_task(function test_true_pref_fixes_typos() {
  prefs.setBoolPref(pref, true);
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let result =
        urifixup.createFixupURI(item.wrong,
                                urifixup.FIXUP_FLAG_FIX_SCHEME_TYPOS).spec;
    do_check_eq(result, item.fixed);
  }
});
