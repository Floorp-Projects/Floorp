function bind(f) {
    return f.call.apply(f.bind, arguments);
};
function h(a, b) {
    a.x;
}
function g(a, b) {
    a.x = b;
    h(a);
}
function f() {
    for(var i=0; i<20; i++) {
        g.call(this, {}, bind(function(){}));
    }
}
f();
