//|jit-test| error: SyntaxError

(function() {
    x = "arguments";
    f = eval("(function(){return(c for (x in eval('print(arguments[0])')))})");
    for (e in f()) {
        (function() {});
    }
})(3)
