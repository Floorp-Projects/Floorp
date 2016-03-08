// |jit-test| --ion-eager
function f(b) {
    var a = arguments;
    if (b)
        f(false);
    else
        g = {
            apply:function(x,y) { "use asm"; function g() {} return g }
        };
    g.apply(null, a);
}
f(true);
