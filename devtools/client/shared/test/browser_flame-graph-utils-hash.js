/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests if (idle) nodes are added when necessary in the flame graph data.

var {FlameGraphUtils} = require("devtools/client/shared/widgets/FlameGraph");

var test = Task.async(function*() {
  let hash1 = FlameGraphUtils._getStringHash("abc");
  let hash2 = FlameGraphUtils._getStringHash("acb");
  let hash3 = FlameGraphUtils._getStringHash(Array.from(Array(100000)).join("a"));
  let hash4 = FlameGraphUtils._getStringHash(Array.from(Array(100000)).join("b"));

  isnot(hash1, hash2, "The hashes should not be equal (1).");
  isnot(hash2, hash3, "The hashes should not be equal (2).");
  isnot(hash3, hash4, "The hashes should not be equal (3).");

  ok(Number.isInteger(hash1), "The hashes should be integers, not Infinity or NaN (1).");
  ok(Number.isInteger(hash2), "The hashes should be integers, not Infinity or NaN (2).");
  ok(Number.isInteger(hash3), "The hashes should be integers, not Infinity or NaN (3).");
  ok(Number.isInteger(hash4), "The hashes should be integers, not Infinity or NaN (4).");

  finish();
});
