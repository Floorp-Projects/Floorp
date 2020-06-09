// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const otherGlobal = newGlobal({newCompartment: true});

let array = [1, 2, 3].values().toArray();
assertEq(array instanceof Array, true);
assertEq(array instanceof otherGlobal.Array, false);

array = otherGlobal.Iterator.prototype.toArray.call([1, 2, 3].values());
assertEq(array instanceof Array, false);
assertEq(array instanceof otherGlobal.Array, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
