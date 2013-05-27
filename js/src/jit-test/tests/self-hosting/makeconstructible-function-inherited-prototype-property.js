var proxy = new Proxy({ get: function() { throw 42; } }, {});
Function.prototype.__proto__ = proxy;
this.hasOwnProperty("Intl");
