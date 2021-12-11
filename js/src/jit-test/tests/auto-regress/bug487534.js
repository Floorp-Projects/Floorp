// Binary: cache/js-dbg-32-96746395df4f-linux
// Flags: -j
//
var fs = { x: /a/, y: /a/, z: /a/ };
for (var p in fs) { this[fs[p]] = null; }
