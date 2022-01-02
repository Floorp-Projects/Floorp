// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [].values();
const array = iter.toArray();

assertEq(Array.isArray(array), true);
assertEq(array.length, 0);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
