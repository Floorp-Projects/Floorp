// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

const iteratorHelperProto = Object.getPrototypeOf([].values().map(x => x));

function *gen() {
  yield 1;
}

assertThrowsInstanceOf(() => iteratorHelperProto.next.call(gen()), TypeError);
assertThrowsInstanceOf(() => iteratorHelperProto.return.call(gen()), TypeError);
assertThrowsInstanceOf(() => iteratorHelperProto.throw.call(gen()), TypeError);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
