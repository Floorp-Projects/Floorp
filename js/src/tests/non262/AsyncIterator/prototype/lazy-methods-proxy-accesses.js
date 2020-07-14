// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
//

/*---
esid: pending
description: Lazy %AsyncIterator.prototype% methods access specified properties only.
info: >
  Iterator Helpers proposal 2.1.6
features: [iterator-helpers]
---*/

let log;
const handlerProxy = new Proxy({}, {
  get: (target, key, receiver) => (...args) => {
    log.push(`${key}: ${args.filter(x => typeof x != 'object').map(x => x.toString())}`);
    return Reflect[key](...args);
  },
});

class TestIterator extends AsyncIterator {
  value = 0;
  async next() {
    if (this.value < 1)
      return new Proxy({done: false, value: this.value++}, handlerProxy);
    return new Proxy({done: true, value: undefined}, handlerProxy);
  }
}

const methods = [
  ['map', x => x],
  ['filter', x => true],
  ['take', 4],
  ['drop', 0],
  ['asIndexedPairs', undefined],
  ['flatMap', async function*(x) { yield x; }],
];

(async () => {
  for (const [method, argument] of methods) {
    log = [];
    const iteratorProxy = new Proxy(new TestIterator(), handlerProxy);
    const iteratorHelper = iteratorProxy[method](argument);

    await iteratorHelper.next();
    await iteratorHelper.next();
    const {done} = await iteratorHelper.next();
    assertEq(done, true);
    assertEq(
      log.join('\n'),
      `get: ${method}
get: next
get: value
get: value
set: value,1
getOwnPropertyDescriptor: value
defineProperty: value
get: then
get: done
get: value
get: value
get: then
get: done`
    );
  }
})();

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
