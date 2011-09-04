var x = Proxy.create({ fix: function() { return []; } });
Object.__proto__ = x;
Object.freeze(x);
quit();
