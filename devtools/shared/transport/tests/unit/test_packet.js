/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { JSONPacket, BulkPacket } =
  require("devtools/shared/transport/packets");

function run_test() {
  add_test(test_packet_done);
  run_next_test();
}

// Ensure done can be checked without getting an error
function test_packet_done() {
  const json = new JSONPacket();
  Assert.ok(!json.done);

  const bulk = new BulkPacket();
  Assert.ok(!bulk.done);

  run_next_test();
}
