// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const log = [];
const handlerProxy = new Proxy({}, {
  get: (target, key, receiver) => (...args) => {
    log.push(`${key}: ${args[1]?.toString()}`);
    return Reflect[key](...args);
  },
});

class TestIterator extends Iterator {
  next() {
    return {done: true};
  }
}

const iter = new Proxy(new TestIterator(), handlerProxy);
assertThrowsInstanceOf(() => iter.reduce(1), TypeError);

assertEqArray(
  log,
  ["get: reduce"]
);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
