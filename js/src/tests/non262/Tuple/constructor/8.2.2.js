// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

/*
8.2.2 Properties of the Tuple Constructor
The Tuple constructor:

has a [[Prototype]] internal slot whose value is %Function.prototype%.
*/
assertEq(Object.getPrototypeOf(Tuple), Function.prototype);

reportCompare(0, 0);
