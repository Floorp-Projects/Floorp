// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
18 ECMAScript Standard Built-in Objects
...
Every built-in function object, including constructors, has a "length" property whose value is a non-negative integral Number. Unless otherwise specified, this value is equal to the number of required parameters shown in the subclause heading for the function description. Optional parameters and rest parameters are not included in the parameter count.
*/
var desc = Object.getOwnPropertyDescriptor(Tuple, "length");
assertEq(desc.value, 0);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

reportCompare(0, 0);
