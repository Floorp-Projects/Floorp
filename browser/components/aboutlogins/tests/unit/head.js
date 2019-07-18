/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://testing-common/LoginTestUtils.jsm", this);
const { LoginHelper } = ChromeUtils.import(
  "resource://gre/modules/LoginHelper.jsm"
);

const TestData = LoginTestUtils.testData;
const newPropertyBag = LoginHelper.newPropertyBag;

/**
 * All the tests are implemented with add_task, this starts them automatically.
 */
function run_test() {
  do_get_profile();
  run_next_test();
}
