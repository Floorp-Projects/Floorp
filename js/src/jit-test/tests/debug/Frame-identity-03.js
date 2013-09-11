// |jit-test| debug
// Test that we create new Debugger.Frames and reuse old ones correctly with recursion.

var g = newGlobal();
g.debuggeeGlobal = this;
g.eval("(" + function () {
        function id(f) {
            return ("id" in f) ? f.id : (f.id = nextid++);
        }

        var dbg = new Debugger(debuggeeGlobal);
        dbg.onDebuggerStatement = function (frame) {
            var a = [];
            for (; frame; frame = frame.older)
                a.push(frame);
            var s = '';
            while (a.length)
                s += id(a.pop());
            results.push(s);
        };
    } + ")();");

function cons(a, b) {
    debugger;
    return [a, b];
}

function tree(n) {
    if (n < 2)
        return n;
    return cons(tree(n - 1), tree(n - 2));
}

g.eval("results = []; nextid = 0;");
debugger;
assertEq(g.results.join(","), "0");
assertEq(g.nextid, 1);

g.eval("results = [];");
tree(2);
assertEq(g.results.join(","), "012"); // 0=global, 1=tree, 2=cons

g.eval("results = []; nextid = 1;");
tree(3);
assertEq(g.results.join(","), "0123,014"); //0=global, 1=tree(3), 2=tree(2), 3=cons, 4=cons

g.eval("results = []; nextid = 1;");
tree(4);
// 0=global, 1=tree(4), 2=tree(3), 3=tree(2), 4=cons, tree(1), 5=cons, 6=tree(2), 7=cons, 8=cons
assertEq(g.results.join(","), "01234,0125,0167,018");
