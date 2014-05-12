
function f(a, b, c, d) { }

function topLevel() {
    for (var i = 0; i < 10000; i++) {
        var unused = {};
        var a = {};
        var b = {};
        var c = {};
        var d = {};
        f(a, b, c, d);
    }
}

topLevel();