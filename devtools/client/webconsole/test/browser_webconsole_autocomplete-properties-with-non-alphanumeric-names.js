/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that properties starting with underscores or dollars can be
// autocompleted (bug 967468).

add_task(function* () {
  const TEST_URI = "data:text/html;charset=utf8,test autocompletion with " +
                   "$ or _";
  yield loadTab(TEST_URI);

  function* autocomplete(term) {
    let deferred = promise.defer();

    jsterm.setInputValue(term);
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, deferred.resolve);

    yield deferred.promise;

    ok(popup.itemCount > 0,
       "There's " + popup.itemCount + " suggestions for '" + term + "'");
  }

  let { jsterm } = yield openConsole();
  let popup = jsterm.autocompletePopup;

  yield jsterm.execute("var testObject = {$$aaab: '', $$aaac: ''}");

  // Should work with bug 967468.
  yield autocomplete("Object.__d");
  yield autocomplete("testObject.$$a");

  // Here's when things go wrong in bug 967468.
  yield autocomplete("Object.__de");
  yield autocomplete("testObject.$$aa");

  // Should work with bug 1207868.
  yield jsterm.execute("let foobar = {a: ''}; const blargh = {a: 1};");
  yield autocomplete("foobar");
  yield autocomplete("blargh");
  yield autocomplete("foobar.a");
  yield autocomplete("blargh.a");
});
