assertEq(Map.prototype.set.toString().includes("function set()"), true);
assertEq(Object.getOwnPropertyDescriptor(Map.prototype, 'size').get.toString().includes("function size()"), true);
assertEq(Object.getOwnPropertyDescriptor(RegExp.prototype, 'flags').get.toString().includes("function flags()"), true);

assertEq(Map.prototype.set.toSource().includes("function set()"), true);
assertEq(Object.getOwnPropertyDescriptor(Map.prototype, 'size').get.toSource().includes("function get size()"), true);
assertEq(Object.getOwnPropertyDescriptor(RegExp.prototype, 'flags').get.toSource().includes("function get flags()"), true);
