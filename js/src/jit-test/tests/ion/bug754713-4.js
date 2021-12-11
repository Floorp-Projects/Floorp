function f(x) {
    var y = (x < 0) ? 1 : 2;
    Math.floor(0); // bailout
}
Math.floor(0);
f(1);
