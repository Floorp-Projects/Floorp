// |reftest| shell-option(--enable-array-grouping) skip-if(release_or_beta)

var BUGNUMBER = 1739648;
var summary = "Implement Array.prototype.groupToMap || group";

print(BUGNUMBER + ": " + summary);

function isNeg(x) {
  if (Object.is(x, -0) || x < 0) {
    return true;
  }
  return false;
}

{
  const a1 = [-Infinity, -2, -1, -0, 0, 1, 2, Infinity];
  const expectedObj = { neg: [-Infinity, -2, -1, -0], pos: [0, 1, 2, Infinity] };
  Object.setPrototypeOf(expectedObj, null);

  const groupedArray = a1.group(x => isNeg(x) ? 'neg' : 'pos');
  const mappedArray = a1.groupToMap(x => isNeg(x) ? 'neg' : 'pos');

  assertEq(Object.getPrototypeOf(groupedArray), null)
  assertDeepEq(groupedArray, expectedObj);
  assertDeepEq(mappedArray.get("neg"), expectedObj["neg"]);
  assertDeepEq(mappedArray.get("pos"), expectedObj["pos"]);


  const expectedObj2 = {"undefined": [1,2,3]}
  Object.setPrototypeOf(expectedObj2, null);
  assertDeepEq([1,2,3].group(() => {}), expectedObj2);
  assertDeepEq([].group(() => {}), Object.create(null));
  assertDeepEq(([1,2,3].groupToMap(() => {})).get(undefined), [1,2,3]);
  assertEq(([1,2,3].groupToMap(() => {})).size, 1);

  const negMappedArray = a1.groupToMap(x => isNeg(x) ? -0 : 0);
  assertDeepEq(negMappedArray.get(0), a1);
  assertDeepEq(negMappedArray.size, 1);

  assertThrowsInstanceOf(() => [].group(undefined), TypeError);
  assertThrowsInstanceOf(() => [].group(null), TypeError);
  assertThrowsInstanceOf(() => [].group(0), TypeError);
  assertThrowsInstanceOf(() => [].group(""), TypeError);
  assertThrowsInstanceOf(() => [].groupToMap(undefined), TypeError);
  assertThrowsInstanceOf(() => [].groupToMap(null), TypeError);
  assertThrowsInstanceOf(() => [].groupToMap(0), TypeError);
  assertThrowsInstanceOf(() => [].groupToMap(""), TypeError);
}

const array = [ 'test' ];
Object.defineProperty(Map.prototype, 4, {
  get() {
    throw new Error('monkey-patched Map get call');
  },
  set(v) {
    throw new Error('monkey-patched Map set call');
  },
  has(v) {
    throw new Error('monkey-patched Map has call');
  }
});

const map1 = array.groupToMap(key => key.length);

assertEq('test', map1.get(4)[0])

Object.defineProperty(Array.prototype, '4', {
  set(v) {
    throw new Error('user observable array set');
  },
  get() {
    throw new Error('user observable array get');
  }
});

const map2 = array.groupToMap(key => key.length);
const arr = array.group(key => key.length);

assertEq('test', map2.get(4)[0])
assertEq('test', arr[4][0])

Object.defineProperty(Object.prototype, "foo", {
  get() { throw new Error("user observable object get"); },
  set(v) { throw new Error("user observable object set"); }
});
[1, 2, 3].group(() => 'foo');

// Ensure property key is correctly accessed
count = 0;
p = [1].group(() => ({ toString() { count++; return 10 } }));
assertEq(count, 1);
[1].groupToMap(() => ({ toString() { count++; return 10 } }));
assertEq(count, 1);

reportCompare(0, 0);
