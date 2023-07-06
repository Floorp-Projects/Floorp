// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const iter = {
  next: () => ({ done: false, value: 0 }),
  return: (value = "old return") => ({ done: true, value }),
};

const wrap = Iterator.from(iter);

let {done, value} = wrap.return("return argument ignored");
assertEq(done, true);
assertEq(value, "old return");

iter.return = () => { throw new Error(); };
assertThrowsInstanceOf(wrap.return, Error);

iter.return = null;
let nullResult = wrap.return("return argument ignored");
assertEq(nullResult.done, true);
assertEq(nullResult.value, undefined);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
