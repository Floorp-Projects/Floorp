// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [1, 2, 3].values();
const log = [];
const fn = (value) => {
  log.push(value.toString());
  return value % 2 == 0;
};

assertEq(iter.some(fn), true);
assertEq(log.join(','), '1,2');

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
