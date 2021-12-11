// |reftest| skip-if(!this.hasOwnProperty('Iterator'))
//

/*---
esid: pending
description: Lazy %Iterator.prototype% methods access specified properties only.
info: >
features: [iterator-helpers]
---*/

class TestIterator extends Iterator {
  value = 0;
  next() {
    if (this.value < 2)
      return { done: false, value: this.value++ };
    return { done: true, value: undefined };
  }
}

const handlerProxy = log => new Proxy({}, {
  get: (target, key, receiver) => (...args) => {
    const target = args[0];
    const item = Reflect[key](...args);

    log.push(`${key}: ${args.filter(x => typeof x != 'object').map(x => x.toString())}`);

    return item;
  },
});

const methods = [
  [iter => iter.map(x => x), 'map'],
  [iter => iter.filter(x => true), 'filter'],
  [iter => iter.take(4), 'take'],
  [iter => iter.drop(0), 'drop'],
  [iter => iter.asIndexedPairs(), 'asIndexedPairs'],
  [iter => iter.flatMap(x => [x]), 'flatMap'],
];

for (const method of methods) {
  const log = [];
  const iteratorProxy = new Proxy(new TestIterator(), handlerProxy(log));
  const iteratorHelper = method[0](iteratorProxy);
  const methodName = method[1];

  iteratorHelper.next();
  iteratorHelper.next();
  iteratorHelper.next();
  assertEq(iteratorHelper.next().done, true);

  assertEq(
    log.join('\n'),
    `get: ${methodName}
get: next
get: value
get: value
getOwnPropertyDescriptor: value
defineProperty: value
set: value,1
get: value
get: value
getOwnPropertyDescriptor: value
defineProperty: value
set: value,2
get: value`
  )
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
