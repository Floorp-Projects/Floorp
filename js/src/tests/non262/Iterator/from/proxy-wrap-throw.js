// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const log = [];
const handlerProxy = new Proxy({}, {
  get: (target, key, receiver) => (...args) => {
    log.push(`${key}: ${args[1].toString()}`);

    const item = Reflect[key](...args);
    if (typeof item === 'function')
      return item.bind(receiver);
    return item;
  },
});
const iter = new Proxy({
  next: () => ({ done: false, value: 0 }),
  throw: (value) => ({ done: true, value }),
}, handlerProxy);

const wrap = Iterator.from(iter);
wrap.throw();
wrap.throw();

assertEq(
  log.join('\n'),
  `get: Symbol(Symbol.iterator)
get: next
get: throw
get: throw`
);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
