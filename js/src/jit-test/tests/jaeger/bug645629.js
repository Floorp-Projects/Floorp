/* Don't assert. */
o1 = {};
o1 = 2;
function f(o) {
    o.hasOwnProperty("x");
}
new f(o1);
f(o1);
