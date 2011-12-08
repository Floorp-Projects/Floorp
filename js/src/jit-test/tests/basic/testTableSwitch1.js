function testTableSwitch1() {
    var x = 'miss';
    var i, j = 0;
    for (i = 0; i < 19; i++) {
        switch (x) {
          case 1: case 2: case 3: case 4: case 5: throw "FAIL";
          default: j++;
        }
    }
    assertEq(i, j);
}

testTableSwitch1();
