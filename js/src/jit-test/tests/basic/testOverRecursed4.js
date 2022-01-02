function tryItOut(code) {
    f = eval("(function(){" + code + "})")
    try {
        f()
    } catch (e) {}
}
tryItOut("x=7");
tryItOut("\"use strict\";for(d in[x=arguments]){}");
tryItOut("for(v in((Object.seal)(x)));x.length=Function")
