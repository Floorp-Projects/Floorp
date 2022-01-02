function g(code) {
    eval("(function(){" + code + "})")()
}
g("evalcx(\"(/a/g,function(){})\")");
g("'a'.replace(/a/g,gc)")
