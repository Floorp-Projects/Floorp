// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [1, 3, 5].values();
const fn = (value) => value % 2 == 1;

assertEq(iter.every(fn), true);

assertEq([].values().every(x => x), true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
