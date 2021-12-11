// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [1, 2, 3].values();
assertEq(Array.isArray(iter), false);

const array = iter.toArray();
assertEq(Array.isArray(array), true);
assertEq(array.length, 3);

const expected = [1, 2, 3];
for (const item of array) {
  const expect = expected.shift();
  assertEq(item, expect);
}

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
