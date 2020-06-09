// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [].values();

assertThrowsInstanceOf(() => iter.every(), TypeError);
assertThrowsInstanceOf(() => iter.every(undefined), TypeError);
assertThrowsInstanceOf(() => iter.every(null), TypeError);
assertThrowsInstanceOf(() => iter.every(0), TypeError);
assertThrowsInstanceOf(() => iter.every(false), TypeError);
assertThrowsInstanceOf(() => iter.every(''), TypeError);
assertThrowsInstanceOf(() => iter.every(Symbol('')), TypeError);
assertThrowsInstanceOf(() => iter.every({}), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
