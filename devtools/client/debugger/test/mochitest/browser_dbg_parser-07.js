/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that nodes with locaiton information attached can be properly
 * verified for containing lines and columns.
 */

function test() {
  let { ParserHelpers } = Cu.import("resource://devtools/shared/Parser.jsm", {});

  let node1 = { loc: {
    start: { line: 1, column: 10 },
    end: { line: 10, column: 1 }
  }};
  let node2 = { loc: {
    start: { line: 1, column: 10 },
    end: { line: 1, column: 20 }
  }};

  ok(ParserHelpers.nodeContainsLine(node1, 1), "1st check.");
  ok(ParserHelpers.nodeContainsLine(node1, 5), "2nd check.");
  ok(ParserHelpers.nodeContainsLine(node1, 10), "3rd check.");

  ok(!ParserHelpers.nodeContainsLine(node1, 0), "4th check.");
  ok(!ParserHelpers.nodeContainsLine(node1, 11), "5th check.");

  ok(ParserHelpers.nodeContainsLine(node2, 1), "6th check.");
  ok(!ParserHelpers.nodeContainsLine(node2, 0), "7th check.");
  ok(!ParserHelpers.nodeContainsLine(node2, 2), "8th check.");

  ok(!ParserHelpers.nodeContainsPoint(node1, 1, 10), "9th check.");
  ok(!ParserHelpers.nodeContainsPoint(node1, 10, 1), "10th check.");

  ok(!ParserHelpers.nodeContainsPoint(node1, 0, 10), "11th check.");
  ok(!ParserHelpers.nodeContainsPoint(node1, 11, 1), "12th check.");

  ok(!ParserHelpers.nodeContainsPoint(node1, 1, 9), "13th check.");
  ok(!ParserHelpers.nodeContainsPoint(node1, 10, 2), "14th check.");

  ok(ParserHelpers.nodeContainsPoint(node2, 1, 10), "15th check.");
  ok(ParserHelpers.nodeContainsPoint(node2, 1, 15), "16th check.");
  ok(ParserHelpers.nodeContainsPoint(node2, 1, 20), "17th check.");

  ok(!ParserHelpers.nodeContainsPoint(node2, 0, 10), "18th check.");
  ok(!ParserHelpers.nodeContainsPoint(node2, 2, 20), "19th check.");

  ok(!ParserHelpers.nodeContainsPoint(node2, 0, 9), "20th check.");
  ok(!ParserHelpers.nodeContainsPoint(node2, 2, 21), "21th check.");

  ok(!ParserHelpers.nodeContainsPoint(node2, 1, 9), "22th check.");
  ok(!ParserHelpers.nodeContainsPoint(node2, 1, 21), "23th check.");

  finish();
}
