// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

async function *gen() { yield 'value'; }

const asyncIteratorHelperProto = Object.getPrototypeOf(gen().map(x => x));

assertThrowsInstanceOf(() => asyncIteratorHelperProto.next.call(gen()), TypeError);
assertThrowsInstanceOf(() => asyncIteratorHelperProto.return.call(gen()), TypeError);
assertThrowsInstanceOf(() => asyncIteratorHelperProto.throw.call(gen()), TypeError);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
