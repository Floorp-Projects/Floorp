var pref = "browser.fixup.typo.scheme";

var data = [
  {
    // ttp -> http.
    wrong: "ttp://www.example.com/",
    fixed: "http://www.example.com/",
  },
  {
    // htp -> http.
    wrong: "htp://www.example.com/",
    fixed: "http://www.example.com/",
  },
  {
    // ttps -> https.
    wrong: "ttps://www.example.com/",
    fixed: "https://www.example.com/",
  },
  {
    // tps -> https.
    wrong: "tps://www.example.com/",
    fixed: "https://www.example.com/",
  },
  {
    // ps -> https.
    wrong: "ps://www.example.com/",
    fixed: "https://www.example.com/",
  },
  {
    // htps -> https.
    wrong: "htps://www.example.com/",
    fixed: "https://www.example.com/",
  },
  {
    // ile -> file.
    wrong: "ile:///this/is/a/test.html",
    fixed: "file:///this/is/a/test.html",
  },
  {
    // le -> file.
    wrong: "le:///this/is/a/test.html",
    fixed: "file:///this/is/a/test.html",
  },
  {
    // Replace ';' with ':'.
    wrong: "http;//www.example.com/",
    fixed: "http://www.example.com/",
    noPrefValue: "http://http;//www.example.com/",
  },
  {
    // Missing ':'.
    wrong: "https//www.example.com/",
    fixed: "https://www.example.com/",
    noPrefValue: "http://https//www.example.com/",
  },
  {
    // Missing ':' for file scheme.
    wrong: "file///this/is/a/test.html",
    fixed: "file:///this/is/a/test.html",
    noPrefValue: "http://file///this/is/a/test.html",
  },
  {
    // Valid should not be changed.
    wrong: "https://example.com/this/is/a/test.html",
    fixed: "https://example.com/this/is/a/test.html",
  },
  {
    // Unmatched should not be changed.
    wrong: "whatever://this/is/a/test.html",
    fixed: "whatever://this/is/a/test.html",
  },
];

var len = data.length;

add_task(async function setup() {
  await setupSearchService();
  // Now we've initialised the search service, we force remove the engines
  // it has, so they don't interfere with this test.
  // Search engine integration is tested in test_URIFixup_search.js.
  Services.search.wrappedJSObject._engines.clear();
});

// Make sure we fix what needs fixing when there is no pref set.
add_task(function test_unset_pref_fixes_typos() {
  Services.prefs.clearUserPref(pref);
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let { preferredURI } = Services.uriFixup.getFixupURIInfo(
      item.wrong,
      Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
    );
    Assert.equal(preferredURI.spec, item.fixed);
  }
});

// Make sure we don't do anything when the pref is explicitly
// set to false.
add_task(function test_false_pref_keeps_typos() {
  Services.prefs.setBoolPref(pref, false);
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let { preferredURI } = Services.uriFixup.getFixupURIInfo(
      item.wrong,
      Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
    );
    Assert.equal(preferredURI.spec, item.noPrefValue || item.wrong);
  }
});

// Finally, make sure we still fix what needs fixing if the pref is
// explicitly set to true.
add_task(function test_true_pref_fixes_typos() {
  Services.prefs.setBoolPref(pref, true);
  for (let i = 0; i < len; ++i) {
    let item = data[i];
    let { preferredURI } = Services.uriFixup.getFixupURIInfo(
      item.wrong,
      Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
    );
    Assert.equal(preferredURI.spec, item.fixed);
  }
});
