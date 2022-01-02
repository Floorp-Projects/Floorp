function test(x) {
    switch(x) {
        case 0:
          return 0;
        default:
          return -1;
    }
}

for(var i=0; i<100; i++) {
    assertEq(test(-0), 0);
}
