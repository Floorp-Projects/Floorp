// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

let iter = [1, 2, 3].values().asIndexedPairs();

for (const v of [[0, 1], [1, 2], [2, 3]]) {
  let result = iter.next();
  assertEq(result.done, false);
  assertEq(result.value[0], v[0]);
  assertEq(result.value[1], v[1]);
}

assertEq(iter.next().done, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
