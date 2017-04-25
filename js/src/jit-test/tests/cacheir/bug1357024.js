function f() {
    var o = {};
    for (var j = 0; j < 15; j++) {
        try {
            o.__proto__ = o || j;
        } catch(e) {
            continue;
        }
        throw "Fail";
    }
}
f();
