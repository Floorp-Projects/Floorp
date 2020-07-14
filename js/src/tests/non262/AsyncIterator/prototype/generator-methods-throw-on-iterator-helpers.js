// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

const asyncGeneratorProto = Object.getPrototypeOf(
  Object.getPrototypeOf(
    (async function *() {})()
  )
);

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(x => (async function*() {})()),
];

for (const method of methods) {
  const iteratorHelper = method((async function*() {})());
  asyncGeneratorProto.next.call(iteratorHelper).then(
    _ => assertEq(true, false, 'Expected reject'),
    err => assertEq(err instanceof TypeError, true),
  );
  asyncGeneratorProto.return.call(iteratorHelper).then(
    _ => assertEq(true, false, 'Expected reject'),
    err => assertEq(err instanceof TypeError, true),
  );
  asyncGeneratorProto.throw.call(iteratorHelper).then(
    _ => assertEq(true, false, 'Expected reject'),
    err => assertEq(err instanceof TypeError, true),
  );
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
