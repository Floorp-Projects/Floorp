// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// See bug 627984 comment 20.
var obj = {m: function () {}};
obj.watch("m", function () { throw 'FAIL'; });
var f = obj.m;  // don't call the watchpoint

reportCompare(0, 0, 'ok');
