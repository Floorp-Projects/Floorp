// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

let simple1 = #[1, 2];
let simple2 = #[1, 2];
let simpleDiff = #[0, 2];
let simpleDiff2 = #[1];

assertEq(simple1 === simple2, true);
assertEq(simple1 === simpleDiff, false);
assertEq(simple1 === simpleDiff2, false);

let withPositiveZero = #[1, 2, +0];
let withPositiveZero2 = #[1, 2, +0];
let withNegativeZero = #[1, 2, -0];

assertEq(withPositiveZero === withPositiveZero2, true);
assertEq(Object.is(withPositiveZero, withPositiveZero2), true);
assertEq(#[withPositiveZero] === #[withPositiveZero2], true);
assertEq(Object.is(#[withPositiveZero], #[withPositiveZero2]), true);

assertEq(withPositiveZero === withNegativeZero, true);
assertEq(Object.is(withPositiveZero, withNegativeZero), false);
assertEq(#[withPositiveZero] === #[withNegativeZero], true);
assertEq(Object.is(#[withPositiveZero], #[withNegativeZero]), false);

let withNaN = #[1, NaN, 2];
let withNaN2 = #[1, NaN, 2];

assertEq(withNaN === withNaN2, true);
assertEq(Object.is(withNaN, withNaN2), true);

if (typeof reportCompare === "function") reportCompare(0, 0);
