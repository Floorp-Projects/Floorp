// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/* 18 ECMAScript Standard Built-in Objects
...

  Built-in function objects that are not identified as constructors do not
  implement the [[Construct]] internal method unless otherwise specified in
  the description of a particular function.
*/

assertEq(isConstructor(Tuple.isTuple), false);

assertThrowsInstanceOf(() => new Tuple.isTuple(#[]), TypeError,
                       "new Tuple.isTuple(#[]) throws");

reportCompare(0, 0);
