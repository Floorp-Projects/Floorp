// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const otherGlobal = newGlobal({newCompartment: true});
assertEq(TypeError !== otherGlobal.TypeError, true);

const iter = [].values();

assertThrowsInstanceOf(() => iter.every(), TypeError);
assertThrowsInstanceOf(
  otherGlobal.Iterator.prototype.every.bind(iter),
  otherGlobal.TypeError,
  'TypeError comes from the realm of the method.',
);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
