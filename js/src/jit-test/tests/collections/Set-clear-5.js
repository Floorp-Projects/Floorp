// Set.clear is unaffected by deleting/monkeypatching Set.prototype.{delete,iterator}.

var data = ["a", 1, {}];
var s1 = Set(data), s2 = Set(data);

delete Set.prototype.delete;
delete Set.prototype.iterator;
s1.clear();
assertEq(s1.size, 0);

Set.prototype.delete = function () { throw "FAIL"; };
Set.prototype.iterator = function () { throw "FAIL"; };
s2.clear();
assertEq(s2.size, 0);
