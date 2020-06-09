// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const fn = x => x;
assertThrowsInstanceOf(Iterator.prototype.forEach.bind(undefined, fn), TypeError);
assertThrowsInstanceOf(Iterator.prototype.forEach.bind({}, fn), TypeError);
assertThrowsInstanceOf(Iterator.prototype.forEach.bind({next: 0}, fn), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
