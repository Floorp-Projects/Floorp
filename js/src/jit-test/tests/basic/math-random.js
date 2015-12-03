function test() {
  setRNGState(0x12341234, 0x98765);

  function f() {
    let x = [];
    for (let i = 0; i < 10000; i++)  {
      x.push(Math.random());
    }
    return x;
  }
  let x = f();
  assertEq(x[0], 0.28443027522441433);
  assertEq(x[10], 0.5283908544644392);
  assertEq(x[100], 0.5593668121538891);
  assertEq(x[1000], 0.7008807796441313);
  assertEq(x[2000], 0.11737403776989574);
  assertEq(x[3000], 0.08573924080320472);
  assertEq(x[4000], 0.22428965439295678);
  assertEq(x[5000], 0.4657521920883555);
  assertEq(x[6000], 0.11816220100329233);
  assertEq(x[7000], 0.6306689010335697);
  assertEq(x[8000], 0.8654862148946609);
  assertEq(x[9000], 0.31734259460387015);
  assertEq(x[9999], 0.013959566914027777);

  // Test some other (arbitrary) seeds.
  setRNGState(0, 1);
  x = f();
  assertEq(x[0], 2.220446049250313e-16);
  assertEq(x[2000], 0.8259328082050756);
  assertEq(x[6000], 0.01060492365550314);
  assertEq(x[9999], 0.7402370773147143);

  setRNGState(0x0fff0101, 0x44440001);
  x = f();
  assertEq(x[0], 0.24994062119568194);
  assertEq(x[2000], 0.4375430448883283);
  assertEq(x[6000], 0.7298689950209452);
  assertEq(x[9999], 0.13284280897626954);
}

if (typeof setRNGState == "function")
  test();
