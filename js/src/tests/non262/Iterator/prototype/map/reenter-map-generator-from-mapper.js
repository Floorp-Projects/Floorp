// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

// Re-entering the map() generator from the called mapper fails.

let iterator;
function mapper(x) {
  let n = iterator.next();
  return x;
}
iterator = [0].values().map(mapper);

assertThrowsInstanceOf(iterator.next, TypeError);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
