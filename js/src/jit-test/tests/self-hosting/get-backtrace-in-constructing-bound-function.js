function t() {
    getBacktrace({ locals: true });
}
var f = t.bind();
new f();
f();
