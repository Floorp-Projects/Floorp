// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-64-1cd24ecc343d-linux
// Flags:
//
(function(){
  var c;
  eval("var c; for each(var c in s);");
})()
