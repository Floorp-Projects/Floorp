// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const fn = x => x;
assertThrowsInstanceOf(Iterator.prototype.some.bind(undefined, fn), TypeError);
assertThrowsInstanceOf(Iterator.prototype.some.bind({}, fn), TypeError);
assertThrowsInstanceOf(Iterator.prototype.some.bind({next: 0}, fn), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
