let g = newGlobal({alwaysUseFdlibm: true});

// Adapted from https://github.com/arkenfox/TZP/blob/master/tests/math.html
// Tests all values that differed from libm for me on Linux and Windows.

function test() {
  for (var i = 0; i < 100; i++) {
    assertEq(g.Math.cos(1e130          ), -0.767224894221913);
    assertEq(g.Math.cos(1e272          ), -0.7415825695514536);
    assertEq(g.Math.cos(1e284          ),  0.7086865671674247);
    assertEq(g.Math.cos(1e75           ), -0.7482651726250321);
    assertEq(g.Math.cos(57*Math.E      ), -0.536911695749024);
    assertEq(g.Math.cos(21*Math.LN2    ), -0.4067775970251724);
    assertEq(g.Math.cos(51*Math.LN2    ), -0.7017203400855446);
    assertEq(g.Math.cos(21*Math.SQRT1_2), -0.6534063185820198);
    assertEq(g.Math.cos(17*Math.LOG10E ),  0.4537557425982784);
    assertEq(g.Math.cos(2*Math.LOG10E  ),  0.6459044007438142);

    assertEq(g.Math.sin(35*Math.LN2    ), -0.765996413898051);
    assertEq(g.Math.sin(110*Math.LOG2E ),  0.9989410140273757);
    assertEq(g.Math.sin(7*Math.LOG10E  ),  0.10135692924965616);

    assertEq(g.Math.tan(1e140          ),  0.7879079967710036);
    assertEq(g.Math.tan(6*Math.E       ),  0.6866761546452431);
    assertEq(g.Math.tan(6*Math.LN2     ),  1.6182817135715877);
    assertEq(g.Math.tan(10*Math.LOG2E  ), -3.3537128705376014);
    assertEq(g.Math.tan(17*Math.SQRT2  ), -1.9222955461799982);
    assertEq(g.Math.tan(34*Math.SQRT1_2), -1.9222955461799982);
    assertEq(g.Math.tan(10*Math.LOG10E ),  2.5824856130712432);
  }
}

/*
cos
   3:  1e130              :     -0.767224894221913 ,
   4:  1e272              :     -0.7415825695514536,
   6:  1e284              :      0.7086865671674247,
   7:  1e75               :     -0.7482651726250321,
  13:  57*Math.E          :     -0.536911695749024,
  14:  21*Math.LN2        :     -0.4067775970251724,
  15:  51*Math.LN2        :     -0.7017203400855446,
  19:  21*Math.SQRT1_2    :     -0.6534063185820198,
  20:  17*Math.LOG10E     :      0.4537557425982784,
  21:  2*Math.LOG10E      :      0.6459044007438142,


sin
  11:          :   -0.765996413898051,
  12:       :      0.9989410140273757,
  13:        :     0.10135692924965616,

tan
  1:  1e140              :      0.7879079967710036,
  10:  6*Math.E           :      0.6866761546452431,
  11:  6*Math.LN2         :      1.6182817135715877,
  12:  10*Math.LOG2E      :     -3.3537128705376014,
  13:  17*Math.SQRT2      :     -1.9222955461799982,
  14:  34*Math.SQRT1_2    :     -1.9222955461799982,
  15:  10*Math.LOG10E     :      2.5824856130712432,
*/

test();

// Test if the test is still useful, or if all results match without
// fingerprinting resistance as well.
if (!getBuildConfiguration("android") &&
    Math.cos(1e130          ) == -0.767224894221913 &&
    Math.cos(1e272          ) == -0.7415825695514536 &&
    Math.cos(1e284          ) ==  0.7086865671674247 &&
    Math.cos(1e75           ) == -0.7482651726250321 &&
    Math.cos(57*Math.E      ) == -0.536911695749024 &&
    Math.cos(21*Math.LN2    ) == -0.4067775970251724 &&
    Math.cos(51*Math.LN2    ) == -0.7017203400855446 &&
    Math.cos(21*Math.SQRT1_2) == -0.6534063185820198 &&
    Math.cos(17*Math.LOG10E ) ==  0.4537557425982784 &&
    Math.cos(2*Math.LOG10E  ) ==  0.6459044007438142 &&
    Math.sin(35*Math.LN2    ) == -0.765996413898051 &&
    Math.sin(110*Math.LOG2E ) ==  0.9989410140273757 &&
    Math.sin(7*Math.LOG10E  ) ==  0.10135692924965616 &&
    Math.tan(1e140          ) ==  0.7879079967710036 &&
    Math.tan(6*Math.E       ) ==  0.6866761546452431 &&
    Math.tan(6*Math.LN2     ) ==  1.6182817135715877 &&
    Math.tan(10*Math.LOG2E  ) == -3.3537128705376014 &&
    Math.tan(17*Math.SQRT2  ) == -1.9222955461799982 &&
    Math.tan(34*Math.SQRT1_2) == -1.9222955461799982 &&
    Math.tan(10*Math.LOG10E ) ==  2.5824856130712432) {
  assertEq(false, true);
}
