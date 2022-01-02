// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const iter = {
  next: () => ({ done: false, value: 0 }),
  return: (value) => ({ done: true, value }),
};

const wrap = Iterator.from(iter);

let {done, value} = wrap.return(1);
assertEq(done, true);
assertEq(value, 1);

iter.return = () => { throw new Error(); };
assertThrowsInstanceOf(wrap.return, Error);

iter.return = null;
let nullResult = wrap.return(2);
assertEq(nullResult.done, true);
assertEq(nullResult.value, 2);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
