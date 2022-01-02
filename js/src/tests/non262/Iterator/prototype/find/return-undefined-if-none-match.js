// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [1, 3, 5].values();
const fn = (value) => value % 2 == 0;

assertEq(iter.find(fn), undefined);

assertEq([].values().find(x => x), undefined);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
