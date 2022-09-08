/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function checkArrayIsSorted(array, msg) {
  let sorted = true;
  let sortedArray = array.slice().sort(function(a, b) {
    return a.localeCompare(b);
  });

  for (let i = 0; i < array.length; i++) {
    if (array[i] != sortedArray[i]) {
      sorted = false;
      break;
    }
  }
  ok(sorted, msg);
}

add_task(async function test_policies_sorted() {
  let { schema } = ChromeUtils.import(
    "resource:///modules/policies/schema.jsm"
  );
  let { Policies } = ChromeUtils.importESModule(
    "resource:///modules/policies/Policies.sys.mjs"
  );

  checkArrayIsSorted(
    Object.keys(schema.properties),
    "policies-schema.json is alphabetically sorted."
  );
  checkArrayIsSorted(
    Object.keys(Policies),
    "Policies.jsm is alphabetically sorted."
  );
});

add_task(async function check_naming_conventions() {
  let { schema } = ChromeUtils.import(
    "resource:///modules/policies/schema.jsm"
  );
  equal(
    Object.keys(schema.properties).some(key => key.includes("__")),
    false,
    "Can't use __ in a policy name as it's used as a delimiter"
  );
});
