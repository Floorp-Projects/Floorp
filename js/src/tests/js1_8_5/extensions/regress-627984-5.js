// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Bug 627984 comment 11.
var o = ({});
o.p = function() {};
o.watch('p', function() { });
o.q = function() {}
delete o.p;
o.p = function() {};
assertEq(o.p, void 0);

reportCompare(0, 0, 'ok');
