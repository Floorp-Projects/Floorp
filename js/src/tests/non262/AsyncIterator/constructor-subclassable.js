// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))
/*---
  AsyncIterator constructor can be subclassed.
---*/

class TestIterator extends AsyncIterator {
}

assertEq(new TestIterator() instanceof AsyncIterator, true);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
