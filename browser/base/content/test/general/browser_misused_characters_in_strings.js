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
    file: "browser.dtd",
    key: "social.activated.description",
    type: "single-quote"
  }, {
    file: "netError.dtd",
    key: "certerror.introPara",
    type: "single-quote"
  }, {
    file: "netError.dtd",
    key: "weakCryptoAdvanced.longDesc",
    type: "single-quote"
  }, {
    file: "netError.dtd",
    key: "weakCryptoAdvanced.override",
    type: "single-quote"
  }, {
    file: "netError.dtd",
    key: "inadequateSecurityError.longDesc",
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
    file: "phishing-afterload-warning-message.dtd",
    key: "safeb.blocked.forbiddenPage.shortDesc2",
    type: "single-quote"
  }
];

var moduleLocation = gTestPath.replace(/\/[^\/]*$/i, "/parsingTestHelpers.jsm");
var {generateURIsFromDirTree} = Cu.import(moduleLocation, {});

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

function fetchFile(uri) {
  return new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", uri, true);
    xhr.onreadystatechange = function() {
      if (this.readyState != this.DONE) {
        return;
      }
      try {
        resolve(this.responseText);
      } catch (ex) {
        ok(false, `Script error reading ${uri}: ${ex}`);
        resolve("");
      }
    };
    xhr.onerror = error => {
      ok(false, `XHR error reading ${uri}: ${error}`);
      resolve("");
    };
    xhr.send(null);
  });
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

add_task(function* checkAllTheProperties() {
  let appDir = Services.dirsvc.get("XCurProcD", Ci.nsIFile);
  // This asynchronously produces a list of URLs (sadly, mostly sync on our
  // test infrastructure because it runs against jarfiles there, and
  // our zipreader APIs are all sync)
  let uris = yield generateURIsFromDirTree(appDir, [".properties"]);
  ok(uris.length, `Found ${uris.length} .properties files to scan for misused characters`);

  for (let uri of uris) {
    let bundle = new StringBundle(uri.spec);
    let entities = bundle.getAll();
    for (let entity of entities) {
      testForErrors(uri.spec, entity.key, entity.value);
    }
  }
});

var checkDTD = Task.async(function* (aURISpec) {
  let rawContents = yield fetchFile(aURISpec);
  // The regular expression below is adapted from:
  // https://hg.mozilla.org/mozilla-central/file/68c0b7d6f16ce5bb023e08050102b5f2fe4aacd8/python/compare-locales/compare_locales/parser.py#l233
  let entities = rawContents.match(/<!ENTITY\s+([\w\.]*)\s+("[^"]*"|'[^']*')\s*>/g);
  for (let entity of entities) {
    let [, key, str] = entity.match(/<!ENTITY\s+([\w\.]*)\s+("[^"]*"|'[^']*')\s*>/);
    // The matched string includes the enclosing quotation marks,
    // we need to slice them off.
    str = str.slice(1, -1);
    testForErrors(aURISpec, key, str);
  }
});

add_task(function* checkAllTheDTDs() {
  let appDir = Services.dirsvc.get("XCurProcD", Ci.nsIFile);
  let uris = yield generateURIsFromDirTree(appDir, [".dtd"]);
  ok(uris.length, `Found ${uris.length} .dtd files to scan for misused characters`);
  for (let uri of uris) {
    yield checkDTD(uri.spec);
  }

  // This support DTD file supplies a string with a newline to make sure
  // the regex in checkDTD works correctly for that case.
  let dtdLocation = gTestPath.replace(/\/[^\/]*$/i, "/bug1262648_string_with_newlines.dtd");
  yield checkDTD(dtdLocation);
});

add_task(function* ensureWhiteListIsEmpty() {
  is(gWhitelist.length, 0, "No remaining whitelist entries exist");
});
