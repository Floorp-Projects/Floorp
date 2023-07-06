// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const log = [];
const handlerProxy = new Proxy({}, {
  get: (target, key, receiver) => (...args) => {
    log.push(`${key}: ${args[1]?.toString()}`);

    const item = Reflect[key](...args);
    if (typeof item === 'function')
      return item.bind(receiver);
    return item;
  },
});
const iter = new Proxy({
  next: () => ({ done: false, value: 0 }),
}, handlerProxy);

const wrap = Iterator.from(iter);
// Call next multiple times. Should not call `get` on proxy.
wrap.next();
wrap.next();
wrap.next();

assertEqArray(log, [
  "get: Symbol(Symbol.iterator)",
  "get: next",
  "getPrototypeOf: undefined",
]);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
