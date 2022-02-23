// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.3 Tuple.prototype.valueOf ( )
When the valueOf function is called, the following steps are taken:

1. Return ? thisTupleValue(this value).
*/

var valueOf = Tuple.prototype.valueOf;
assertEq(typeof valueOf, 'function');

var tup = #[1,2,3];
assertEq(valueOf.call(tup), tup);
assertEq(valueOf.call(Object(tup)), tup);
assertThrowsInstanceOf(() => valueOf.call("monkeys"), TypeError,
                       "this is not Tuple");
assertThrowsInstanceOf(() => valueOf.call({}), TypeError,
                       "this is not Tuple");
assertThrowsInstanceOf(() => valueOf.call(new Object(1)), TypeError,
                       "this is not Tuple");


reportCompare(0, 0);
