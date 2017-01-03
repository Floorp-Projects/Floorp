/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test ThreadSafeDevToolsUtils.flatten

function run_test() {
  const { flatten } = DevToolsUtils;

  const flat = flatten([["a", "b", "c"],
                        ["d", "e", "f"],
                        ["g", "h", "i"]]);

  equal(flat.length, 9);
  equal(flat[0], "a");
  equal(flat[1], "b");
  equal(flat[2], "c");
  equal(flat[3], "d");
  equal(flat[4], "e");
  equal(flat[5], "f");
  equal(flat[6], "g");
  equal(flat[7], "h");
  equal(flat[8], "i");
}
