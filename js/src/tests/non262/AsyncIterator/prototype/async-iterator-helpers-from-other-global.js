// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

class TestError extends Error {}

class TestIterator extends AsyncIterator {
  async next() {
    return {done: false, value: 'value'};
  }

  closed = false;
  async return(value) {
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
  ["flatMap", async function*(x) { yield x; }],
];

const {next: otherNext, return: otherReturn, throw: otherThrow} =
  Object.getPrototypeOf(otherGlobal.eval("(async function*() {})().map(x => x)"));

(async () => {
  for (const [method, arg] of methods) {
    const iterator = new TestIterator();
    const helper = iterator[method](arg);
    checkIterResult(await otherNext.call(helper), false, 'value');
  }

  for (const [method, arg] of methods) {
    const iterator = new TestIterator();
    const helper = iterator[method](arg);
    assertEq(iterator.closed, false);
    checkIterResult(await otherReturn.call(helper), true, undefined);
    assertEq(iterator.closed, true);
  }

  for (const [method, arg] of methods) {
    const iterator = new TestIterator();
    const helper = iterator[method](arg);
    try {
      await otherThrow.call(helper, new TestError());
      assertEq(true, false, 'Expected exception');
    } catch (exc) {
      assertEq(exc instanceof TestError, true);
    }
    checkIterResult(await helper.next(), true, undefined);
  }
})();

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
