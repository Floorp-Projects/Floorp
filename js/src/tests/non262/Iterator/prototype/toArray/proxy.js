// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
//
// This test checks that %Iterator.prototype%.toArray only gets the `next` method off of the
// iterator once, and never accesses the @@iterator property.
const log = [];
const handlerProxy = new Proxy({}, {
  get: (target, key, receiver) => (...args) => {
    log.push(`${key}: ${args[1]?.toString()}`);
    return Reflect[key](...args);
  },
});

class Counter extends Iterator {
  value = 0;
  next() {
    const value = this.value;
    if (value < 2) {
      this.value = value + 1;
      return {done: false, value};
    }
    return {done: true};
  }
}

const iter = new Proxy(new Counter(), handlerProxy);
const array = iter.toArray();

assertEq(
  log.join('\n'),
  `get: toArray
get: next
get: value
set: value
getOwnPropertyDescriptor: value
defineProperty: value
get: value
set: value
getOwnPropertyDescriptor: value
defineProperty: value
get: value`
);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
