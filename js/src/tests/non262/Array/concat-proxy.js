var BUGNUMBER = 1287520;
var summary = 'Array.prototype.concat should check HasProperty everytime for non-dense array';

print(BUGNUMBER + ": " + summary);

var a = [1, 2, 3];
a.constructor = {
  [Symbol.species]: function(...args) {
    var p = new Proxy(new Array(...args), {
      defineProperty(target, propertyKey, receiver) {
        if (propertyKey === "0") delete a[1];
        return Reflect.defineProperty(target, propertyKey, receiver);
      }
    });
    return p;
  }
};

var p = a.concat();
assertEq(0 in p, true);
assertEq(1 in p, false);
assertEq(2 in p, true);

if (typeof reportCompare === "function")
  reportCompare(true, true);
