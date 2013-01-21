// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-9174e1bbfa9f-linux
// Flags: -j
//
'a'.replace(/a/g, [].push);
x = [];
x++;
(function(){ eval("for(let y in []) [][x]"); })();
