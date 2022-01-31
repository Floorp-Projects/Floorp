// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
18 ECMAScript Standard Built-in Objects

Every built-in function object, including constructors, has a "name" property whose value is a String. Unless otherwise specified, this value is the name that is given to the function in this specification. Functions that are identified as anonymous functions use the empty String as the value of the "name" property.
...
Unless otherwise specified, the "name" property of a built-in function object has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
*/


var desc = Object.getOwnPropertyDescriptor(Tuple, "name");
assertEq(desc.value, "Tuple");

assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

reportCompare(0, 0);


