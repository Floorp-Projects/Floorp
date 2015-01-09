function f() {
    let(x) yield x;
}
var g = f();
g.next();
g.close();
