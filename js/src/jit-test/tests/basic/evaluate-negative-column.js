// Negative column number should be handled as the first column.
const column = evaluate("new Error().columnNumber;", { columnNumber: -1 });
assertEq(column, 1);
