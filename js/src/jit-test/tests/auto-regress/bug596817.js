// Binary: cache/js-dbg-32-a409054e1395-linux
// Flags: -m
//
load(libdir + 'asserts.js');
// value is not iterable
assertThrowsInstanceOf(function(){for(var[x]=x>>x in[[]<[]]){[]}}, TypeError);
