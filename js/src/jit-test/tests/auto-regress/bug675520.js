// Binary: cache/js-dbg-64-6ca8580eb84f-linux
// Flags:
//
var handler = {iterate: function() { return Iterator.prototype; }};
var proxy = Proxy.create(handler);
for (var p in proxy) { }
