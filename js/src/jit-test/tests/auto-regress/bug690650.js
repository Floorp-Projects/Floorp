// |jit-test| need-for-each

// Binary: cache/js-dbg-32-44ef245b8706-linux
// Flags: -m -n -a
//
[].__proto__.__defineGetter__("", function() {});
var tryCatch = " try{}catch(e){}";
for (var i = 0; i < 13; ++i) {
  tryCatch += tryCatch;
}
eval("\
    (function() {\
        for each(c in[0,0]){\
" + tryCatch + "\
            print\
        }\
    })\
")()
