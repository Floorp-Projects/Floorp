// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const iter = [].values();
assertThrowsInstanceOf(() => iter.reduce((x, y) => x + y), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
