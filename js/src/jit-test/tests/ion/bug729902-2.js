function f() {
    var i = 0;
  outer:
    for (var x = 0; x < 10; x++) {
        while (true) {
            if (i > 150)
                continue outer;
            i++;
        }
    }
    assertEq(i, 151);
}
f();
