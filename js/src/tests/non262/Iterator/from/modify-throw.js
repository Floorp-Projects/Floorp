// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const iter = {
  next: () => ({ done: false, value: 0 }),
  throw: (value) => ({ done: true, value }),
};

const wrap = Iterator.from(iter);

let {done, value} = wrap.throw(0);
assertEq(done, true);
assertEq(value, 0);

class TestError extends Error {}
iter.throw = () => { throw new TestError(); };
assertThrowsInstanceOf(() => wrap.throw(), TestError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
