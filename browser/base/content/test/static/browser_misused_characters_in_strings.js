/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* This list allows pre-existing or 'unfixable' issues to remain, while we
 * detect newly occurring issues in shipping files. It is a list of objects
 * specifying conditions under which an error should be ignored.
 *
 * As each issue is found in the whitelist, it is removed from the list. At
 * the end of the test, there is an assertion that all items have been
 * removed from the whitelist, thus ensuring there are no stale entries. */
let gWhitelist = [{
    file: "search.properties",
    key: "searchForSomethingWith",
    type: "single-quote"
  }, {
    file: "netError.dtd",
    key: "certerror.introPara",
    type: "single-quote"
  }, {
    file: "netError.dtd",
    key: "inadequateSecurityError.longDesc",
    type: "single-quote"
  }, {
    file: "netError.dtd",
    key: "certerror.wrongSystemTime2",
    type: "single-quote"
  }, {
    file: "netError.dtd",
    key: "certerror.wrongSystemTimeWithoutReference",
    type: "single-quote"
  }, {
    file: "phishing-afterload-warning-message.dtd",
    key: "safeb.blocked.malwarePage.shortDesc",
    type: "single-quote"
  }, {
    file: "phishing-afterload-warning-message.dtd",
    key: "safeb.blocked.unwantedPage.shortDesc",
    type: "single-quote"
  }, {
    file: "phishing-afterload-warning-message.dtd",
    key: "safeb.blocked.phishingPage.shortDesc2",
    type: "single-quote"
  }, {
    file: "mathfont.properties",
    key: "operator.\\u002E\\u002E\\u002E.postfix",
    type: "ellipsis"
  }, {
    file: "layout_errors.properties",
    key: "ImageMapRectBoundsError",
    type: "double-quote"
  }, {
    file: "layout_errors.properties",
    key: "ImageMapCircleWrongNumberOfCoords",
    type: "double-quote"
  }, {
    file: "layout_errors.properties",
    key: "ImageMapCircleNegativeRadius",
    type: "double-quote"
  }, {
    file: "layout_errors.properties",
    key: "ImageMapPolyWrongNumberOfCoords",
    type: "double-quote"
  }, {
    file: "layout_errors.properties",
    key: "ImageMapPolyOddNumberOfCoords",
    type: "double-quote"
  }, {
    file: "xbl.properties",
    key: "CommandNotInChrome",
    type: "double-quote"
  }, {
    file: "dom.properties",
    key: "PatternAttributeCompileFailure",
    type: "single-quote"
  }, {
    file: "pipnss.properties",
    key: "certErrorMismatchSingle2",
    type: "double-quote"
  }, {
    file: "pipnss.properties",
    key: "certErrorCodePrefix2",
    type: "double-quote"
  }, {
    file: "aboutSupport.dtd",
    key: "aboutSupport.pageSubtitle",
    type: "single-quote"
  }, {
    file: "aboutSupport.dtd",
    key: "aboutSupport.userJSDescription",
    type: "single-quote"
  }, {
    file: "netError.dtd",
    key: "inadequateSecurityError.longDesc",
    type: "single-quote"
  }, {
    file: "netErrorApp.dtd",
    key: "securityOverride.warningContent",
    type: "single-quote"
  }, {
    file: "pocket.properties",
    key: "tos",
    type: "double-quote"
  }, {
    file: "aboutNetworking.dtd",
    key: "aboutNetworking.logTutorial",
    type: "single-quote"
  }, {
    file: "preferences.properties",
    key: "searchResults.needHelp2",
    type: "double-quote"
  }
];

/**
 * Check if an error should be ignored due to matching one of the whitelist
 * objects defined in gWhitelist.
 *
 * @param filepath The URI spec of the locale file
 * @param key The key of the entity that is being checked
 * @param type The type of error that has been found
 * @return true if the error should be ignored, false otherwise.
 */
function ignoredError(filepath, key, type) {
  for (let index in gWhitelist) {
    let whitelistItem = gWhitelist[index];
    if (filepath.endsWith(whitelistItem.file) &&
        key == whitelistItem.key &&
        type == whitelistItem.type) {
      gWhitelist.splice(index, 1);
      return true;
    }
  }
  return false;
}

function testForError(filepath, key, str, pattern, type, helpText) {
  if (str.match(pattern) &&
      !ignoredError(filepath, key, type)) {
    ok(false, `${filepath} with key=${key} has a misused ${type}. ${helpText}`);
  }
}

function testForErrors(filepath, key, str) {
  testForError(filepath, key, str, /\w'\w/, "apostrophe", "Strings with apostrophes should use foo\u2019s instead of foo's.");
  testForError(filepath, key, str, /\w\u2018\w/, "incorrect-apostrophe", "Strings with apostrophes should use foo\u2019s instead of foo\u2018s.");
  testForError(filepath, key, str, /'.+'/, "single-quote", "Single-quoted strings should use Unicode \u2018foo\u2019 instead of 'foo'.");
  testForError(filepath, key, str, /"/, "double-quote", "Double-quoted strings should use Unicode \u201cfoo\u201d instead of \"foo\".");
  testForError(filepath, key, str, /\.\.\./, "ellipsis", "Strings with an ellipsis should use the Unicode \u2026 character instead of three periods.");
}

async function getAllTheFiles(extension) {
  let appDirGreD = Services.dirsvc.get("GreD", Ci.nsIFile);
  let appDirXCurProcD = Services.dirsvc.get("XCurProcD", Ci.nsIFile);
  if (appDirGreD.contains(appDirXCurProcD)) {
    return await generateURIsFromDirTree(appDirGreD, [extension]);
  }
  if (appDirXCurProcD.contains(appDirGreD)) {
    return await generateURIsFromDirTree(appDirXCurProcD, [extension]);
  }
  let urisGreD = await generateURIsFromDirTree(appDirGreD, [extension]);
  let urisXCurProcD = await generateURIsFromDirTree(appDirXCurProcD, [extension]);
  return Array.from(new Set(urisGreD.concat(appDirXCurProcD)));
}

add_task(async function checkAllTheProperties() {
  // This asynchronously produces a list of URLs (sadly, mostly sync on our
  // test infrastructure because it runs against jarfiles there, and
  // our zipreader APIs are all sync)
  let uris = await getAllTheFiles(".properties");
  ok(uris.length, `Found ${uris.length} .properties files to scan for misused characters`);

  for (let uri of uris) {
    let bundle = Services.strings.createBundle(uri.spec);
    let enumerator = bundle.getSimpleEnumeration();

    while (enumerator.hasMoreElements()) {
      let entity = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
      testForErrors(uri.spec, entity.key, entity.value);
    }
  }
});

var checkDTD = async function(aURISpec) {
  let rawContents = await fetchFile(aURISpec);
  // The regular expression below is adapted from:
  // https://hg.mozilla.org/mozilla-central/file/68c0b7d6f16ce5bb023e08050102b5f2fe4aacd8/python/compare-locales/compare_locales/parser.py#l233
  let entities = rawContents.match(/<!ENTITY\s+([\w\.]*)\s+("[^"]*"|'[^']*')\s*>/g);
  if (!entities) {
    // Some files have no entities defined.
    return;
  }
  for (let entity of entities) {
    let [, key, str] = entity.match(/<!ENTITY\s+([\w\.]*)\s+("[^"]*"|'[^']*')\s*>/);
    // The matched string includes the enclosing quotation marks,
    // we need to slice them off.
    str = str.slice(1, -1);
    testForErrors(aURISpec, key, str);
  }
};

add_task(async function checkAllTheDTDs() {
  let uris = await getAllTheFiles(".dtd");
  ok(uris.length, `Found ${uris.length} .dtd files to scan for misused characters`);
  for (let uri of uris) {
    await checkDTD(uri.spec);
  }

  // This support DTD file supplies a string with a newline to make sure
  // the regex in checkDTD works correctly for that case.
  let dtdLocation = gTestPath.replace(/\/[^\/]*$/i, "/bug1262648_string_with_newlines.dtd");
  await checkDTD(dtdLocation);
});

add_task(async function ensureWhiteListIsEmpty() {
  is(gWhitelist.length, 0, "No remaining whitelist entries exist");
});
