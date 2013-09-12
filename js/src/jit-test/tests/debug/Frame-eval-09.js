// assigning to local variables in frame.eval code

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    frame.eval("outerarg = 1; outervar = 2; innerarg = 3; innervar = 4;");
};

var result = g.eval("(" + function outer(outerarg) {
        var outervar = 200;
        function inner(innerarg) {
            var innervar = 400;
            debugger;
            return innerarg + innervar;
        }
        var innersum = inner(300);
        return outerarg + outervar + innersum;
    } + ")(100)");

assertEq(result, 10);
