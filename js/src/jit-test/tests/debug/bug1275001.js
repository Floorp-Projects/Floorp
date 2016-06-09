
g = newGlobal();
g.parent = this;
g.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function(frame) {
        frame.older
    }
} + ")()")
function check_one(expected, f, err) {
    try {
        f()
    } catch (ex) {
        s = ex.toString()
        assertEq(s.slice(11, -err.length), expected)
    }
}
ieval = eval
function check(expr, expected = expr) {
    var end, err
    for ([end, err] of[[".random_prop", " is undefined" ]]) 
         statement = "o = {};" + expr + end;
         cases = [
            function() ieval("var undef;" + statement),
            Function(statement)
        ]
        for (f of cases) 
            check_one(expected, f, err)
}
check("undef");
check("o.b");
