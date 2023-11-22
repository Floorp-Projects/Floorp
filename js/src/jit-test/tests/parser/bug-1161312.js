// Test that lazy scripts can handle OOB column numbers.

assertEq(evaluate(`var f = x=>saveStack().column; f()`, { columnNumber: 1730 }), 1741);
