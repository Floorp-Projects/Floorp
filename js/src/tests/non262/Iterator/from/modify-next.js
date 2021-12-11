// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const iter = {
  next: () => ({ done: false, value: 0 }),
};

const wrap = Iterator.from(iter);

iter.next = () => ({ done: true, value: undefined });

let {done, value} = wrap.next();
assertEq(done, false);
assertEq(value, 0);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
