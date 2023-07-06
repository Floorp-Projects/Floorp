// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

const otherIteratorProto = newGlobal({newCompartment: true}).Iterator.prototype;

const methods = [
  ["map", x => x],
  ["filter", x => true],
  ["take", Infinity],
  ["drop", 0],
  ["flatMap", x => [x]],
];

// Use the lazy Iterator methods from another global on an iterator from this global.
for (const [method, arg] of methods) {
  const iterator = [1, 2, 3].values();
  const helper = otherIteratorProto[method].call(iterator, arg);

  for (const expected of [1, 2, 3]) {
    const {done, value} = helper.next();
    assertEq(done, false);
    assertEq(Array.isArray(value) ? value[1] : value, expected);
  }

  const {done, value} = helper.next();
  assertEq(done, true);
  assertEq(value, undefined);
}

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
