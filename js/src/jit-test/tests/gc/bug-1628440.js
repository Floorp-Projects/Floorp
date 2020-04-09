// |jit-test| error: ReferenceError
gczeal(10, 2);
let cleanup = function(iter) {}
let key = {"k": "this is my key"};
let fg = new FinalizationRegistry(cleanup);
let object = {};
fg.register(object, {}, key);
let success = fg.unregister(key);
throw "x";
