// Calls Array.prototype.sort and tests that properties are deleted in the same order in the
// native and the self-hosted implementation.

function createProxy() {
  var deleted = [];
  var proxy = new Proxy([, , 0], {
    deleteProperty(t, pk){
      deleted.push(pk);
      return delete t[pk];
    }
  });

  return {proxy, deleted};
}

function compareFn(a, b) {
  return a < b ? -1 : a > b ? 1 : 0;
}

// Sort an array without a comparator function. This calls the native sort implementation.

var {proxy, deleted} = createProxy();

assertEqArray(deleted, []);
proxy.sort()
assertEqArray(deleted, ["1", "2"]);

// Now sort an array with a comparator function. This calls the self-hosted sort implementation.

var {proxy, deleted} = createProxy();

assertEqArray(deleted, []);
proxy.sort(compareFn);
assertEqArray(deleted, ["1", "2"]);

if (typeof reportCompare === "function")
  reportCompare(true, true);
