// Test that Array.prototype.join doesn't call the [[HasProperty]] internal
// method of objects.

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

result = Array.prototype.join.call(proxy);
assertEqArray(log, [ "get:length" ]);
assertEq(result, "");

log.length = 0;
array.push(1);

result = Array.prototype.join.call(proxy);
assertEqArray(log, [ "get:length", "get:0" ]);
assertEq(result, "1");

log.length = 0;
array.push(2);

result = Array.prototype.join.call(proxy);
assertEqArray(log, [ "get:length", "get:0", "get:1" ]);
assertEq(result, "1,2");

if (typeof reportCompare === "function")
  reportCompare(true, true);
