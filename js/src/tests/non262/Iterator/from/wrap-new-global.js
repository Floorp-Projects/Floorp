// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const otherGlobal = newGlobal({newCompartment: true});

const iter = [1, 2, 3].values();
assertEq(iter, Iterator.from(iter));
assertEq(iter !== otherGlobal.Iterator.from(iter), true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
