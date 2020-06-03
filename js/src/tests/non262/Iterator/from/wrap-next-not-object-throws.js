// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const iter = (value) => Iterator.from({
  next: () => value,
});

assertThrowsInstanceOf(() => iter(undefined).next(), TypeError);
assertThrowsInstanceOf(() => iter(null).next(), TypeError);
assertThrowsInstanceOf(() => iter(0).next(), TypeError);
assertThrowsInstanceOf(() => iter(false).next(), TypeError);
assertThrowsInstanceOf(() => iter('test').next(), TypeError);
assertThrowsInstanceOf(() => iter(Symbol('')).next(), TypeError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
