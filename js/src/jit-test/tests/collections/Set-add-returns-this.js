// Bug 1031632 - Map.prototype.set, WeakMap.prototype.set and
// Set.prototype.add should be chainable

var s = new Set();
assertEq(s.add('BAR'), s);
var b = s.add('foo').has('foo');
assertEq(b, true);
