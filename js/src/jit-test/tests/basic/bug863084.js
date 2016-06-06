// |jit-test| error: too much recursion
function f(code) {
    try {
        eval(code)
    } catch (e) {}
    eval(code)
}
f("\
    z=evalcx('');\
    gc();\
    z.toString=(function(){\
        v=evaluate(\"(Math.atan2(\\\"\\\",this))\",{\
            global:z,catchTermination:this\
        });\
    });\
    String(z)\
")
