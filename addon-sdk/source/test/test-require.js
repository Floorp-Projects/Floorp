/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const traceback = require("sdk/console/traceback");

exports.test_no_args = function(test) {
  var passed = false;
  try {
    var oops = require(); // leave this on line 6!
  } catch(e) {
    let msg = e.toString();
    test.assertEqual(msg.indexOf("Error: you must provide a module name when calling require() from "), 0);
    test.assertNotEqual(msg.indexOf("test-require"), -1, msg);
    // we'd also like to assert that the right filename and linenumber is in
    // the stack trace, but this currently doesn't work (see bugs 679591 and
    // 551604)
    if (0) {
      let tb = traceback.fromException(e);
      let lastFrame = tb[tb.length-1];
      test.assertNotEqual(lastFrame.filename.indexOf("test-require.js"), -1,
                          lastFrame.filename);
      test.assertEqual(lastFrame.lineNo, 6);
      test.assertEqual(lastFrame.funcName, "??");
    }
    passed = true;
  }
  test.assert(passed, 'require() with no args should raise helpful error');
};
