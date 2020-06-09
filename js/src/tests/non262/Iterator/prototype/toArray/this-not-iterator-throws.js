// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

assertThrowsInstanceOf(Iterator.prototype.toArray.bind(undefined), TypeError);
assertThrowsInstanceOf(Iterator.prototype.toArray.bind({}), TypeError);
assertThrowsInstanceOf(Iterator.prototype.toArray.bind({next: 0}), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
