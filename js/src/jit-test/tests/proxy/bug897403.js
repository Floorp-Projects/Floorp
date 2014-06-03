// |jit-test| error: TypeError

var f = (function () {}).bind({});
var p = new Proxy(f, {});
Object.defineProperty(p, "caller", {get: function(){}});
