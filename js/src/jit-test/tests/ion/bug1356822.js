const d = 0;
function f() {
    var m = Math;
    (function () {
        d = m;
    })()
}
for (var i = 0; i < 4; i++) {
    try {
	f();
    } catch (e) {
	continue;
    }
    throw "Fail";
}
