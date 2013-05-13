// Binary: cache/js-dbg-32-ebafee0cea36-linux
// Flags: -m -n
//
function tryItOut(code) {
    f = eval("(function(){" + code + "})")
    for (e in f()) {}
}
tryItOut("\
    for each(x in[0,0,0,0,0,0,0]) {\
        function f(b) {\
            Object.defineProperty(b,\"\",({t:f}))\
        }\
        for each(d in[(1),String,String,String,String,(0),String,(1),String]) {\
            try{\
                f(d);\
                yield\
            }catch(e){}\
        }\
    }\
")
