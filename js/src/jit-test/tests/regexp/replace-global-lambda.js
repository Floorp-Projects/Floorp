"use strict";

function testNoCaptureGroups() {
  var s = "a..bb.cba.";
  for (var i = 0; i < 20; i++) {
    var log = "";
    var res = s.replace(/a|b/g, function(...args) {
      assertEq(this, undefined);
      assertEq(args.length, 3);
      assertEq(args[2], s);
      log += "(" + args[0] + args[1] + ")";
      return "X";
    });
    assertEq(res, "X..XX.cXX.");
    assertEq(log, "(a0)(b3)(b4)(b7)(a8)");
  }
}
testNoCaptureGroups();

function testCaptureGroups() {
  var s = "a..bb.cba.";
  for (var i = 0; i < 20; i++) {
    var log = "";
    var res = s.replace(/(a)|(b)/g, function(...args) {
      assertEq(this, undefined);
      assertEq(args.length, 5);
      assertEq(args[4], s);
      log += "(" + args[0] + args[1] + args[2] + args[3] + ")";
      return "X";
    });
    assertEq(res, "X..XX.cXX.");
    assertEq(log, "(aaundefined0)(bundefinedb3)(bundefinedb4)(bundefinedb7)(aaundefined8)");
  }
}
testCaptureGroups();

// Calling RegExp#compile in the callback has no effect, because |replace| will
// create a new RegExp object from the original source/flags if this happens.
function testCaptureGroupsChanging() {
  var s = "a..bb.cba.";
  for (var i = 0; i < 20; i++) {
    var log = "";
    var re = /a|b/g;
    var res = s.replace(re, function(...args) {
      assertEq(this, undefined);
      assertEq(args.length, 3);
      assertEq(args[2], s);
      log += "(" + args[0] + args[1] + ")";
      re.compile("(a)|(b)", "g");
      return "X";
    });
    assertEq(res, "X..XX.cXX.");
    assertEq(log, "(a0)(b3)(b4)(b7)(a8)");
  }
}
testCaptureGroupsChanging();
