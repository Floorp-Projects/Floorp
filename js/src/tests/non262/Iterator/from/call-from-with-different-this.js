// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const iter = {
  next() {
    assertEq(arguments.length, 0);
    return {done: false, value: 0};
  },
};
const wrap = Iterator.from.call(undefined, iter);

const result = wrap.next("next argument is ignored");
assertEq(result.done, false);
assertEq(result.value, 0);

const returnResult = wrap.return("return argument is ignored");
assertEq(returnResult.done, true);
assertEq(returnResult.value, undefined);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
