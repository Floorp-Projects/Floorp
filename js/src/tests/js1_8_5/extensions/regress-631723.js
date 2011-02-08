// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var o = {a:1, b:2};
o.watch("p", function() { return 13; });
delete o.p;
o.p = 0;
assertEq(o.p, 13);

reportCompare(0, 0, 'ok');
