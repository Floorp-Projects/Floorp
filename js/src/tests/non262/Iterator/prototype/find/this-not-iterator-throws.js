// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const fn = x => x;
assertThrowsInstanceOf(Iterator.prototype.find.bind(undefined, fn), TypeError);
assertThrowsInstanceOf(Iterator.prototype.find.bind({}, fn), TypeError);
assertThrowsInstanceOf(Iterator.prototype.find.bind({next: 0}, fn), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
