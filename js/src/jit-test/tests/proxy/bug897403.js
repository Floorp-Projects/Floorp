var f = (function () {}).bind({});
var p = new Proxy(f, {});
Object.defineProperty(p, "caller", {get: function(){}});
