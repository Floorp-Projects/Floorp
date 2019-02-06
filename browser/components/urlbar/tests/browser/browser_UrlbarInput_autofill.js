/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the autofill functionality of UrlbarInput.
 */

"use strict";

add_task(async function test() {
  gURLBar.setValueFromResult({
    autofill: {
      value: "foobar",
      selectionStart: "foo".length,
      selectionEnd: "foobar".length,
    },
    type: UrlbarUtils.RESULT_TYPE.URL,
  });
  Assert.equal(gURLBar.value, "foobar",
    "The input value should be correct");
  Assert.equal(gURLBar.selectionStart, "foo".length,
    "The start of the selection should be correct");
  Assert.equal(gURLBar.selectionEnd, "foobar".length,
    "The end of the selection should be correct");
});
