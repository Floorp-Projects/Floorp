// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

//CHECK#1
Tuple.prototype.toString = Object.prototype.toString;
var x = Tuple();
assertEq(x.toString(), "[object Tuple]");

//CHECK#2
Tuple.prototype.toString = Object.prototype.toString;
var x = Tuple(0, 1, 2);
assertEq(x.toString(), "[object Tuple]");

reportCompare(0, 0);
