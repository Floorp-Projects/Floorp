// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

const realm = newGlobal();

const realm_TupleConstructor = realm.eval("Tuple");
const realm_tuple = realm.eval(`
  var tuple = Tuple(1, 2, 3);
  tuple;
`);

assertEq(realm_tuple === #[1, 2, 3], true);
assertEq(realm_tuple.constructor, Tuple);
assertEq(realm.eval("tuple.constructor"), realm_TupleConstructor);

realm_TupleConstructor.prototype.foo = 42;
assertEq(realm.eval("tuple.foo"), 42);
assertEq(realm_tuple.foo, undefined);

assertEq("foo" in Object(realm_tuple), false);
assertEq(realm.eval(`"foo" in Object(tuple)`), true);

if (typeof reportCompare === "function") reportCompare(0, 0);
