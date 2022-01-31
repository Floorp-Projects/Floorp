// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var list = {
  '0': 41,
  '1': 42,
  '2': 43,
  length: 3
};
var calls = [];

function mapFn(value) {
  calls.push({
    args: arguments,
    thisArg: this
  });
  return value * 2;
}

var result = Tuple.from(list, mapFn);

assertEq(result.length, 3);
assertEq(result[0], 82);
assertEq(result[1], 84);
assertEq(result[2], 86);

assertEq(calls.length, 3);

assertEq(calls[0].args.length, 2);
assertEq(calls[0].args[0], 41);
assertEq(calls[0].args[1], 0);
assertEq(calls[0].thisArg, this);

assertEq(calls[1].args.length, 2);
assertEq(calls[1].args[0], 42);
assertEq(calls[1].args[1], 1);
assertEq(calls[1].thisArg, this);

assertEq(calls[2].args.length, 2);
assertEq(calls[2].args[0], 43);
assertEq(calls[2].args[1], 2);
assertEq(calls[2].thisArg, this);

reportCompare(0, 0);
