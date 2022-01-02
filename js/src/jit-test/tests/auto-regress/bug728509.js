// Binary: cache/js-dbg-32-78fde7e54d92-linux
// Flags: -m -n -a
//
function g(code) {
    try {
        f = eval("(function(){" + code + "})")
    } catch (r) {}
    f()
    try {
        evalcx("(function(){return" + code + "})()")
    } catch (e) {}
}
g("")
g(" function(x,[]){NaN.x::c}()")
