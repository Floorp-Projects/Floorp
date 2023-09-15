// |jit-test| --fast-warmup

function foo(o) {
  return o.x;
}

with ({}) {}

var handler = {
  get: (target, prop) => { return 1; },
  getOwnPropertyDescriptor: (target, prop) => { return Object.getOwnPropertyDescriptor(target, prop); }
}

var o = {};
Object.defineProperty(o, 'x', { value: 1, configurable: false, writable: false });

var proxy = new Proxy(o, handler);
for (var i = 0; i < 50; i++) {
  foo(proxy);
}

var proxy_proxy = new Proxy(proxy, handler);
foo(proxy_proxy);
