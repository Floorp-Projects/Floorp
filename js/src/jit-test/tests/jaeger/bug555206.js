// |jit-test| error: TypeError
__defineGetter__("x", function() { return /a/.exec(undefined); } );
" ".replace(/\s/,"");
x.b
