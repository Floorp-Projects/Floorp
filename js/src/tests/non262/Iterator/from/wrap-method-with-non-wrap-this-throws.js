// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
// All methods on %WrapForValidIteratorPrototype% require an [[Iterated]]
// internal slot on the `this` object.

class TestIterator {
  next() {
    return {
      done: false,
      value: 0,
    };
  }
}

const nextMethod = Iterator.from(new TestIterator()).next;
assertThrowsInstanceOf(() => nextMethod.call(undefined), TypeError);
assertThrowsInstanceOf(() => nextMethod.call(null), TypeError);
assertThrowsInstanceOf(() => nextMethod.call(0), TypeError);
assertThrowsInstanceOf(() => nextMethod.call(false), TypeError);
assertThrowsInstanceOf(() => nextMethod.call('test'), TypeError);
assertThrowsInstanceOf(() => nextMethod.call(Object(1)), TypeError);
assertThrowsInstanceOf(() => nextMethod.call({}), TypeError);

const returnMethod = Iterator.from(new TestIterator()).next;
assertThrowsInstanceOf(() => returnMethod.call(undefined), TypeError);
assertThrowsInstanceOf(() => returnMethod.call(null), TypeError);
assertThrowsInstanceOf(() => returnMethod.call(0), TypeError);
assertThrowsInstanceOf(() => returnMethod.call(false), TypeError);
assertThrowsInstanceOf(() => returnMethod.call('test'), TypeError);
assertThrowsInstanceOf(() => returnMethod.call(Object(1)), TypeError);
assertThrowsInstanceOf(() => returnMethod.call({}), TypeError);

const throwMethod = Iterator.from(new TestIterator()).next;
assertThrowsInstanceOf(() => throwMethod.call(undefined), TypeError);
assertThrowsInstanceOf(() => throwMethod.call(null), TypeError);
assertThrowsInstanceOf(() => throwMethod.call(0), TypeError);
assertThrowsInstanceOf(() => throwMethod.call(false), TypeError);
assertThrowsInstanceOf(() => throwMethod.call('test'), TypeError);
assertThrowsInstanceOf(() => throwMethod.call(Object(1)), TypeError);
assertThrowsInstanceOf(() => throwMethod.call({}), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
