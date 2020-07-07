// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: %Iterator.prototype%.map does not call return when IteratorValue returns an abrupt completion.
info: >
features: [iterator-helpers]
---*/

const handlerProxy = log => new Proxy({}, {
  get: (target, key, receiver) => (...args) => {
    const target = args[0];
    const item = Reflect[key](...args);

    log.push(`${key}: ${args.filter(x => typeof x != 'object').map(x => x.toString())}`);

    switch (typeof item) {
      case 'function': return item.bind(new Proxy(target, handlerProxy(log)));
      case 'object': return new Proxy(item, handlerProxy(log));
      default: return item;
    }
  },
});

const log = [];
const iterator = Object.setPrototypeOf({
  next: function() {
    throw 'error';
    return { done: false, value: 0 };
  },
  return: function(value) {
    log.push('close iterator');
    return { done: true, value };
  },
}, Iterator.prototype);
const iteratorProxy = new Proxy(iterator, handlerProxy(log));
const mappedProxy = iteratorProxy.map(x => x);

try {
  mappedProxy.next();
} catch (exc) {
  assertEq(exc, 'error');
}

console.log(log.join('\n'));

assertEq(
  log.join('\n'),
`get: map
get: next`
);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
