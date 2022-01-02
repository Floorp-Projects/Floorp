// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

async function* gen(value) {
  yield value;
}
const fn = x => x;
function check(value, expected) {
  gen(value).find(fn).then(result => assertEq(result, expected));
}

check(true, true);
check(1, 1);
check('test', 'test');

check(false, undefined);
check(0, undefined);
check('', undefined);
check(null, undefined);
check(undefined, undefined);
check(NaN, undefined);
check(-0, undefined);
check(0n, undefined);
check(Promise.resolve(false), undefined);

let array = [];
check(array, array);

let object = {};
check(object, object);

const htmlDDA = createIsHTMLDDA();
check(htmlDDA, undefined);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
