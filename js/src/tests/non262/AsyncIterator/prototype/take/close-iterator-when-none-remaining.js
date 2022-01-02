// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

//
//
/*---
esid: pending
description: %AsyncIterator.prototype%.take closes the iterator when remaining is 0.
info: >
  Iterator Helpers proposal 2.1.6.4
features: [iterator-helpers]
---*/

class TestIterator extends AsyncIterator {
  async next() {
    return {done: false, value: 'value'};
  }

  closed = false;
  async return() {
    this.closed = true;
    return {done: true};
  }
}

const iter = new TestIterator();
const iterTake = iter.take(1);

iterTake.next().then(
  ({done, value}) => {
    assertEq(done, false);
    assertEq(value, 'value');
    assertEq(iter.closed, false);

    iterTake.next().then(
      ({done, value}) => {
        assertEq(done, true);
        assertEq(value, undefined);
        assertEq(iter.closed, true);
      }
    );
  }
);

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
