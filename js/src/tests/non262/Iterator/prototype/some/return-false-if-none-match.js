// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [1, 3, 5].values();
const fn = (value) => value % 2 == 0;

assertEq(iter.some(fn), false);

assertEq([].values().some(x => x), false);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
