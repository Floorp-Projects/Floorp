// |jit-test| --fast-warmup

function foo(o) {
  return o.x;
}

with ({}) {}

var trigger = false;

var handler = {
  get: (target, prop) => {
    if (trigger) {
      transplant(newGlobal({newCompartment: true}));
    }
    return 1;
  },
  getOwnPropertyDescriptor: (target, prop) => {
    return Object.getOwnPropertyDescriptor(target, prop);
  }
}

let {object, transplant} = transplantableObject();
Object.defineProperty(object, 'x', { value: 1, configurable: false, writable: false });

var proxy = new Proxy(object, handler);
for (var i = 0; i < 50; i++) {
  foo(proxy);
}

trigger = true;
foo(proxy);
