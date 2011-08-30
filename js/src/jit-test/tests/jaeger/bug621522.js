
/* Don't crash. */
function f() {
    var x;
    x.a;
    x = {};
}

try {
    f();
    assertEq(0, 1);
} catch(e) {

}
