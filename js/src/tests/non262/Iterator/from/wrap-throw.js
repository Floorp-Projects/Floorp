// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
class Iter {
  next() {
    return { done: false, value: 0 };
  }
}

const iter = new Iter();
const wrap = Iterator.from(iter);

assertThrowsInstanceOf(() => wrap.throw(new Error()), Error);
assertThrows(() => wrap.throw());
assertThrows(() => wrap.throw(1));

class IterThrowNull {
  next() {
    return { done: false, value: 0 };
  }
  throw = null;
}

const iterNull = new IterThrowNull();
const wrapNull = Iterator.from(iter);

assertThrowsInstanceOf(() => wrapNull.throw(new Error()), Error);
assertThrows(() => wrapNull.throw());
assertThrows(() => wrapNull.throw(1));

class IterWithThrow {
  next() {
    return { done: false, value: 0 };
  }

  throw(value) {
    return value;
  }
}

const iterWithThrow = new IterWithThrow();
const wrapWithThrow = Iterator.from(iterWithThrow);

assertEq(wrapWithThrow.throw(1), 1);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
