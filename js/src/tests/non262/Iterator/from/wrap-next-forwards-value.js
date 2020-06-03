// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
class Iter {
  next(value) {
    this.v = value;
    return { done: false, value };
  }
}

const iter = new Iter();
const wrap = Iterator.from(iter);
assertEq(iter !== wrap, true);

assertEq(iter.v, undefined);
wrap.next(1);
assertEq(iter.v, 1);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
