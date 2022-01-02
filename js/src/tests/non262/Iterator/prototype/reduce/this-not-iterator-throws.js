// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const sum = (x, y) => x + y;
assertThrowsInstanceOf(Iterator.prototype.reduce.bind(undefined, sum), TypeError);
assertThrowsInstanceOf(Iterator.prototype.reduce.bind({}, sum), TypeError);
assertThrowsInstanceOf(Iterator.prototype.reduce.bind({next: 0}, sum), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
