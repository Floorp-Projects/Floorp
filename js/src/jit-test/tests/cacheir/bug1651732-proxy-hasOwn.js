// An object with a custom [Symbol.toPrimitive] function.
var key = {
  value: "a",

  [Symbol.toPrimitive]() {
    return this.value;
  }
};

var target = {
  a: 0,
  [Symbol.iterator]: 0,
};
var obj = new Proxy(target, {});

for (var i = 0; i < 100; ++i) {
  // Change key[Symbol.toPrimitive] to return a symbol after some warm-up.
  if (i > 80) {
    key.value = Symbol.iterator;
  }

  // Attach an IC for JSOp::HasOwn on proxies.
  assertEq(Object.prototype.hasOwnProperty.call(obj, key), true);
}
