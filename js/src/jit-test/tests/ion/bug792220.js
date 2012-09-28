var p = Proxy.create({
    has : function(id) {}
});
Object.prototype.__proto__ = p;
function f() {
        if (/a/.exec("a"))
            (null).default++;
}
delete RegExp.prototype.test;
f();
