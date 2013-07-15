// |jit-test| error: SyntaxError
s = newGlobal();
function g(c) {
    evalcx(c, s)
}
g("[eval]=(function(){})")
g("while(eval());")
