// Map.clear is unaffected by deleting/monkeypatching Map.prototype.{delete,iterator}.

var data = [["a", 1], ["b", 2]];
var m1 = Map(data), m2 = Map(data);

delete Map.prototype.delete;
delete Map.prototype.iterator;
m1.clear();
assertEq(m1.size, 0);

Map.prototype.delete = function () { throw "FAIL"; };
Map.prototype.iterator = function () { throw "FAIL"; };
m2.clear();
assertEq(m2.size, 0);
