// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

let iter = [1, 2, 3].values().flatMap(x => [x, x + 1]);
for (const v of [1, 2, 2, 3, 3, 4]) {
  let result = iter.next();
  assertEq(result.done, false);
  assertEq(result.value, v);
}
assertEq(iter.next().done, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
