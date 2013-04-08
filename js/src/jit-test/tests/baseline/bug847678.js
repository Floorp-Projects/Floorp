// |jit-test| error: TypeError
s = newGlobal();
function g(c) {
    evalcx(c, s)
}
g("[eval]=(function(){})")
g("while(eval());")
