// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [].values();

assertThrowsInstanceOf(() => iter.find(), TypeError);
assertThrowsInstanceOf(() => iter.find(undefined), TypeError);
assertThrowsInstanceOf(() => iter.find(null), TypeError);
assertThrowsInstanceOf(() => iter.find(0), TypeError);
assertThrowsInstanceOf(() => iter.find(false), TypeError);
assertThrowsInstanceOf(() => iter.find(''), TypeError);
assertThrowsInstanceOf(() => iter.find(Symbol('')), TypeError);
assertThrowsInstanceOf(() => iter.find({}), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
