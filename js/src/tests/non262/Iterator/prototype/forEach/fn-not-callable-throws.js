// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [].values();

assertThrowsInstanceOf(() => iter.forEach(), TypeError);
assertThrowsInstanceOf(() => iter.forEach(undefined), TypeError);
assertThrowsInstanceOf(() => iter.forEach(null), TypeError);
assertThrowsInstanceOf(() => iter.forEach(0), TypeError);
assertThrowsInstanceOf(() => iter.forEach(false), TypeError);
assertThrowsInstanceOf(() => iter.forEach(''), TypeError);
assertThrowsInstanceOf(() => iter.forEach(Symbol('')), TypeError);
assertThrowsInstanceOf(() => iter.forEach({}), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
