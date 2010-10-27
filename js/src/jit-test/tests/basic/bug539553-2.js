function f() {
    var x = arguments;
    arguments.length = {};
    for (var i = 0; i < 9; i++)
        x.length.toSource();
}
f();
