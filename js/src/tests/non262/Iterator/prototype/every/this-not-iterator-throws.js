// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const fn = x => x;
assertThrowsInstanceOf(Iterator.prototype.every.bind(undefined, fn), TypeError);
assertThrowsInstanceOf(Iterator.prototype.every.bind({}, fn), TypeError);
assertThrowsInstanceOf(Iterator.prototype.every.bind({next: 0}, fn), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
