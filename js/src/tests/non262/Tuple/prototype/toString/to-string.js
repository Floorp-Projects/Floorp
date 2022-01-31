// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/* Step 1 */
/* toString() should throw on a non-Tuple */
let method = Tuple.prototype.toString;
assertEq(method.call(#[1,2,3,4,5,6]), "1,2,3,4,5,6");
assertEq(method.call(Object(#[1,2,3,4,5,6])), "1,2,3,4,5,6");
assertThrowsInstanceOf(() => method.call("monkeys"), TypeError,
                       "value of TupleObject must be a Tuple");

// Normal case
assertEq(#[].toString(), "");
assertEq(#[1].toString(), "1");
assertEq(#[1,2].toString(), "1,2");

// if join method isn't callable, Object.toString should be called
Tuple.prototype.join = 7;
var t = #[1,2,3];
assertEq(t.toString(), "[object Tuple]");


reportCompare(0, 0);
