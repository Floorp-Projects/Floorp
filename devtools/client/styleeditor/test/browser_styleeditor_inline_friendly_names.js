/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that inline style sheets get correct names if they are saved to disk and
// that those names survice a reload but not navigation to another page.

const FIRST_TEST_PAGE = TEST_BASE_HTTP + "inline-1.html";
const SECOND_TEST_PAGE = TEST_BASE_HTTP + "inline-2.html";
const SAVE_PATH = "test.css";

add_task(function* () {
  let { ui } = yield openStyleEditorForURL(FIRST_TEST_PAGE);

  testIndentifierGeneration(ui);

  yield saveFirstInlineStyleSheet(ui);
  yield testFriendlyNamesAfterSave(ui);
  yield reloadPageAndWaitForStyleSheets(ui);
  yield testFriendlyNamesAfterSave(ui);
  yield navigateToAndWaitForStyleSheets(SECOND_TEST_PAGE, ui);
  yield testFriendlyNamesAfterNavigation(ui);
});

function testIndentifierGeneration(ui) {
  let fakeStyleSheetFile = {
    "href": "http://example.com/test.css",
    "nodeHref": "http://example.com/",
    "styleSheetIndex": 1
  };

  let fakeInlineStyleSheet = {
    "href": null,
    "nodeHref": "http://example.com/",
    "styleSheetIndex": 2
  };

  is(ui.getStyleSheetIdentifier(fakeStyleSheetFile),
    "http://example.com/test.css",
    "URI is the identifier of style sheet file.");

  is(ui.getStyleSheetIdentifier(fakeInlineStyleSheet),
    "inline-2-at-http://example.com/",
    "Inline sheets are identified by their page and position in the page.");
}

function saveFirstInlineStyleSheet(ui) {
  let deferred = defer();
  let editor = ui.editors[0];

  let destFile = FileUtils.getFile("ProfD", [SAVE_PATH]);

  editor.saveToFile(destFile, function (file) {
    ok(file, "File was correctly saved.");
    deferred.resolve();
  });

  return deferred.promise;
}

function testFriendlyNamesAfterSave(ui) {
  let firstEditor = ui.editors[0];
  let secondEditor = ui.editors[1];

  // The friendly name of first sheet should've been remembered, the second
  // should not be the same (bug 969900).
  is(firstEditor.friendlyName, SAVE_PATH,
    "Friendly name is correct for the saved inline style sheet.");
  isnot(secondEditor.friendlyName, SAVE_PATH,
    "Friendly name for the second inline sheet isn't the same as the first.");

  return promise.resolve(null);
}

function testFriendlyNamesAfterNavigation(ui) {
  let firstEditor = ui.editors[0];
  let secondEditor = ui.editors[1];

  // Inline style sheets shouldn't have the name of previously saved file as the
  // page is different.
  isnot(firstEditor.friendlyName, SAVE_PATH,
    "The first editor doesn't have the save path as a friendly name.");
  isnot(secondEditor.friendlyName, SAVE_PATH,
    "The second editor doesn't have the save path as a friendly name.");

  return promise.resolve(null);
}
