/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

// Test that properties starting with underscores or dollars can be
// autocompleted (bug 967468).

var test = asyncTest(function*() {
  const TEST_URI = "data:text/html;charset=utf8,test autocompletion with " +
                   "$ or _";
  yield loadTab(TEST_URI);

  function* autocomplete(term) {
    let deferred = promise.defer();

    jsterm.setInputValue(term);
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, deferred.resolve);

    yield deferred.promise;

    ok(popup.itemCount > 0, "There's suggestions for '" + term + "'");
  }

  let { jsterm } = yield openConsole();
  let popup = jsterm.autocompletePopup;

  yield jsterm.execute("let testObject = {$$aaab: '', $$aaac: ''}");

  // Should work with bug 967468.
  yield autocomplete("Object.__d");
  yield autocomplete("testObject.$$a");

  // Here's when things go wrong in bug 967468.
  yield autocomplete("Object.__de");
  yield autocomplete("testObject.$$aa");
});
