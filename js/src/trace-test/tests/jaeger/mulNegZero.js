
function mul(x, y)    { return x * y;  };
function mulConst0(x) { return x * 0;  };
function mulConst1(x) { return -5 * x; };
function mulConst2(x) { return x * -5; };

function f() {
    assertEq(mulConst0(7), 0);
    assertEq(mulConst0(-5), -0);
    assertEq(mulConst0(0), 0);
    assertEq(mulConst0(-0), -0);
    
    assertEq(mulConst1(7), -35);
    assertEq(mulConst1(-8), 40);
    assertEq(mulConst1(0), -0);
    assertEq(mulConst1(-0), 0);
    
    assertEq(mulConst2(7), -35);
    assertEq(mulConst2(-8), 40);
    assertEq(mulConst2(0), -0);
    assertEq(mulConst2(-0), 0);
    
    assertEq(mul(55, 2), 110);
    assertEq(mul(0, -10), -0);
    assertEq(mul(-5, 0), -0);
    assertEq(mul(-0, 0), -0);
    assertEq(mul(0, -0), -0);
    assertEq(mul(0, 0), 0);
    assertEq(mul(-0, -0), 0);
}
f();
