// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [].values();

assertThrowsInstanceOf(() => iter.some(), TypeError);
assertThrowsInstanceOf(() => iter.some(undefined), TypeError);
assertThrowsInstanceOf(() => iter.some(null), TypeError);
assertThrowsInstanceOf(() => iter.some(0), TypeError);
assertThrowsInstanceOf(() => iter.some(false), TypeError);
assertThrowsInstanceOf(() => iter.some(''), TypeError);
assertThrowsInstanceOf(() => iter.some(Symbol('')), TypeError);
assertThrowsInstanceOf(() => iter.some({}), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
