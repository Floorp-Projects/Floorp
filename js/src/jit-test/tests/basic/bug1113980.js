var p = new Proxy({}, {
    getOwnPropertyDescriptor: function() {
        return {value: 1, configurable: true, writable: true};
    },
    defineProperty: function() {
    }
}, null);

var o = Object.create(p);
o.a = 1;
