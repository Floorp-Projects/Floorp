/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function() {
  let { TestFile } = ChromeUtils.import("resource://test/TestFile.jsm");
  TestFile.doTest(result => {
    Assert.ok(result);
    run_next_test();
  });
});
