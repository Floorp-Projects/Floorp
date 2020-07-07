// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

// All truthy values are kept.
const truthyValues = [true, 1, [], {}, 'test'];
for (const value of [...truthyValues].values().filter(x => x)) {
  assertEq(truthyValues.shift(), value);
}

// All falsy values are filtered out.
const falsyValues = [false, 0, '', null, undefined, NaN, -0, 0n, createIsHTMLDDA()];
const result = falsyValues.values().filter(x => x).next();
assertEq(result.done, true);
assertEq(result.value, undefined);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
