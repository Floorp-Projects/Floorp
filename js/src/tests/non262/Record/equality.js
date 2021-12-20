// |reftest| skip-if(!this.hasOwnProperty("Record"))

assertEq(#{} === #{}, true);
assertEq(#{} === #{ x: 1 }, false);
assertEq(#{} === #{ x: undefined }, false);
assertEq(#{ x: 1 } === #{ x: 1 }, true);
assertEq(#{ x: 2 } === #{ x: 1 }, false);
assertEq(#{ y: 1 } === #{ x: 1 }, false);
assertEq(#{ x: 1, y: 2 } === #{ y: 2, x: 1 }, true);
assertEq(#{ x: 1, y: 2 } === #{ y: 1, x: 2 }, false);

let withPositiveZero = #{ x: +0 };
let withNegativeZero = #{ x: -0 };

assertEq(withPositiveZero === withNegativeZero, true);
assertEq(#[withPositiveZero] === #[withNegativeZero], true);
assertEq(Object.is(withPositiveZero, withNegativeZero), false);
assertEq(Object.is(#[withPositiveZero], #[withNegativeZero]), false);

assertEq(#{ x: NaN } === #{ x: NaN }, true);
assertEq(Object.is(#{ x: NaN }, #{ x: NaN }), true);

if (typeof reportCompare === "function") reportCompare(0, 0);
