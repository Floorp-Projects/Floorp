// An object with a custom [Symbol.toPrimitive] function.
var key = {
  value: "a",

  [Symbol.toPrimitive]() {
    return this.value;
  }
};

var target = {};
var obj = new Proxy(target, {});

for (var i = 0; i < 100; ++i) {
  // Change key[Symbol.toPrimitive] to return a symbol after some warm-up.
  if (i > 80) {
    key.value = Symbol.iterator;
  }

  target[key.value] = i;

  // Attach an IC for JSOp::GetElem on proxies.
  assertEq(obj[key], i);
}
