// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
class Iter {
  next() {
    if (this.closed)
      return { done: true, value: undefined };
    return { done: false, value: 0 };
  }

  return(value) {
    assertEq(arguments.length, 0);
    this.closed = true;
    return { done: true, value: 42 };
  }
}

const iter = new Iter();
const wrap = Iterator.from(iter);
assertEq(iter.closed, undefined);

let result = wrap.next();
assertEq(result.done, false);
assertEq(result.value, 0);

result = wrap.return(1);
assertEq(result.done, true);
assertEq(result.value, 42);

assertEq(iter.closed, true);
result = wrap.next();
assertEq(result.done, true);
assertEq(result.value, undefined);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
