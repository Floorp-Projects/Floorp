var a;
try {
    a();
} catch(e) {
    assertEq(e instanceof TypeError, true);
}
