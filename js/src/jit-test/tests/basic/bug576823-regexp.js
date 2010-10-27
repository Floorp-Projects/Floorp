// Sticky should work across disjunctions.

assertEq(/A|B/y.exec("CB"), null);
