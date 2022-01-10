var BUGNUMBER = 1739648;
var summary = "Implement Array.prototype.groupByToMap || groupBy";

print(BUGNUMBER + ": " + summary);

const array = [ 'test' ];
Object.defineProperty(Map.prototype, 4, {
  get() {
    throw new Error('monkey-patched get call');
  }
});

const map1 = array.groupByToMap(key => key.length);

assertEq('test', map1.get(4)[0], "Check for override of get property Map.prototype")



const array = [ 'test' ];
Object.defineProperty(Array.prototype, '4', {
  set(v) {
    throw new Error('user observable array set');
  }
});

const map2 = array.groupByToMap(key => key.ength);

const arr = array.groupBy(key=>key.length);

assertEq('test', map2.get(4)[0], "Check for override of set property Array.prototype")
assertEq('test', arr[4][0], "Check for override of set property Array.prototype")

reportCompare(0, 0);
