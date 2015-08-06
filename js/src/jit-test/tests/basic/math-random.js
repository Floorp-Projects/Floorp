function test() {
  // With this seed, state won't be reset in 10000 iteration.  The result is
  // deterministic and it can be used to check the correctness of
  // implementation.
  setRNGState(0x12341234);

  function f() {
    let x = [];
    for (let i = 0; i < 10000; i++)  {
      x.push(Math.random());
    }
    return x;
  }
  let x = f();
  assertEq(x[0], 0.3562073961260165);
  assertEq(x[10], 0.9777930699941514);
  assertEq(x[100], 0.9146259915430884);
  assertEq(x[1000], 0.315983055288946);
  assertEq(x[2000], 0.7132284805929497);
  assertEq(x[3000], 0.9621073641614717);
  assertEq(x[4000], 0.3928228025111996);
  assertEq(x[5000], 0.555710685962832);
  assertEq(x[6000], 0.5207553912782503);
  assertEq(x[7000], 0.08268413491723015);
  assertEq(x[8000], 0.031796243723989925);
  assertEq(x[9000], 0.900683320457098);
  assertEq(x[9999], 0.7750389203054577);

  // With this seed, state will be reset before calculating high bits.
  // The result is nondeterministic, but it should be in [0, 1) range.
  setRNGState(0);
  x = f();
  assertEq(x[0] >= 0 && x[0] < 1, true);

  // With this seed, high bits will be 0 and state will be reset before
  // calculating low bits.  The result is also nondeterministic, but it should
  // be in [0, 1 / (1 << 26)) range.
  setRNGState(0x615c0e462aa9);
  x = f();
  assertEq(x[0] >= 0 && x[0] < 1 / (1 << 26), true);
}

if (typeof setRNGState == "function")
  test();
