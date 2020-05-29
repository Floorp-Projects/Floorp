// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
/*---
  Iterator constructor can be subclassed.
---*/

class TestIterator extends Iterator {
}

assertEq(new TestIterator() instanceof Iterator, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
