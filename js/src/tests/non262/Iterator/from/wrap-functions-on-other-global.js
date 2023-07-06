// |reftest| skip-if(!this.hasOwnProperty('Iterator'))

class TestError extends Error {}

function checkIterResult({done, value}, expectedDone, expectedValue) {
  assertEq(done, expectedDone);
  assertEq(value, expectedValue);
}

const iter = {
  next(value) {
    return {done: false, value: arguments.length};
  },
  return() {
    throw new TestError();
  },
  throw: (value) => ({done: true, value}),
};
const thisWrap = Iterator.from(iter);
const otherGlobal = newGlobal({newCompartment: true});
const otherWrap = otherGlobal.Iterator.from(iter);

checkIterResult(thisWrap.next.call(otherWrap), false, 0);
checkIterResult(thisWrap.next.call(otherWrap, 'value'), false, 0);

assertThrowsInstanceOf(thisWrap.return.bind(otherWrap), TestError);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
