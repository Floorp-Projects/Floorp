// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

class TestError extends Error {}

class TestIterator extends Iterator {
  next() {
    return {done: false, value: 'value'};
  }

  closed = false;
  return(value) {
    this.closed = true;
    return {done: true, value};
  }
}

function checkIterResult({done, value}, expectedDone, expectedValue) {
  assertEq(done, expectedDone);
  assertEq(Array.isArray(value) ? value[1] : value, expectedValue);
}

const otherGlobal = newGlobal({newCompartment: true});

const methods = [
  ["map", x => x],
  ["filter", x => true],
  ["take", Infinity],
  ["drop", 0],
  ["asIndexedPairs", undefined],
  ["flatMap", x => [x]],
];

for (const [method, arg] of methods) {
  const {next: otherNext} = Object.getPrototypeOf(
    new otherGlobal.Array().values()[method](arg)
  );
  const iterator = new TestIterator();
  const helper = iterator[method](arg);
  checkIterResult(otherNext.call(helper), false, 'value');
}

for (const [method, arg] of methods) {
  const {return: otherReturn} = Object.getPrototypeOf(
    new otherGlobal.Array().values()[method](arg)
  );
  const iterator = new TestIterator();
  const helper = iterator[method](arg);
  assertEq(iterator.closed, false);
  checkIterResult(otherReturn.call(helper), true, undefined);
  assertEq(iterator.closed, true);
}

for (const [method, arg] of methods) {
  const {throw: otherThrow} = Object.getPrototypeOf(
    new otherGlobal.Array().values()[method](arg)
  );
  const iterator = new TestIterator();
  const helper = iterator[method](arg);
  assertThrowsInstanceOf(otherThrow.bind(helper, new TestError()), TestError);
  checkIterResult(helper.next(), true, undefined);
}

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
