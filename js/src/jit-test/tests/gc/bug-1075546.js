for (var i = 0; i < 200; ++i) {
    Object.getOwnPropertyNames(undefined + "");
}
function p(s) {
    for (var i = 0; i < s.length; i++) {
        s.charCodeAt(i);
    }
}
function m(f) {
    var a = [];
    for (var j = 0; j < 700; ++j) {
        try {
            f()
        } catch (e) {
            a.push(e.toString());
        }
    }
    p(uneval(a));
}
f = Function("\
    function f() {\
        functionf\n{}\
    }\
    m(f);\
");
f();
f();
for (var x = 0; x < 99; x++) {
    newGlobal()
}
