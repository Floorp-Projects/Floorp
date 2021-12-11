// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

async function* gen(value) {
  yield value;
}
const fn = x => x;
function check(value, expected) {
  gen(value).every(fn).then(result => assertEq(result, expected));
}

check(true, true);
check(1, true);
check([], true);
check({}, true);
check('test', true);

check(false, false);
check(0, false);
check('', false);
check(null, false);
check(undefined, false);
check(NaN, false);
check(-0, false);
check(0n, false);
check(createIsHTMLDDA(), false);
check(Promise.resolve(false), false);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
