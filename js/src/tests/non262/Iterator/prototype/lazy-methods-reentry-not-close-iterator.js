// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

const methods = [
  [iter => iter.map, x => x],
  [iter => iter.filter, x => true],
  [iter => iter.flatMap, x => [x]],
];

for (const method of methods) {
  const iter = [1, 2, 3].values();
  const iterMethod = method[0](iter);
  let iterHelper;
  let reentered = false;
  iterHelper = iterMethod.call(iter, x => {
    if (x == 2) {
      // Reenter the currently running generator.
      reentered = true;
      assertThrowsInstanceOf(() => iterHelper.next(), TypeError);
    }
    return method[1](x);
  });

  let result = iterHelper.next();
  assertEq(result.value, 1);
  assertEq(result.done, false);

  assertEq(reentered, false);
  result = iterHelper.next();
  assertEq(reentered, true);
  assertEq(result.value, 2);
  assertEq(result.done, false);

  result = iterHelper.next();
  assertEq(result.value, 3);
  assertEq(result.done, false);

  result = iterHelper.next();
  assertEq(result.value, undefined);
  assertEq(result.done, true);
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
