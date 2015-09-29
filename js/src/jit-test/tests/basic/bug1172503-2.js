load(libdir + "immutable-prototype.js");

var n = 0;
if (globalPrototypeChainIsMutable()) {
  this.__proto__ = new Proxy({}, {
      has: function () {
          if (++n === 2)
              return false;
          a = 0;
      }
  });
  }
a = 0;
assertEq(a, 0);
