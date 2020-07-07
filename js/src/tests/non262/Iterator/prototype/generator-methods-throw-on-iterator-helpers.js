// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

const generatorProto = Object.getPrototypeOf(
  Object.getPrototypeOf(
    (function *() {})()
  )
);

const iteratorHelper = [0].values().map(x => x);

assertThrowsInstanceOf(() => generatorProto.next.call(iteratorHelper), TypeError);
assertThrowsInstanceOf(() => generatorProto.return.call(iteratorHelper), TypeError);
assertThrowsInstanceOf(() => generatorProto.throw.call(iteratorHelper), TypeError);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
