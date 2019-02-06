/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the UrlbarInput.autofill() method.  It needs to be tested in the actual
 * browser because UrlbarInput relies on the trimURL() function defined in the
 * browser window's global scope, and this test hits the path that calls it.
 */

"use strict";

add_task(async function test() {
  let tests = [
    // [initial value, autofill value, expected selected substring in new value]
    ["foo", "foobar", "bar"],
    ["FOO", "foobar", "bar"],
    ["fOo", "foobar", "bar"],
    ["foo", "quuxbar", ""],
    ["FOO", "quuxbar", ""],
    ["fOo", "quuxbar", ""],
  ];

  gURLBar.focus();
  for (let [initial, autofill, expectedSelected] of tests) {
    gURLBar.value = initial;
    gURLBar.autofill(autofill);
    let expectedValue = initial + expectedSelected;
    Assert.equal(gURLBar.value, expectedValue,
      "The input value should be correct");
    Assert.equal(gURLBar.selectionStart, initial.length,
      "The start of the selection should be correct");
    Assert.equal(gURLBar.selectionEnd, expectedValue.length,
      "The end of the selection should be correct");
  }
});
