// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

assertEq(Object.prototype.toString.call(#[1,2,3]),
         "[object Tuple]");
assertEq(Object.prototype.toString.call(Object(#[1,2,3])),
         "[object Tuple]");
assertEq(Object.prototype.toString.call(#[]),
         "[object Tuple]");
assertEq(Object.prototype.toString.call(Object(#[])),
         "[object Tuple]");

reportCompare(0, 0);
