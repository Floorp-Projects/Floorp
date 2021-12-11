// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const reducer = (x, y) => 0;
const iterator = [].values();

assertEq(iterator.reduce(reducer, 1), 1);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
