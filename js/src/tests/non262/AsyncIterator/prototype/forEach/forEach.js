// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

const log = [];
const fn = (value) => log.push(value);
const iter = [1, 2, 3].values();

assertEq(iter.forEach(fn), undefined);
assertEq(log.join(','), '1,2,3');

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
