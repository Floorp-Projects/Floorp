/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*
 * NB: this test hardcodes for the value of PropertyTable::HASH_THRESHOLD (6).
 */

function s(e) {
  var a, b, c, d;
  function e() { }
}

reportCompare(0, 0, "don't crash");
