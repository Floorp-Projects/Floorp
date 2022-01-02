// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

const methods = [
  iter => iter.map,
  iter => iter.filter,
  iter => iter.flatMap,
];

for (const method of methods) {
  const iter = [1].values();
  const iterMethod = method(iter);
  let iterHelper;
  iterHelper = iterMethod.call(iter, x => iterHelper.next());
  assertThrowsInstanceOf(() => iterHelper.next(), TypeError);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
