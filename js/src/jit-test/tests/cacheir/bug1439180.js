h = function f(x) {
        x = +"NaN";
        return /I/ (~x);
    }
for (var j = 0; j < 3; j++) {
    try {
        h();
    } catch (e) {}
}
