// Test that Array.prototype.pop doesn't call the [[HasProperty]] internal
// method of objects when retrieving the element at the last index.

var log = [];
var array = [];
var proxy = new Proxy(array, new Proxy({}, {
    get(t, trap, r) {
      return (t, pk, ...more) => {
        log.push(`${trap}:${String(pk)}`);
        return Reflect[trap](t, pk, ...more);
      };
    }
}));

var result;

result = Array.prototype.pop.call(proxy);
assertEqArray(log, [
  "get:length",
  "set:length", "getOwnPropertyDescriptor:length", "defineProperty:length"
]);
assertEq(result, undefined);

log.length = 0;
array.push(1);

result = Array.prototype.pop.call(proxy);
assertEqArray(log, [
  "get:length",
  "get:0", "deleteProperty:0",
  "set:length", "getOwnPropertyDescriptor:length", "defineProperty:length"
]);
assertEq(result, 1);

log.length = 0;
array.push(2, 3);

result = Array.prototype.pop.call(proxy);
assertEqArray(log, [
  "get:length",
  "get:1", "deleteProperty:1",
  "set:length", "getOwnPropertyDescriptor:length", "defineProperty:length"
]);
assertEq(result, 3);

log.length = 0;
array.push(4, 5);

result = Array.prototype.pop.call(proxy);
assertEqArray(log, [
  "get:length",
  "get:2", "deleteProperty:2",
  "set:length", "getOwnPropertyDescriptor:length", "defineProperty:length"
]);
assertEq(result, 5);

if (typeof reportCompare === "function")
  reportCompare(true, true);
