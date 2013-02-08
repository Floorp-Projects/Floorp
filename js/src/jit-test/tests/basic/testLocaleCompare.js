assertEq("undefined".localeCompare(), 0);
assertEq("a".localeCompare(), "a".localeCompare("undefined"));
assertEq("a".localeCompare("b"), -1);
assertEq("a".localeCompare("b", "a"), -1);
assertEq("b".localeCompare("a"), 1);
assertEq("b".localeCompare("a", "b"), 1);
assertEq("a".localeCompare("a"), 0);
assertEq("a".localeCompare("a", "b", "c"), 0);
