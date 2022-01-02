// |jit-test| error: TypeError

var glob = this;
var arr = [];
Object.defineProperty(arr, 0, {
  get: (function() {
    glob.__proto__;
  })
});
try {
  arr.pop();
} catch (e) {}
arr.pop();
