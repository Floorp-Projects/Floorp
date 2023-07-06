// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
class Iter {
  next(value) {
    assertEq(arguments.length, 0);
    return { done: false, value };
  }
}

const iter = new Iter();
const wrap = Iterator.from(iter);
assertEq(iter !== wrap, true);

assertEq(iter.v, undefined);
wrap.next(1);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
