function f(o, n) {
    if (n) {
        o[n] = true;
    } else {
        o.x = true;
    }
}

// Warm up object so HadElementsAccess check will trip next
var o = {};
for (var i = 0; i < 43; ++i) {
    o["x" + i] = true;
}

f(o, "y");
