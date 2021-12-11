// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
const log = [];
const handlerProxy = new Proxy({}, {
  get: (target, key, receiver) => (...args) => {
    log.push(`${key}: ${args[1]?.toString()}`);
    return Reflect[key](...args);
  },
});

class TestIterator extends AsyncIterator {
  next() {
    return Promise.resolve({done: true});
  }
}

async function* gen() {
  yield 1;
}

const iter = new Proxy(new TestIterator(), handlerProxy);
iter.every(1).then(() => assertEq(true, false, 'expected error'), err => assertEq(err instanceof TypeError, true));

assertEq(
  log.join('\n'),
  `get: every
get: next`
);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
