// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const iter = {
  next: () => ({done: false, value: 0}),
};
const wrap = Iterator.from.call(undefined, iter);

const result = wrap.next();
assertEq(result.done, false);
assertEq(result.value, 0);

const returnResult = wrap.return(1);
assertEq(returnResult.done, true);
assertEq(returnResult.value, 1);

assertThrowsInstanceOf(() => wrap.throw(new Error()), Error);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
