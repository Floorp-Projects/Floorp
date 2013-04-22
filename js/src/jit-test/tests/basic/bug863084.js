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
            global:z,newContext:7,catchTermination:this\
        });\
    });\
    String(z)\
")
