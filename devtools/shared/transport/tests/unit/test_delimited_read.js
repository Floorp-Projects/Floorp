/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const StreamUtils = require("devtools/shared/transport/stream-utils");

const StringInputStream = CC("@mozilla.org/io/string-input-stream;1",
                             "nsIStringInputStream", "setData");

function run_test() {
  add_task(async function() {
    await test_delimited_read("0123:", "0123:");
    await test_delimited_read("0123:4567:", "0123:");
    await test_delimited_read("012345678901:", "0123456789");
    await test_delimited_read("0123/0123", "0123/0123");
  });

  run_next_test();
}

/** * Tests ***/

function test_delimited_read(input, expected) {
  input = new StringInputStream(input, input.length);
  const result = StreamUtils.delimitedRead(input, ":", 10);
  Assert.equal(result, expected);
}
