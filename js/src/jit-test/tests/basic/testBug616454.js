function isnan(x) { return x !== x }
assertEq(isnan(deserialize(serialize(-'test'))), true);
